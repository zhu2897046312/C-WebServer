#ifndef __ZHUYI_FIBER_H_
#define __ZHUYI_FIBER_H_
#include <memory>
#include <functional>
#include <ucontext.h>

namespace zhuyi
{
class Fiber : public std::enable_shared_from_this<Fiber> {      //继承了该类 能当前类的智能指针 但当前类就不能在栈中创建对象 因为它一定要是只能指针的成员 但在构造函数中不能
public:
    typedef std::shared_ptr<Fiber> ptr;
    enum State {
        /// 初始化状态
        INIT,
        /// 暂停状态
        HOLD,
        /// 执行中状态
        EXEC,
        /// 结束状态
        TERM,
        /// 可执行状态
        READY,
        /// 异常状态
        EXCEPT
    };
private:
    Fiber();    //不许默认构造  -- 私有的构造函数 当我们使用静态方法获取类的实例时，需要通过私有构造函数来实现
                //真正的主协程   ---- 主协程没有栈
public:
    Fiber(std::function<void()> cb, size_t stacksize = 0);
                    //开启新的协程，需要分配栈空间   --- 每一个协程都有一个独立的栈
    ~Fiber();

    void reset(std::function<void()> cb);           //重置，省了创建和重新构造         
    void swapIn();                                  //进去执行，切换到当前协程执行
    void swapOut();                                 //让出执行控制权，切换到main协程
    void call();
    void back();
    uint64_t getId() const { return m_id;}
    State getState() const { return m_state;}
public:
    static void SetThis(Fiber* f);                  //设置当前协程
    static Fiber::ptr GetThis();                    //返回当前协程
    static void YieldToReady();                     //让出执行权，并设置状态，即 切换到后台
    static void YieldToHold();                      //让出执行权，并设置状态，
    static uint64_t TotalFibers();
    static void MainFunc();
    static void CallerMainFunc();
    static uint64_t GetFiberId();
private:
    uint64_t m_id = 0;
    uint32_t m_stacksize = 0;
    State m_state = INIT;
    ucontext_t m_ctx;           //上下文结构
    void* m_stack = nullptr;
    std::function<void()> m_cb; //协程回调
};

}

#endif