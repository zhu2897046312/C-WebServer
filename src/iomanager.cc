#include "iomanager.h"
#include "macro.h"
#include "log.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <string.h>
#include <unistd.h>

namespace fst {

static fst::Logger::ptr g_logger = FANSHUTOU_LOG_NAME("system");

enum EpollCtlOp {
};

static std::ostream& operator<< (std::ostream& os, const EpollCtlOp& op) {
    switch((int)op) {
#define XX(ctl) \
        case ctl: \
            return os << #ctl;
        XX(EPOLL_CTL_ADD);
        XX(EPOLL_CTL_MOD);
        XX(EPOLL_CTL_DEL);
        default:
            return os << (int)op;
    }
#undef XX
}

static std::ostream& operator<< (std::ostream& os, EPOLL_EVENTS events) {
    if(!events) {
        return os << "0";
    }
    bool first = true;
#define XX(E) \
    if(events & E) { \
        if(!first) { \
            os << "|"; \
        } \
        os << #E; \
        first = false; \
    }
    XX(EPOLLIN);
    XX(EPOLLPRI);
    XX(EPOLLOUT);
    XX(EPOLLRDNORM);
    XX(EPOLLRDBAND);
    XX(EPOLLWRNORM);
    XX(EPOLLWRBAND);
    XX(EPOLLMSG);
    XX(EPOLLERR);
    XX(EPOLLHUP);
    XX(EPOLLRDHUP);
    XX(EPOLLONESHOT);
    XX(EPOLLET);
#undef XX
    return os;
}

IOManager::FdContext::EventContext& IOManager::FdContext::getContext(IOManager::Event event) {
    switch(event) {
        case IOManager::READ:
            return read;
        case IOManager::WRITE:
            return write;
        default:
            FANSHUTOU_ASSERT2(false, "getContext");
    }
    throw std::invalid_argument("getContext invalid event");
}

void IOManager::FdContext::resetContext(EventContext& ctx) {
    ctx.scheduler = nullptr;
    ctx.fiber.reset();
    ctx.cb = nullptr;
}

void IOManager::FdContext::triggerEvent(IOManager::Event event) {

    FANSHUTOU_ASSERT(events & event);           //事件相同触发断言
    events = (Event)(events & ~event);
    EventContext& ctx = getContext(event);
    if(ctx.cb) {
        ctx.scheduler->schedule(&ctx.cb);
    } else {
        ctx.scheduler->schedule(&ctx.fiber);
    }
    ctx.scheduler = nullptr;
    return;
}

IOManager::IOManager(size_t threads, bool use_caller, const std::string& name)
    :Scheduler(threads, use_caller, name) {
    m_epfd = epoll_create(5000);
    FANSHUTOU_ASSERT(m_epfd > 0);
    //将pipe加入事件监听
    int rt = pipe(m_tickleFds);
    FANSHUTOU_ASSERT(!rt);

    epoll_event event;
    memset(&event, 0, sizeof(epoll_event));
    event.events = EPOLLIN | EPOLLET;       //边缘触发，高性能 ，仅在文件描述符状态发生变化时触发事件，并只通知一次。
    event.data.fd = m_tickleFds[0];

    rt = fcntl(m_tickleFds[0], F_SETFL, O_NONBLOCK);    // O_NONBLOCK 修改为非阻塞模式。
    FANSHUTOU_ASSERT(!rt);

    rt = epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_tickleFds[0], &event);
    FANSHUTOU_ASSERT(!rt);

    contextResize(32);

    start();
}

IOManager::~IOManager() {
    stop();
    close(m_epfd);
    close(m_tickleFds[0]);
    close(m_tickleFds[1]);

    for(size_t i = 0; i < m_fdContexts.size(); ++i) {
        if(m_fdContexts[i]) {
            delete m_fdContexts[i];
        }
    }
}

void IOManager::contextResize(size_t size) {
    m_fdContexts.resize(size);

    for(size_t i = 0; i < m_fdContexts.size(); ++i) {
        if(!m_fdContexts[i]) {
            m_fdContexts[i] = new FdContext;
            m_fdContexts[i]->fd = i;
        }
    }
}

int IOManager::addEvent(int fd, Event event, std::function<void()> cb) {
    /**
     * 读一下 该文件描述符 看是否存在 存在则修改为当前事件 不存在则添加
     * 
     *  fd->events 初始化为 NONE & any = 0x0 即 原来没有事件
     * 
     *  第一步：从 fd 对象池中取出对应的对象指针，如果不存在就扩容对象池。
     *  第二步：检查 fd 对象是否存在相同的事件，
     *  第三步：创建 epoll 事件对象，并注册事件,
     *  最后：更新 fd 对象的事件处理器
     * 
     * & 运算
     * 0x1 & 0x1 = 0x1 (0001)
     * 0x4 & 0x4 = 0x4 (0004)
     * 0x1 & 0x4 = 0x0 (0000)
     * 0x5 & 0x4 = 0x4
     * 0x5 & 0x1 = 0x1
     * 0x0 & 0x0 = 0x0
     * 
     * | 运算
     * 
     * 0x0 | 0x1 = 0x1 ---- 读事件
     * 0x0 | 0x4 = 0x4 ---- 写事件
     * 0x1 | 0x4 = 0x5 ---- 读写事件
    */

   //1.从 fd 对象池中取出对应的对象指针，如果不存在就扩容对象池，
   FdContext* fd_ctx = nullptr;
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() > fd) {
        fd_ctx = m_fdContexts[fd];
        lock.unlock();
    } else {
        lock.unlock();
        RWMutexType::WriteLock lock2(m_mutex);
        contextResize(fd * 1.5);            //半倍扩容
        fd_ctx = m_fdContexts[fd];
    }

    //2.检查 fd 对象是否存在相同的事。 fd->events 初始化为 NONE & any = 0x0 即 原来没有事件
    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(FANSHUTOU_UNLIKELY(fd_ctx->events & event)) { 
        // 禁止给 fd 添加 相同的事件       
        FANSHUTOU_LOG_ERROR(g_logger) << "addEvent assert fd=" << fd
                    << " event=" << (EPOLL_EVENTS)event
                    << " fd_ctx.event=" << (EPOLL_EVENTS)fd_ctx->events;
        FANSHUTOU_ASSERT(!(fd_ctx->events & event));                            
    }

    //3.到这里说明fd_ctx->envets 是空(要添加) 或者 与当前指定的事件不同(要修改)
    int op = fd_ctx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;    
    epoll_event epevent;
    epevent.events = EPOLLET | fd_ctx->events | event;  //EPOLLET边缘触发
    epevent.data.ptr = fd_ctx;                          //携带的数据
    int rt = epoll_ctl(m_epfd, op, fd, &epevent);       
    if(rt) {
        FANSHUTOU_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
            << (EpollCtlOp)op << ", " << fd << ", " << (EPOLL_EVENTS)epevent.events << "):"
            << rt << " (" << errno << ") (" << strerror(errno) << ") fd_ctx->events="
            << (EPOLL_EVENTS)fd_ctx->events;
        return -1;
    }
    ++m_pendingEventCount;

    //4.更新fd_ctx 的事件内容。
    fd_ctx->events = (Event)(fd_ctx->events | event);
    FdContext::EventContext& event_ctx = fd_ctx->getContext(event);
    //如果已经设置过 该事件的EentContext 的内容就 不再重新设置
    FANSHUTOU_ASSERT(!event_ctx.scheduler
                && !event_ctx.fiber
                && !event_ctx.cb);
    //否则 获取当前的协程调度器 并 设置 为当前协程 
    event_ctx.scheduler = Scheduler::GetThis();
    if(cb) {
        event_ctx.cb.swap(cb);
    } else {
        event_ctx.fiber = Fiber::GetThis();
        FANSHUTOU_ASSERT2(event_ctx.fiber->getState() == Fiber::EXEC
                      ,"state=" << event_ctx.fiber->getState());
    }
    return 0;
}

bool IOManager::delEvent(int fd, Event event) {
    //同上面相反操作
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() <= fd) {
        return false;
    }
    FdContext* fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(FANSHUTOU_UNLIKELY(!(fd_ctx->events & event))) {
        return false;
    }
    /**
     * fd_ctx->events & ~event
     *      0x1          0x1       = 0x0
     *      0x5          0x1       = 0x4
     *      0x5          0x4       = 0x1
     *              
    */
    Event new_events = (Event)(fd_ctx->events & ~event);
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = EPOLLET | new_events;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt) {
        FANSHUTOU_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
            << (EpollCtlOp)op << ", " << fd << ", " << (EPOLL_EVENTS)epevent.events << "):"
            << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return false;
    }

    --m_pendingEventCount;
    //重置  ---- 单独取消读事件 但没有取消写事件 但还是将 fd_ctx重置了 ---- ？？？？？？？可修改逻辑？？
    fd_ctx->events = new_events;
    FdContext::EventContext& event_ctx = fd_ctx->getContext(event);
    fd_ctx->resetContext(event_ctx);
    return true;
}

bool IOManager::cancelEvent(int fd, Event event) {
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() <= fd) {
        return false;
    }
    FdContext* fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(FANSHUTOU_UNLIKELY(!(fd_ctx->events & event))) {
        return false;
    }

    Event new_events = (Event)(fd_ctx->events & ~event);
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = EPOLLET | new_events;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt) {
        FANSHUTOU_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
            << (EpollCtlOp)op << ", " << fd << ", " << (EPOLL_EVENTS)epevent.events << "):"
            << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return false;
    }
    //上面已经修改过事件了 ，如果 事件还相同 在 进入triggerEvent（）会触发断言
    fd_ctx->triggerEvent(event);        //取消 强制执行 当前事件的回调 再从任务中移除
    --m_pendingEventCount;
    return true;
}

bool IOManager::cancelAll(int fd) {
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size() <= fd) {
        return false;
    }
    FdContext* fd_ctx = m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(!fd_ctx->events) {
        return false;
    }

    int op = EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = 0;
    epevent.data.ptr = fd_ctx;

    int rt = epoll_ctl(m_epfd, op, fd, &epevent);
    if(rt) {
        FANSHUTOU_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
            << (EpollCtlOp)op << ", " << fd << ", " << (EPOLL_EVENTS)epevent.events << "):"
            << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return false;
    }

    if(fd_ctx->events & READ) {
        fd_ctx->triggerEvent(READ);
        --m_pendingEventCount;
    }
    if(fd_ctx->events & WRITE) {
        fd_ctx->triggerEvent(WRITE);
        --m_pendingEventCount;
    }

    FANSHUTOU_ASSERT(fd_ctx->events == 0);
    return true;
}

IOManager* IOManager::GetThis() {
    return dynamic_cast<IOManager*>(Scheduler::GetThis());
}

void IOManager::tickle() {
    //判断是否还有空闲的线程 没有 tickle了也没有线程去执行
    if(!hasIdleThreads()) { 
        return;
    }
    int rt = write(m_tickleFds[1], "T", 1);
    FANSHUTOU_ASSERT(rt == 1);
}

bool IOManager::stopping(uint64_t& timeout) {
    timeout = getNextTimer();
    return timeout == ~0ull
        && m_pendingEventCount == 0
        && Scheduler::stopping();

}

bool IOManager::stopping() {
    uint64_t timeout = 0;
    return stopping(timeout);
}
//核心
void IOManager::idle() {
    /**
     * 1-4 循环监听
     * 
     * 1.等待事件就绪或超时
     * 2.获取当前就绪事件
     * 3.获取事件对应的上下文信息
     * 4.触发事件回调
     * 
     * 5.出循环，让出cpu
    */
    FANSHUTOU_LOG_DEBUG(g_logger) << "idle";
    const uint64_t MAX_EVNETS = 256;
    epoll_event* events = new epoll_event[MAX_EVNETS]();
    std::shared_ptr<epoll_event> shared_events(events, [](epoll_event* ptr){
        delete[] ptr;
    });

    while(true) {
        uint64_t next_timeout = 0;
        if(FANSHUTOU_UNLIKELY(stopping(next_timeout))) {
            //在停止
            FANSHUTOU_LOG_INFO(g_logger) << "name=" << getName()
                                     << " idle stopping exit";
            break;
        }

        int rt = 0;
        //1.等待事件就绪或超时
        do {
             // 设置最大超时时间
            static const int MAX_TIMEOUT = 3000;
            if(next_timeout != ~0ull) {
                next_timeout = (int)next_timeout > MAX_TIMEOUT
                                ? MAX_TIMEOUT : next_timeout;
            } else {
                next_timeout = MAX_TIMEOUT;
            }
            rt = epoll_wait(m_epfd, events, MAX_EVNETS, (int)next_timeout);
            if(rt < 0 && errno == EINTR) {
                // 如果是(IO等待数据时，系统强制中断)中断错误，则继续等待
            } else {
                break;
            }
        } while(true);

         // 超时触发的回调函数
        std::vector<std::function<void()> > cbs;
        listExpiredCb(cbs);
        if(!cbs.empty()) {
            //FANSHUTOU_LOG_DEBUG(g_logger) << "on timer cbs.size=" << cbs.size();
            schedule(cbs.begin(), cbs.end());
            cbs.clear();
        }

        //2.获取当前就绪事件
        for(int i = 0; i < rt; ++i) {
            epoll_event& event = events[i];

            // 如果是 tickleFds 的事件，则清空 pipe 缓冲区，并继续下一个事件的处理
            if(event.data.fd == m_tickleFds[0]) {
                uint8_t dummy[256]; //该消息没有实际意义，取出来即可 --- 作用仅仅是 唤醒阻塞的线程
                while(read(m_tickleFds[0], dummy, sizeof(dummy)) > 0);
                continue;
            }
            //3.获取事件对应的上下文信息
            FdContext* fd_ctx = (FdContext*)event.data.ptr;
            FdContext::MutexType::Lock lock(fd_ctx->mutex);
            // 如果事件包含错误或者挂起的情况，修改事件以包含读写权限
            if(event.events & (EPOLLERR | EPOLLHUP)) {
                event.events |= (EPOLLIN | EPOLLOUT) & fd_ctx->events;
            }
            // 计算真正触发的事件类型       ---- 因为自己封装了一个事件类型 所以需要计算转换
            int real_events = NONE;
            if(event.events & EPOLLIN) {
                real_events |= READ;        
            }
            if(event.events & EPOLLOUT) {
                real_events |= WRITE;      
            }
            // 如果没有触发的事件，继续下一个事件的处理         
            /**
             * 给人见解：
             * ---- event与fd_ctx->fd不是相同的吗？ 为什么计算出来的会不同？？？？？ 
             * 已经将fd_ctx加入事件监听后，才能从event.data.ptr拿到数据 ， 那为什么 事件计算出来后会不同呢？
            */
            if((fd_ctx->events & real_events) == NONE) {
                continue;
            }
            // 计算剩余的事件类型 ---- 
             /**
             * fd_ctx->events & ~real_events
             *      0x1          0x1       = 0x0
             *      0x5          0x1       = 0x4
             *      0x5          0x4       = 0x1
             * 比如此次先处理 读事件 就把 读事件减调 剩余的写事件 再次加入 事件对象池中 
             * 个人见解：
             * 复用addEvent(fd_ctx->events & ~real_events); 这样不是更能增加可读性和可复用性吗？
             * 
             * 1.既然一次只能执一个事件 为什么后面 会有 同时执行 两个事件的情况
             *   那在既有读事件，也有写事件时 被剪掉的事件 不是会执行两次吗？
             * 2.另一种考虑：该线程在执行到读事件时 ，进入fiber->cb 去执行了 
             *   该线程就 要等读事件执行完后 才会切到写事件去执行，但在此期间 
             *   如果是剪切了写事件，就可以让其他空闲的线程来执行该事件，从而提高执行效率。
             * 3.反驳2 ，这一整个都被锁住了，在进入下一个for循环前都不会释放锁，
             *   其他线程就拿不到该事件剪切后的事件，只有在写事件也执行完后才会出循环，释放锁
             * 
             * 所以总结以上3点 问题多多 ？？？？？？？？？
             *              
            */      
            int left_events = (fd_ctx->events & ~real_events);
            int op = left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;       ////？？？？？？？？？
            event.events = EPOLLET | left_events;

            int rt2 = epoll_ctl(m_epfd, op, fd_ctx->fd, &event);
            if(rt2) {
                FANSHUTOU_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                    << (EpollCtlOp)op << ", " << fd_ctx->fd << ", " << (EPOLL_EVENTS)event.events << "):"
                    << rt2 << " (" << errno << ") (" << strerror(errno) << ")";
                continue;
            }

            //FANSHUTOU_LOG_INFO(g_logger) << " fd=" << fd_ctx->fd << " events=" << fd_ctx->events
            //                         << " real_events=" << real_events;
            if(real_events & READ) {
                fd_ctx->triggerEvent(READ);         //执行回调
                --m_pendingEventCount;
            }
            if(real_events & WRITE) {
                fd_ctx->triggerEvent(WRITE);
                --m_pendingEventCount;
            }
        }

        Fiber::ptr cur = Fiber::GetThis();
        auto raw_ptr = cur.get();
        cur.reset();

        raw_ptr->swapOut();
    }
}

void IOManager::onTimerInsertedAtFront() {
    tickle();
}

}
