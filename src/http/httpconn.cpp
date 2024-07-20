// Author: Cukoo
// Date: 2024-07-04

#include "http/httpconn.h"

std::string HttpConn::kWorkDir_;
std::atomic<int> HttpConn::client_count_{};

HttpConn::HttpConn() {
    sockfd_ = -1;
    addr_ = {};
    is_closed_ = true;
}

HttpConn::~HttpConn() {
    Close();
}

void HttpConn::Init(int sockfd, const sockaddr_in& addr) {
    assert(sockfd > 0);
    if (is_closed_) {
        ++client_count_;
    }
    addr_ = addr;
    sockfd_ = sockfd;
    write_buff_.RetrieveAll();
    read_buff_.RetrieveAll();
    is_closed_ = false;
    spdlog::info("Client[{}]({}:{}) init. \t[client count:{}]", sockfd_, ip(), port(), client_count_);
}

void HttpConn::Close() {
    if (is_closed_) {
        return;
    }
    is_closed_ = true;
    --client_count_;
    close(sockfd_);
    spdlog::info("Client[{}]({}:{}) quit. \t[client count:{}]", sockfd_, ip(), port(), client_count_);
}

int HttpConn::sockfd() const {
    return sockfd_;
}

sockaddr_in HttpConn::addr() const {
    return addr_;
}

const char* HttpConn::ip() const {
    return inet_ntoa(addr_.sin_addr);
}

int HttpConn::port() const {
    return addr_.sin_port;
}

//
ssize_t HttpConn::Read(int* save_errno) {
    ssize_t len = -1;
    // ET 模式下需要读到不能再读
    while (true) {
        len = read_buff_.ReadFd(sockfd_, save_errno);
        if (len <= 0) {
            break;
        }
    }
    return len;
}

//
ssize_t HttpConn::Write(int* save_errno) {
    ssize_t len = -1;
    // ET 模式下需要写到不能再写
    while (true) {
        len = writev(sockfd_, iov_, iov_cnt_);
        if (len == -1) {
            // 写到不能再写了（发送缓冲区已满）
            *save_errno = errno;
            break;
        }
        if (iov_[0].iov_len + iov_[1].iov_len == 0) {
            // 写完了
            break;
        }
        // 响应头中的字节已经写入完毕
        else if (static_cast<size_t>(len) > iov_[0].iov_len) {
            iov_[1].iov_base = (uint8_t*)iov_[1].iov_base +
                               (len - iov_[0].iov_len);  // char* 也可以吧？
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            if (iov_[0].iov_len) {
                write_buff_.RetrieveAll();
                iov_[0].iov_len = 0;
            }
        }
        // 响应头中的字节尚未写入完毕
        else {
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len;
            iov_[0].iov_len -= len;
            write_buff_.Retrieve(len);
        }
    }
    return len;
}

//
bool HttpConn::Process() {
    // 准备下一次请求
    if (request_.state() == HttpRequest::ParseState::FINISH) {
        request_.Init();
    }
    if (read_buff_.ReadableBytes() <= 0) {
        return false;
    }
    HttpRequest::ParseResult http_code = request_.Parse(read_buff_);
    // 已接收到完整的请求报文
    if (http_code == HttpRequest::ParseResult::INCOMPLETE) {
        spdlog::debug("Client[{}]({}:{})  request url: {}", sockfd_, ip(), port(), request_.url());
        response_.Init(kWorkDir_, request_.url(), request_.IsKeepAlive(), 200);
    }
    // 请求报文不完整
    else if (http_code == HttpRequest::ParseResult::COMPLETE) {
        return false;
    }
    // 请求报文解析发生错误
    else {
        response_.Init(kWorkDir_, request_.url(), false, 400);
    }

    // 响应头
    response_.MakeResponse(write_buff_);
    iov_[0].iov_base = const_cast<char*>(write_buff_.ReadBegin());
    iov_[0].iov_len = write_buff_.ReadableBytes();
    iov_cnt_ = 1;

    // 文件（响应体）
    if (response_.file_size() > 0 && response_.file_addr()) {
        iov_[1].iov_base = response_.file_addr();
        iov_[1].iov_len = response_.file_size();
        iov_cnt_ = 2;
    }
    spdlog::debug("Client[{}]({}:{})  filesize: {}, iov_cnt: {},  ToWriteBytes: {}", sockfd_, ip(), port(),response_.file_size(), iov_cnt_, ToWriteBytes());
    return true;
}

int HttpConn::ToWriteBytes() {
    return iov_[0].iov_len + iov_[1].iov_len;
}

bool HttpConn::IsKeepAlive() const {
    return request_.IsKeepAlive();
}
