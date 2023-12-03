#include"log.h"
#include"iomanager.h"

#include<sys/socket.h>
#include<sys/types.h>
#include<fcntl.h>
#include<arpa/inet.h>
#include<unistd.h>

fst::Logger::ptr logger = FANSHUTOU_LOG_ROOT();

void test_fiber(){
    FANSHUTOU_LOG_INFO(logger) << "test_fiber";
}

void test1(){
    fst::IOManager iom;
    iom.schedule(&test_fiber);

    int sock = socket(AF_INET,SOCK_STREAM,0);
    fcntl(sock,F_SETFL,O_NONBLOCK);
    sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET,"120.232.145.185",&addr.sin_addr.s_addr);

    iom.addEvent(sock,fst::IOManager::WRITE,[](){
        FANSHUTOU_LOG_INFO(logger) << "connected";
    });

    connect(sock,(const sockaddr*)&addr,sizeof(addr));
}

int main(){
    test1();
    return 0;
}