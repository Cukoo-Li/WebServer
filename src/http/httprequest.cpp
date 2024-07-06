#include "httprequest.h"

HttpRequest::HttpRequest() {
    Init();
}

void HttpRequest::Init() {
    method_ = path_ = version_ = body_ = "";
    header_.clear();
    post_.clear();
}

// 解析请求报文
bool HttpRequest::Parse(Buffer &buff) {
    const char CRLF[] = "\r\n";
    
}

bool HttpRequest::IsKeepAlive() const {
    if (header_.count("Connection") == 1) {
        return header_.at("Connection") == "keep-alive" && version_ == "1.1";
    }
    return false;
}
