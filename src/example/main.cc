#include"log.h"
#include"thread.h"
#include"config.h"
#include"fiber.h"
#include"scheduler.h"
#include"iomanager.h"
#include"socket.h"
#include"address.h"
#include"http/http.h"
#include"http/myhttp_parser.h"
#include"tcp_server.h"

#include<sys/socket.h>
#include<arpa/inet.h>
#include<sys/types.h>
#include<fcntl.h>
fst::Logger::ptr logger = FANSHUTOU_LOG_ROOT();

// fst::ConfigVar<int>::ptr g_int_value_config =
//     fst::Config::Lookup("system.port", (int)8080, "system port");

// fst::ConfigVar<float>::ptr g_int_valuex_config =
//     fst::Config::Lookup("system.port", (float)8080, "system port");

// fst::ConfigVar<float>::ptr g_float_value_config =
//     fst::Config::Lookup("system.value", (float)10.2f, "system value");

// fst::ConfigVar<std::vector<int> >::ptr g_int_vec_value_config =
//     fst::Config::Lookup("system.int_vec", std::vector<int>{1,2}, "system int vec");

// fst::ConfigVar<std::list<int> >::ptr g_int_list_value_config =
//     fst::Config::Lookup("system.int_list", std::list<int>{1,2}, "system int list");

// fst::ConfigVar<std::set<int> >::ptr g_int_set_value_config =
//     fst::Config::Lookup("system.int_set", std::set<int>{1,2}, "system int set");

// fst::ConfigVar<std::unordered_set<int> >::ptr g_int_uset_value_config =
//     fst::Config::Lookup("system.int_uset", std::unordered_set<int>{1,2}, "system int uset");

// fst::ConfigVar<std::map<std::string, int> >::ptr g_str_int_map_value_config =
//     fst::Config::Lookup("system.str_int_map", std::map<std::string, int>{{"k",2}}, "system str int map");

// fst::ConfigVar<std::unordered_map<std::string, int> >::ptr g_str_int_umap_value_config =
//     fst::Config::Lookup("system.str_int_umap", std::unordered_map<std::string, int>{{"k",2}}, "system str int map");

// void print_yaml(const YAML::Node& node, int level) {
//     if(node.IsScalar()) {
//         FANSHUTOU_LOG_INFO(FANSHUTOU_LOG_ROOT()) << std::string(level * 4, ' ')
//             << node.Scalar() << " - " << node.Type() << " - " << level;
//     } else if(node.IsNull()) {
//         FANSHUTOU_LOG_INFO(FANSHUTOU_LOG_ROOT()) << std::string(level * 4, ' ')
//             << "NULL - " << node.Type() << " - " << level;
//     } else if(node.IsMap()) {
//         for(auto it = node.begin();
//                 it != node.end(); ++it) {
//             FANSHUTOU_LOG_INFO(FANSHUTOU_LOG_ROOT()) << std::string(level * 4, ' ')
//                     << it->first << " - " << it->second.Type() << " - " << level;
//             print_yaml(it->second, level + 1);
//         }
//     } else if(node.IsSequence()) {
//         for(size_t i = 0; i < node.size(); ++i) {
//             FANSHUTOU_LOG_INFO(FANSHUTOU_LOG_ROOT()) << std::string(level * 4, ' ')
//                 << i << " - " << node[i].Type() << " - " << level;
//             print_yaml(node[i], level + 1);
//         }
//     }
// }

// void test_yaml() {
//     YAML::Node root = YAML::LoadFile("./log.yml");
//     //print_yaml(root, 0);
//     //FANSHUTOU_LOG_INFO(FANSHUTOU_LOG_ROOT()) << root.Scalar();

//     FANSHUTOU_LOG_INFO(FANSHUTOU_LOG_ROOT()) << root["test"].IsDefined();
//     FANSHUTOU_LOG_INFO(FANSHUTOU_LOG_ROOT()) << root["logs"].IsDefined();
//     FANSHUTOU_LOG_INFO(FANSHUTOU_LOG_ROOT()) << root;
// }

// void run_in_fiber()
// {
//     FANSHUTOU_LOG_INFO(g_logger) << "run in fiber begin";
//     fst::Fiber::YieldToHold();
//     FANSHUTOU_LOG_INFO(g_logger) << "run in fiber end";
//      fst::Fiber::YieldToHold();
// }

// void test_fiber()
// {
//     FANSHUTOU_LOG_INFO(FANSHUTOU_LOG_ROOT()) << " main begin -1 ";
//     {
//         fst::Fiber::GetThis();
//         FANSHUTOU_LOG_INFO(FANSHUTOU_LOG_ROOT()) << " main begin ";
//         fst::Fiber::ptr fiber(new fst::Fiber(run_in_fiber));
//         fiber->swapIn();
//         FANSHUTOU_LOG_INFO(FANSHUTOU_LOG_ROOT()) << " mmain after swapin ";
//         fiber->swapIn();
//         FANSHUTOU_LOG_INFO(FANSHUTOU_LOG_ROOT()) << " main after end ";
//         fiber->swapIn();
//     }
// }
// void main_test_yaml()
// {
//     FANSHUTOU_LOG_INFO(FANSHUTOU_LOG_ROOT()) << "test_thread begin";
//     test_yaml();
//     YAML::Node root = YAML::LoadFile("./log.yml");
//     fst::Config::LoadFromYaml(root);
//     print_yaml(root,fst::LogLevel::INFO);
// }


// void test_http_server()
// {
//     fst::http::HttpServer::ptr server(new fst::http::HttpServer);
//     fst::Address::ptr addr = fst::Address::LookupAnyIPAddress("0.0.0.0:8080");
//     while(server->bind(addr)){sleep(2);}
//     server->start();
// }

void test_socket()
{ 
    fst::IPAddress::ptr addr= fst::Address::LookupAnyIPAddress("www.baidu.com");
    if(addr){
        FANSHUTOU_LOG_INFO(logger) << "get address: " << addr->toString();
    }else{
        FANSHUTOU_LOG_INFO(logger) <<"get address fail";
        return;
    }

    fst::Socket::ptr sock = fst::Socket::CreateTCP(addr); 
    addr->setPort(80);
    if(!sock->connect(addr)){
        FANSHUTOU_LOG_ERROR(logger) << "connect" << addr->toString() << "fail";
        return;
     }
    const char buff[] = "GET / HTTP/1.0\r\n\r\n";
    int rt = sock->send(buff,sizeof(buff)); 
    if(rt <= 0){
        FANSHUTOU_LOG_INFO(logger) << "send fail rt=" <<rt;
        return;
    }else{
        FANSHUTOU_LOG_INFO(logger) << "connect " << addr->toString() << "connected";
    }
    std::string buffs;
    buffs.resize(4096);
    // char* buff;
    rt = sock->recv(&buffs[0],buffs.size());
    if(rt <= 0){
        FANSHUTOU_LOG_INFO(logger) << "recv fail rt=" <<rt;
        return;
    }
    buffs.resize(rt);
    FANSHUTOU_LOG_INFO(logger) << buffs;
}

void test_addr(){
    std::vector<fst::Address::ptr> addrs;

    FANSHUTOU_LOG_INFO(logger) << "begin";

    bool v = fst::Address::Lookup(addrs,"www.fst.top",AF_INET);
    FANSHUTOU_LOG_INFO(logger) << "end";
    if(!v){
        FANSHUTOU_LOG_ERROR(logger) << "lookup fail";
        return;
    }
    for(size_t i = 0; i < addrs.size(); i++){
        FANSHUTOU_LOG_INFO(logger) << i << " - " << addrs[i]->toString();
    }
}

void test1(){
    FANSHUTOU_LOG_INFO(logger) <<"test fiber";
}
void test_iomanager(){
    fst::IOManager iom;
    iom.schedule(&test1);

    int sock = socket(AF_INET,SOCK_STREAM,0);
    fcntl(sock,F_SETFL,O_NONBLOCK);

    sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET,"120.232.145.144",&addr.sin_addr.s_addr);

    connect(sock,(const sockaddr*)&addr,sizeof(addr));
    iom.addEvent(sock,fst::IOManager::READ,[]{FANSHUTOU_LOG_INFO(logger) << "connected" ;});
    iom.addEvent(sock,fst::IOManager::WRITE,[]{FANSHUTOU_LOG_INFO(logger) << "connected" ;});
}
void test_http_parser(){
    fst::http::HttpRequestParser parser;
    const char data[] = {
        "POST / HTTP/1.1\r\n"
        "Host: www.fst.top\r\n"
        "Content-Length: 10\r\n\r\n"
        "1234567890"};
    std::string tmp = data;
    size_t s = parser.execute(&tmp[0],tmp.size());

    FANSHUTOU_LOG_INFO(logger) << "execute rt = " << s 
            << "has_error=" << parser.hasError()
            << " is_finished=" << parser.isFinished();
    FANSHUTOU_LOG_INFO(logger) << parser.getData()->toString();
}

void test_tcpserver(){
    auto addr = fst::Address::LookupAny("0.0.0.0:8033");
    auto addr2 = fst::UnixAddress::ptr(new fst::UnixAddress("/tmp/unix_addr"));
    FANSHUTOU_LOG_INFO(logger) << *addr << " - " << *addr2;
    std::vector<fst::Address::ptr> addrs;
    addrs.push_back(addr);
    addrs.push_back(addr2);

    fst::TcpServer::ptr tcp_server(new fst::TcpServer);
    std::vector<fst::Address::ptr> fails;
    while (!tcp_server->bind(addrs,fails))
    {
        sleep(2);
    }
    
    
    tcp_server->start();
}
int main()
{
    /*

    //test_http_server();
    // fst::IOManager iom(2);
    // iom.schedule(test_http_server);
    */

    // fst::IOManager iom;
    // iom.schedule(&test_socket);

    // test_addr();
    // SThreadPool pool(10);
    // pool.AddTask(&test_socket);
    //test_socket();
    // test_iomanager();

    // test_http_parser();

     fst::IOManager iom(2);
    iom.schedule(&test_tcpserver);
    return 0;
}