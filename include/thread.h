#ifndef __FANSHUTOU_THREAD_H__
#define __FANSHUTOU_THREAD_H__

#include "mutex.h"

#include<string>
namespace fst {

class Thread : Noncopyable {
public:
    typedef std::shared_ptr<Thread> ptr;
    Thread(std::function<void()> cb, const std::string& name);
    ~Thread();

    pid_t getId() const { return m_id;}                     //返回线程id
    const std::string& getName() const { return m_name;}    //返回线程名称
    void join();

    static Thread* GetThis();                       //返回当前线程
    static const std::string& GetName();            //返回当前线程名称
    static void SetName(const std::string& name);   //设置当前线程名称
private:

    static void* run(void* arg);
private:
    pid_t m_id = -1;            //线程id
    pthread_t m_thread = 0;     //线程结构
    std::function<void()> m_cb; //线程回调函数
    std::string m_name;         //线程名称
    Semaphore m_semaphore;      //信号量        --- 每一个线程都有一份是 因为线程间通信保证变量的线程安全问题
};

}

#endif
