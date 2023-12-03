#include"http/http_server.h"
#include"log.h"

static fst::Logger::ptr logger = FANSHUTOU_LOG_ROOT();

void run(){
    fst::http::HttpServer::ptr server(new fst::http::HttpServer);
    fst::Address::ptr addr = fst::Address::LookupAnyIPAddress("0.0.0.0:8020");
    while(!server->bind(addr)){
        sleep(2);
    }
    server->start();
}

int main(){
    fst::IOManager iom(2);
    iom.schedule(run);
    return 0;
}