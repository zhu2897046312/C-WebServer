
#ifndef __FANSHUTOU_HTTP_PARSER_H__
#define __FANSHUTOU_HTTP_PARSER_H__

#include "http.h"
#include "http11_parser.h"
#include "httpclient_parser.h"

namespace fst {
namespace http {

class HttpRequestParser {
public:
    /// HTTP解析类的智能指针
    typedef std::shared_ptr<HttpRequestParser> ptr;

    HttpRequestParser();

    size_t execute(char* data, size_t len);

    int isFinished();

    int hasError(); 

    HttpRequest::ptr getData() const { return m_data;}

    void setError(int v) { m_error = v;}

    uint64_t getContentLength();

    const http_parser& getParser() const { return m_parser;}
public:
    static uint64_t GetHttpRequestBufferSize();

    static uint64_t GetHttpRequestMaxBodySize();
private:
    /// http_parser
    http_parser m_parser;
    /// HttpRequest结构
    HttpRequest::ptr m_data;
    /// 错误码
    /// 1000: invalid method
    /// 1001: invalid version
    /// 1002: invalid field
    int m_error;
};

class HttpResponseParser {
public:
    /// 智能指针类型
    typedef std::shared_ptr<HttpResponseParser> ptr;

    HttpResponseParser();

    size_t execute(char* data, size_t len, bool chunck);

    int isFinished();

    int hasError(); 

    HttpResponse::ptr getData() const { return m_data;}

    void setError(int v) { m_error = v;}

    uint64_t getContentLength();

    const httpclient_parser& getParser() const { return m_parser;}
public:
    static uint64_t GetHttpResponseBufferSize();

    static uint64_t GetHttpResponseMaxBodySize();
private:
    /// httpclient_parser
    httpclient_parser m_parser;
    /// HttpResponse
    HttpResponse::ptr m_data;
    /// 错误码
    /// 1001: invalid version
    /// 1002: invalid field
    int m_error;
};

}
}

#endif
