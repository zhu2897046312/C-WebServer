#include "fiber.h"
#include "config.h"
#include "macro.h"
#include "log.h"
#include "scheduler.h"
#include <atomic>

namespace fst {

static Logger::ptr g_logger = FANSHUTOU_LOG_NAME("system");

static std::atomic<uint64_t> s_fiber_id {0};
static std::atomic<uint64_t> s_fiber_count {0};
//当前协程
static thread_local Fiber* t_fiber = nullptr;
//主协程 -- 也是每一个线程的第一个协程
static thread_local Fiber::ptr t_threadFiber = nullptr;     // ---> 执行run方法 进行协程调度

static ConfigVar<uint32_t>::ptr g_fiber_stack_size =
    Config::Lookup<uint32_t>("fiber.stack_size", 128 * 1024, "fiber stack size");

class MallocStackAllocator {
public:
    static void* Alloc(size_t size) {
        return malloc(size);
    }

    static void Dealloc(void* vp, size_t size) {
        return free(vp);
    }
};

using StackAllocator = MallocStackAllocator;

uint64_t Fiber::GetFiberId() {
    if(t_fiber) {
        return t_fiber->getId();
    }
    return 0;
}

Fiber::Fiber() {
    m_state = EXEC;
    SetThis(this);

    if(getcontext(&m_ctx)) {
        FANSHUTOU_ASSERT2(false, "getcontext");
    }

    ++s_fiber_count;

    FANSHUTOU_LOG_DEBUG(g_logger) << "Fiber::Fiber main";
}

Fiber::Fiber(std::function<void()> cb, size_t stacksize, bool use_caller)
    :m_id(++s_fiber_id)
    ,m_cb(cb) {
    ++s_fiber_count;
    m_stacksize = stacksize ? stacksize : g_fiber_stack_size->getValue();

    m_stack = StackAllocator::Alloc(m_stacksize);
    if(getcontext(&m_ctx)) {
        FANSHUTOU_ASSERT2(false, "getcontext");
    }
    m_ctx.uc_link = nullptr;
    // m_ctx.uc_link = t_threadFiber;
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stacksize;

    if(!use_caller) {
        makecontext(&m_ctx, &Fiber::MainFunc, 0);
    } else {
        makecontext(&m_ctx, &Fiber::CallerMainFunc, 0);
    }
    // makecontext(&m_ctx, &Fiber::MainFunc, 0);

    FANSHUTOU_LOG_DEBUG(g_logger) << "Fiber::Fiber id=" << m_id;
}

Fiber::~Fiber() {
    --s_fiber_count;
    if(m_stack) {
        FANSHUTOU_ASSERT(m_state == TERM
                || m_state == EXCEPT
                || m_state == INIT);

        StackAllocator::Dealloc(m_stack, m_stacksize);
    } else {        //线程在执行时，主协程一直在执行，不会有其他状态
        FANSHUTOU_ASSERT(!m_cb);
        FANSHUTOU_ASSERT(m_state == EXEC);

        Fiber* cur = t_fiber;
        if(cur == this) {
            SetThis(nullptr);
        }
    }
    FANSHUTOU_LOG_DEBUG(g_logger) << "Fiber::~Fiber id=" << m_id
                              << " total=" << s_fiber_count;
}

//重置协程函数，并重置状态
//INIT，TERM, EXCEPT
void Fiber::reset(std::function<void()> cb) {
    FANSHUTOU_ASSERT(m_stack);
    FANSHUTOU_ASSERT(m_state == TERM
            || m_state == EXCEPT
            || m_state == INIT);
    m_cb = cb;
    if(getcontext(&m_ctx)) {
        FANSHUTOU_ASSERT2(false, "getcontext");
    }

    m_ctx.uc_link = nullptr;
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stacksize;

    makecontext(&m_ctx, &Fiber::MainFunc, 0);
    m_state = INIT;
}
/**
 * 线程的主协程t_htreadFiber 不会进入 MainFunc 中 
*/
void Fiber::call() {
    SetThis(this);      //
    m_state = EXEC;
    if(swapcontext(&t_threadFiber->m_ctx, &m_ctx)) {
        FANSHUTOU_ASSERT2(false, "swapcontext");
    }
}

void Fiber::back() {
    SetThis(t_threadFiber.get());
    if(swapcontext(&m_ctx, &t_threadFiber->m_ctx)) {
        FANSHUTOU_ASSERT2(false, "swapcontext");
    }
}

//切换到当前协程执行            --- 操作对象一定是子协程 就从主协程切换到该协程执行了
void Fiber::swapIn() {
    SetThis(this);
    FANSHUTOU_ASSERT(m_state != EXEC);
    m_state = EXEC;
    // 让协程调度器创建的m_rootFiber 作为主协程
    if(swapcontext(&Scheduler::GetMainFiber()->m_ctx, &m_ctx)) {
        FANSHUTOU_ASSERT2(false, "swapcontext");
    }
    // if(swapcontext(&t_threadFiber->m_ctx, &m_ctx)) {
    //     FANSHUTOU_ASSERT2(false, "swapcontext");
    // }
}

//切换到后台执行
void Fiber::swapOut() {
    SetThis(Scheduler::GetMainFiber());
    if(swapcontext(&m_ctx, &Scheduler::GetMainFiber()->m_ctx)) {    //t_scheduler_fiber --- 
        FANSHUTOU_ASSERT2(false, "swapcontext");
    }
    // if(swapcontext(&m_ctx, &t_threadFiber->m_ctx)) {
    //     FANSHUTOU_ASSERT2(false, "swapcontext");
    // }
}

//设置当前协程
void Fiber::SetThis(Fiber* f) {
    t_fiber = f;
}

//返回当前协程
Fiber::ptr Fiber::GetThis() {
    if(t_fiber) {
        return t_fiber->shared_from_this();
    }
    //当前没有协程 --- 也就没有主协程
    Fiber::ptr main_fiber(new Fiber);
    FANSHUTOU_ASSERT(t_fiber == main_fiber.get());
    t_threadFiber = main_fiber;                 //当前没有任何一个协程，创建一个主协程
    return t_fiber->shared_from_this();
}

//协程切换到后台，并且设置为Ready状态
void Fiber::YieldToReady() {
    Fiber::ptr cur = GetThis();
    FANSHUTOU_ASSERT(cur->m_state == EXEC);
    cur->m_state = READY;
    cur->swapOut();                     //把当前协程切出去，执行权回到主协程
}

//协程切换到后台，并且设置为Hold状态
void Fiber::YieldToHold() {
    Fiber::ptr cur = GetThis();
    FANSHUTOU_ASSERT(cur->m_state == EXEC);
    //cur->m_state = HOLD;
    cur->swapOut();
}

//总协程数
uint64_t Fiber::TotalFibers() {
    return s_fiber_count;
}

void Fiber::MainFunc() {
    Fiber::ptr cur = GetThis();
    FANSHUTOU_ASSERT(cur);
    try {
        cur->m_cb();
        cur->m_cb = nullptr;        //置空，防止引用计数重复+1 bind参数的情况下
        cur->m_state = TERM;
    } catch (std::exception& ex) {
        cur->m_state = EXCEPT;
        FANSHUTOU_LOG_ERROR(g_logger) << "Fiber Except: " << ex.what()
            << " fiber_id=" << cur->getId()
            << std::endl
            << fst::BacktraceToString();
    } catch (...) {
        cur->m_state = EXCEPT;
        FANSHUTOU_LOG_ERROR(g_logger) << "Fiber Except"
            << " fiber_id=" << cur->getId()
            << std::endl
            << fst::BacktraceToString();
    }
//这一段不明白      ---- 可以根据弹幕，重构 -- 比如在带参构造中判断是否存在t_fiber主协程


    //让子协程执行完后回到主协程    --- 让主协程去回收子协程的资源
    auto raw_ptr = cur.get();
    cur.reset();
    //上面两步，GetThis() 获取了一次 解决智能指针的引用计数问题
    raw_ptr->swapOut();         //重新切回主协程

    FANSHUTOU_ASSERT2(false, "never reach fiber_id=" + std::to_string(raw_ptr->getId()));
}

void Fiber::CallerMainFunc() {
    Fiber::ptr cur = GetThis();     // GetThis 是默认构造，创建的是主协程，主协程没有cb，就会产生异常所以需要itry catch
    //智能指针引用计数 +1 


    FANSHUTOU_ASSERT(cur);
    try {
        cur->m_cb();
        cur->m_cb = nullptr;
        cur->m_state = TERM;
    } catch (std::exception& ex) {
        cur->m_state = EXCEPT;
        FANSHUTOU_LOG_ERROR(g_logger) << "Fiber Except: " << ex.what()
            << " fiber_id=" << cur->getId()
            << std::endl
            << fst::BacktraceToString();
    } catch (...) {
        cur->m_state = EXCEPT;
        FANSHUTOU_LOG_ERROR(g_logger) << "Fiber Except"
            << " fiber_id=" << cur->getId()
            << std::endl
            << fst::BacktraceToString();
    }

    auto raw_ptr = cur.get();
    cur.reset();                //释放指针 引用计数-1 使引用计数减为0 释放内存 -- 引用计数为 1，那么 reset() 函数会释放当前指针所管理的对象，并将引用计数置为 0。
                                //引用计数大于 1，那么 reset() 函数会减少引用计数，但不会立即释放对象。只有当引用计数减少到 0 时，才会释放对象。
    raw_ptr->back();
    FANSHUTOU_ASSERT2(false, "never reach fiber_id=" + std::to_string(raw_ptr->getId()));

}

}

/**
#include <stdio.h>
#include <ucontext.h>

ucontext_t main_ctx, child_ctx;

void child_function() {
    printf("Child function: Before context switch\n");
    swapcontext(&child_ctx, &main_ctx); // 切换到主函数上下文
    printf("Child function: After context switch\n");
}

int main() {
    getcontext(&child_ctx);
    child_ctx.uc_link = &main_ctx; // 设置子函数执行完毕后切换到主函数上下文  --- uc_link(连接要切换到的函数上下文) 的使用方式 在上面的带参构造中可以直接连接主协程 
    child_ctx.uc_stack.ss_sp = malloc(SIGSTKSZ);
    child_ctx.uc_stack.ss_size = SIGSTKSZ;
    makecontext(&child_ctx, child_function, 0);

    printf("Main function: Before context switch\n");
    swapcontext(&main_ctx, &child_ctx); // 切换到子函数上下文
    printf("Main function: After context switch\n");

    free(child_ctx.uc_stack.ss_sp);
    return 0;
}
*/