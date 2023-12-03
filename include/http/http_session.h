
#ifndef __FANSHUTOU_HTTP_SESSION_H__
#define __FANSHUTOU_HTTP_SESSION_H__

#include "../streams/socket_stream.h"
#include "http.h"

namespace fst {
namespace http {

class HttpSession : public SocketStream {
public:
    /// 智能指针类型定义
    typedef std::shared_ptr<HttpSession> ptr;

    HttpSession(Socket::ptr sock, bool owner = true);

    HttpRequest::ptr recvRequest();

    int sendResponse(HttpResponse::ptr rsp);
};

}
}

#endif
