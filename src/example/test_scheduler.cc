#include"scheduler.h"
#include"log.h"



fst::Logger::ptr logger = FANSHUTOU_LOG_ROOT();


int main(){
    fst::Scheduler sc(2);
    sc.start();
    sc.stop();
    return 0;
}