#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <atomic>

#include "httprequest.h"
#include "httpresponse.h"
#include "../log/log.h"
#include "../pool/sqlconnguard.h"
#include "../buffer/buffer.h"

class HttpConn {
   public:
    HttpConn();

    ~HttpConn();

    void Init(int sockfd, const sockaddr_in& addr);

    ssize_t Read(int* save_errno);

    ssize_t Write(int* save_errno);

    void Close();

    int sockfd() const;

    int port() const;

    const char* ip() const;

    sockaddr_in addr() const;

    bool Process();

    int ToWriteBytes();

    bool IsKeepAlive() const;

    static bool is_et_;
    static const char* kSrcDir_;
    static std::atomic<int> client_count_;

   private:
    int sockfd_;
    sockaddr_in addr_;
    bool is_closed_;
    int iov_cnt_;
    iovec iov_[2];
    Buffer read_buff_;
    Buffer write_buff_;
    HttpRequest request_;
    HttpResponse response_;
};

#endif