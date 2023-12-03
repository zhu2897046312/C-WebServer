#include"log.h"
fst::Logger::ptr logger = FANSHUTOU_LOG_ROOT();
int main(){
    FANSHUTOU_LOG_INFO(logger) << "---------------";
    return 0;
}