#include"myfiber.h"
#include"log.h"
#include"thread.h"

#include<iostream>
fst::Logger::ptr logger = FANSHUTOU_LOG_ROOT();
void run_in_fiber(){
    std::cout << " run_in_fiber begin" <<std::endl;
    fst::Fiber::YieldToHold();
    std::cout << " run_in_fiber end" <<std::endl;
    fst::Fiber::YieldToHold();
}
void run_my_fiber(){
    std::cout << " main begin -1 " <<std::endl;
    {
        // zhuyi::Fiber::GetThis();
        std::cout << " main begin" <<std::endl;
        zhuyi::Fiber::ptr fiber(new zhuyi::Fiber(run_in_fiber,1024));
        fiber->swapIn();
        std::cout << " main after swapIn" <<std::endl;
        fiber->swapIn();
        std::cout << " main after end" <<std::endl;
        fiber->swapIn();
    }
    std::cout << " main after end2 " <<std::endl;
}
void test_fiber(){
    FANSHUTOU_LOG_INFO(logger) << " main begin -1 ";
    {
        zhuyi::Fiber::GetThis();
        FANSHUTOU_LOG_INFO(logger) << " main begin";
        zhuyi::Fiber::ptr fiber(new zhuyi::Fiber(run_in_fiber,1024));
        fiber->swapIn();
        FANSHUTOU_LOG_INFO(logger) << " main after swapIn";
        fiber->swapIn();
        FANSHUTOU_LOG_INFO(logger) << " main after end" ;
        fiber->swapIn();
    }
    FANSHUTOU_LOG_INFO(logger) << " main after end2 " ;
}
int main(){
    fst::Thread::SetName("main");
    std::vector<fst::Thread::ptr> thrs;
    for(int i = 0; i < 3 ; i++){
        thrs.push_back(fst::Thread::ptr(new fst::Thread(&test_fiber,"name_" + std::to_string(i))));
    }
    for(auto i : thrs){
        i->join();
    }
    return 0;
}