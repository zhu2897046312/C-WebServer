#include"fiber.h"
#include"log.h"
#include"thread.h"

#include<iostream>
#include<vector>
fst::Logger::ptr logger = FANSHUTOU_LOG_ROOT();

void run_in_fiber(){
    FANSHUTOU_LOG_INFO(logger) << " run_in_fiber begin";
    fst::Fiber::YieldToHold();
    FANSHUTOU_LOG_INFO(logger) << " run_in_fiber end";
    fst::Fiber::YieldToHold();
}
void test_fiber(){
    FANSHUTOU_LOG_INFO(logger) << " main begin -1 ";
    {
        fst::Fiber::GetThis();
        FANSHUTOU_LOG_INFO(logger) << " main begin";
        fst::Fiber::ptr fiber(new fst::Fiber(run_in_fiber));
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