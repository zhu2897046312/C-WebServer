#include "scheduler.h"
#include "log.h"
#include "macro.h"
#include "hook.h"

namespace fst {

static fst::Logger::ptr g_logger = FANSHUTOU_LOG_NAME("system");

//每一个线程都有一个协调度器
static thread_local Scheduler* t_scheduler = nullptr;
//调度协程  -- 的主协程
static thread_local Fiber* t_scheduler_fiber = nullptr;

Scheduler::Scheduler(size_t threads, bool use_caller, const std::string& name)
    :m_name(name) {
    FANSHUTOU_ASSERT(threads > 0);

    if(use_caller) {
        fst::Fiber::GetThis();    //初始化主协程
        --threads;                  
        /**创建scheduler的线程，也是当前线程也会作为工作线程 也就不需要多创建一个线程 
         * 比如我指定 3个工作线程 use_caller = true 就有 创建scheduler的线程 + 另外多创建 2 个线程
         * 如果use_caller  false 就有 创建scheduler 的线程 不工作 + 另外创建 3 个工作线程
         */
        FANSHUTOU_ASSERT(GetThis() == nullptr); //一个线程只能有一个协程调度器
        t_scheduler = this;

        m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, true));    
        /**协程调度器创建的新的主协程 reset出来的是真正执行所有调度操作方法的协程  --- run() 让这个rootFiber 来做 调度器的工作
         *GetThis()出来的主协程是负责调度其余协程
         */

        /**
         * 在线程里面声明一个调度器，再把该线程加入到调度器中作为工作线程时，
         * 该线程的主协程不再是 该线程一开始创建的主协程 而是调度器中创建的 主协程 m_rootFiber 作为该线程的主协程
         * */ 
        fst::Thread::SetName(m_name);

        t_scheduler_fiber = m_rootFiber.get();
        m_rootThread = fst::GetThreadId();
        m_threadIds.push_back(m_rootThread);
    } else {
        m_rootThread = -1;      // 不是use_caller 线程
    }
    /**
     * 没有使用创建调度器线程作为工作线程时，
     * 调度协程的还是每一个线程创建的主协程t_fiber
    */
    m_threadCount = threads;
}

Scheduler::~Scheduler() {
    FANSHUTOU_ASSERT(m_stopping);
    if(GetThis() == this) {
        t_scheduler = nullptr;
    }
}

Scheduler* Scheduler::GetThis() {
    return t_scheduler;
}

Fiber* Scheduler::GetMainFiber() {
    return t_scheduler_fiber;
}

void Scheduler::start() {
    MutexType::Lock lock(m_mutex);
    if(!m_stopping) {
        return;             //默认m_stopping = true 说明还没启动 就需要继续下面的操作启动该调度器 如果其为false 就说明已经启动了 就i不需要重新启动一次直接返回就行
    }
    m_stopping = false;
    FANSHUTOU_ASSERT(m_threads.empty());

    m_threads.resize(m_threadCount);
    for(size_t i = 0; i < m_threadCount; ++i) {
        m_threads[i].reset(new Thread(std::bind(&Scheduler::run, this)  //让线程来执行调度工作
                            , m_name + "_" + std::to_string(i)));
        m_threadIds.push_back(m_threads[i]->getId());
    }
    lock.unlock();

    //if(m_rootFiber) {
    //    //m_rootFiber->swapIn();
    //    m_rootFiber->call();
    //    FANSHUTOU_LOG_INFO(g_logger) << "call out " << m_rootFiber->getState();
    //}

    //在start启动会直接切到run 任务还没进来就idle就执行结束了，整个scheduler就结束了 ，所以将其放到stop中启动
}

void Scheduler::stop() {
    m_autoStop = true;
    if(m_rootFiber
            && m_threadCount == 0
            && (m_rootFiber->getState() == Fiber::TERM
                || m_rootFiber->getState() == Fiber::INIT)) {
        FANSHUTOU_LOG_INFO(g_logger) << this << " stopped";
        m_stopping = true;

        if(stopping()) {
            return;
        }
    }
    /**
     * 主线程 ！= -1 说明是创建用了use_caller调度器的线程 也就是 use_caller 的线程 当调度要stop时 让创建它的线程去执行stop 而不是让其他线程去执行
     * 没有的调度器可以在任意线程执行stop
    */
    if(m_rootThread != -1) {
        FANSHUTOU_ASSERT(GetThis() == this);
    } else {
        FANSHUTOU_ASSERT(GetThis() != this);
    }

    m_stopping = true;
    for(size_t i = 0; i < m_threadCount; ++i) {
        tickle();
    }
    //唤醒线程让它们自己结束线程 m_threadCount 不包括scheduler的线程 所以下面 判段 m_rootFiber 是否存在 来判断是否使用use_caller 如果是 则发tickle 通知该线程结束

    if(m_rootFiber) {
        tickle();
    }

    if(m_rootFiber) {
        if(!stopping()) {
            m_rootFiber->call();    //启动调度工作run 
        }
    }

    std::vector<Thread::ptr> thrs;
    {
        MutexType::Lock lock(m_mutex);
        thrs.swap(m_threads);
    }

    for(auto& i : thrs) {
        i->join();
    }
}

void Scheduler::setThis() {
    t_scheduler = this;
}
/**
 * 将同一个回调函数绑定到多个线程中的不同任务或事件上，每个线程会独立执行回调函数。
 * 每个线程都会有自己的栈空间和局部变量，这意味着回调函数中的局部变量对于每个线程来说是线程局部的。
*/
void Scheduler::run() {
    FANSHUTOU_LOG_DEBUG(g_logger) << m_name << " run";
    set_hook_enable(true);
    setThis();
    if(fst::GetThreadId() != m_rootThread) {
        t_scheduler_fiber = Fiber::GetThis().get(); //如果不是use_caller 线程 主协程 就是 线程直接创建的 t_fiber为主协程
    }

    Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle, this)));    //没有认为执行idle 不让线程终止
    Fiber::ptr cb_fiber;            //基于function 创建的协程 

    FiberAndThread ft;
    while(true) {
        ft.reset();
        bool tickle_me = false;
        bool is_active = false;
        {
            //取出一个协程
            MutexType::Lock lock(m_mutex);
            auto it = m_fibers.begin();
            while(it != m_fibers.end()) {
                if(it->thread != -1 && it->thread != fst::GetThreadId()) {
                    ++it;
                    tickle_me = true; // 通知其他线程，因为该协程指定了线程执行
                    continue;
                }

                FANSHUTOU_ASSERT(it->fiber || it->cb);
                if(it->fiber && it->fiber->getState() == Fiber::EXEC) {         // ？ 都exec态了，为什么还在任务队列中
                    ++it;
                    continue;   //该协程处于执行态
                }

                ft = *it;   //拷贝一份任务
                m_fibers.erase(it++);//从任务列表移除
                ++m_activeThreadCount;
                is_active = true;
                break;
            }
            tickle_me |= it != m_fibers.end();//都为false则tickle_me为false
        }

        if(tickle_me) {
            tickle();
        }
        //是fiber
        if(ft.fiber && (ft.fiber->getState() != Fiber::TERM
                        && ft.fiber->getState() != Fiber::EXCEPT)) {
            ft.fiber->swapIn(); //执行
            --m_activeThreadCount;//活跃线程数  

            //执行完，回来后 判断是否还需执行
            if(ft.fiber->getState() == Fiber::READY) {
                schedule(ft.fiber);         //再次加入到任务队列中
            } else if(ft.fiber->getState() != Fiber::TERM
                    && ft.fiber->getState() != Fiber::EXCEPT) {
                ft.fiber->m_state = Fiber::HOLD;    //让出cpu
            }
            ft.reset();
        //是cb
        } else if(ft.cb) {
            if(cb_fiber) {
                cb_fiber->reset(ft.cb);
            } else {
                cb_fiber.reset(new Fiber(ft.cb));
            }
            ft.reset();
            cb_fiber->swapIn();
            --m_activeThreadCount;  //执行完回来 活跃线程 --  
            if(cb_fiber->getState() == Fiber::READY) {
                schedule(cb_fiber);
                cb_fiber.reset();
            } else if(cb_fiber->getState() == Fiber::EXCEPT
                    || cb_fiber->getState() == Fiber::TERM) {
                cb_fiber->reset(nullptr);
            } else {//if(cb_fiber->getState() != Fiber::TERM) 
                cb_fiber->m_state = Fiber::HOLD;        //HOLD状态为什么不进入消息队列？
                cb_fiber.reset();
            }
        } else {
            //两种任务都不是  -- 因为在取任务时 ++ 了 但它既不是fiber 也 不是 function 任务 所以在来到这时要 -- 
            if(is_active) {
                --m_activeThreadCount;
                continue;
            }
            if(idle_fiber->getState() == Fiber::TERM) {
                FANSHUTOU_LOG_INFO(g_logger) << "idle fiber term";
                break;
            }

            ++m_idleThreadCount;
            idle_fiber->swapIn();
            --m_idleThreadCount;
            if(idle_fiber->getState() != Fiber::TERM
                    && idle_fiber->getState() != Fiber::EXCEPT) {
                idle_fiber->m_state = Fiber::HOLD;
            }
        }
    }
}

void Scheduler::tickle() {
    FANSHUTOU_LOG_INFO(g_logger) << "tickle";
}

bool Scheduler::stopping() {
    MutexType::Lock lock(m_mutex);
    return m_autoStop && m_stopping
        && m_fibers.empty() && m_activeThreadCount == 0;
}

void Scheduler::idle() {
    FANSHUTOU_LOG_INFO(g_logger) << "idle";
    while(!stopping()) {
        fst::Fiber::YieldToHold();
    }
}

void Scheduler::switchTo(int thread) {
    FANSHUTOU_ASSERT(Scheduler::GetThis() != nullptr);
    if(Scheduler::GetThis() == this) {
        if(thread == -1 || thread == fst::GetThreadId()) {
            return;
        }
    }
    schedule(Fiber::GetThis(), thread);
    Fiber::YieldToHold();
}

std::ostream& Scheduler::dump(std::ostream& os) {
    os << "[Scheduler name=" << m_name
       << " size=" << m_threadCount
       << " active_count=" << m_activeThreadCount
       << " idle_count=" << m_idleThreadCount
       << " stopping=" << m_stopping
       << " ]" << std::endl << "    ";
    for(size_t i = 0; i < m_threadIds.size(); ++i) {
        if(i) {
            os << ", ";
        }
        os << m_threadIds[i];
    }
    return os;
}

SchedulerSwitcher::SchedulerSwitcher(Scheduler* target) {
    m_caller = Scheduler::GetThis();
    if(target) {
        target->switchTo();
    }
}

SchedulerSwitcher::~SchedulerSwitcher() {
    if(m_caller) {
        m_caller->switchTo();
    }
}

}
