#include"http/http_connection.h"
#include"log.h"
#include"iomanager.h"
#include<iostream>
#include<fstream>
static fst::Logger::ptr logger = FANSHUTOU_LOG_ROOT();

void test_pool(){
    fst::http::HttpConnectionPool::ptr pool(
        new fst::http::HttpConnectionPool
        ("www.fst.top","",true,80,10,1000*30,20));

        fst::IOManager::GetThis()->addTimer(1000,[pool](){
            auto r = pool->doGet("/",300);
        },true); 
}

void run(){
    fst::Address::ptr addr = fst::Address::LookupAnyIPAddress("www.fst.top:80");
    if(!addr){
        FANSHUTOU_LOG_INFO(logger) << "get addr error";
        return;
    }
    fst::Socket::ptr sock = fst::Socket::CreateTCP(addr);
    bool rt = sock->connect(addr);
    if(!rt){
        FANSHUTOU_LOG_INFO(logger) << "connect " << *addr << " failed";
        return;
    }

    fst::http::HttpConnection::ptr conn(new fst::http::HttpConnection(sock));
    fst::http::HttpRequest::ptr req(new fst::http::HttpRequest);

    req->setHeader("host","www.fst.top");
    FANSHUTOU_LOG_INFO(logger) << "req: " << std::endl << *req;
    conn->sendRequest(req);
    auto rsp = conn->recvResponse();

    if(!rsp){
        FANSHUTOU_LOG_INFO(logger) << " recv response error ";
        return;
    }
    FANSHUTOU_LOG_INFO(logger) << "rsp:" <<std::endl << *rsp;

    std::ofstream ofs("rsp.dat");
    ofs << *rsp;

    FANSHUTOU_LOG_INFO(logger) << "=========================";

    auto r = fst::http::HttpConnection::DoGet("http://www.fst.top/blog",300);

    FANSHUTOU_LOG_INFO(logger) << "result=" << r->result
        << " error=" << r->error 
        << "rsp=" << (r->response ? r->response->toString() : "");


    FANSHUTOU_LOG_INFO(logger) << "=========================";

    test_pool();
}

int main(int argc, char** argv){
    fst::IOManager iom(2);
    iom.schedule(run);
    return 0;
}