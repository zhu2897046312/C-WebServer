#include"myfiber.h"

#include <atomic>
#include<stdlib.h>
#include<iostream>

namespace zhuyi
{
static std::atomic<uint64_t> s_fiber_id {0};
static std::atomic<uint64_t> s_fiber_count {0};
//当前协程
static thread_local Fiber* t_fiber = nullptr;
//主协程 -- 也是每一个线程的第一个协程
static thread_local Fiber::ptr t_threadFiber = nullptr;

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
        
    }

    ++s_fiber_count;

    std::cout << "Fiber::Fiber main" << std::endl;
}

Fiber::Fiber(std::function<void()> cb, size_t stacksize)
    :m_id(++s_fiber_id)
    ,m_cb(cb) {

    // if(t_fiber == nullptr){
    //     Fiber::ptr main_fiber(new Fiber);
    //     t_threadFiber = main_fiber;  
    // }

    ++s_fiber_count;
    m_stacksize = stacksize ? stacksize : 1024;

    m_stack = StackAllocator::Alloc(m_stacksize);
    if(getcontext(&m_ctx)) {

    }
    m_ctx.uc_link = nullptr;
    // m_ctx.uc_link = t_threadFiber;
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stacksize;

    makecontext(&m_ctx,&Fiber::MainFunc,0);

    std::cout << "Fiber::Fiber id=" << m_id << std::endl;
}

Fiber::~Fiber() {
    --s_fiber_count;
    if(m_stack) {
        StackAllocator::Dealloc(m_stack, m_stacksize);
    } else {        //线程在执行时，主协程一直在执行，不会有其他状态

        Fiber* cur = t_fiber;
        if(cur == this) {
            SetThis(nullptr);
        }
    }
    std::cout << "Fiber::~Fiber id=" << m_id
                              << " total=" << s_fiber_count << std::endl;
}

//重置协程函数，并重置状态
//INIT，TERM, EXCEPT
void Fiber::reset(std::function<void()> cb) {
    m_cb = cb;
    if(getcontext(&m_ctx)) {

    }

    m_ctx.uc_link = nullptr;
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stacksize;

    makecontext(&m_ctx, &Fiber::MainFunc, 0);
    m_state = INIT;
}

void Fiber::call() {
    SetThis(this);
    m_state = EXEC;
    if(swapcontext(&t_threadFiber->m_ctx, &m_ctx)) {

    }
}

void Fiber::back() {
    SetThis(t_threadFiber.get());
    if(swapcontext(&m_ctx, &t_threadFiber->m_ctx)) {

    }
}

//切换到当前协程执行            --- 操作对象一定是子协程 就从主协程切换到该协程执行了
void Fiber::swapIn() {
    SetThis(this);

    m_state = EXEC;
    if(swapcontext(&t_threadFiber->m_ctx, &m_ctx)) {

    }
}

//切换到后台执行
void Fiber::swapOut() {
    SetThis(t_threadFiber.get());
    if(swapcontext(&m_ctx, &t_threadFiber->m_ctx)) {

    }
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

    t_threadFiber = main_fiber;                 //当前没有任何一个协程，创建一个主协程
    return t_fiber->shared_from_this();
}

//协程切换到后台，并且设置为Ready状态
void Fiber::YieldToReady() {
    Fiber::ptr cur = GetThis();

    cur->m_state = READY;
    cur->swapOut();                     //把当前协程切出去，执行权回到主协程
}

//协程切换到后台，并且设置为Hold状态
void Fiber::YieldToHold() {
    Fiber::ptr cur = GetThis();

    cur->m_state = HOLD;
    cur->swapOut();
}

//总协程数
uint64_t Fiber::TotalFibers() {
    return s_fiber_count;
}

void Fiber::MainFunc() {
    Fiber::ptr cur = GetThis();

    try {
        cur->m_cb();
        cur->m_cb = nullptr;        //置空，防止引用计数重复+1 bind参数的情况下
        cur->m_state = TERM;
    } catch (std::exception& ex) {
        cur->m_state = EXCEPT;
        std::cout << "Fiber Except: " << ex.what()
            << " fiber_id=" << cur->getId()
            << std::endl;
    } catch (...) {
        cur->m_state = EXCEPT;
        std::cout << "Fiber Except"
            << " fiber_id=" << cur->getId()
            << std::endl;
    }
//这一段不明白      ---- 可以根据弹幕，重构 -- 比如在带参构造中判断是否存在t_fiber主协程


    //让子协程执行完后回到主协程    --- 让主协程去回收子协程的资源
    auto raw_ptr = cur.get();
    cur.reset();
    //上面两步，GetThis() 获取了一次 解决智能指针的引用计数问题

    raw_ptr->swapOut();         //重新切回主协程
    // cur.reset();            //智能指针计数问题
    // t_fiber->swapOut();

}


}