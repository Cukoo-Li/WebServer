#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>

#include "../pool/db_conn_pool.h"
#include "../timer/timer_pile.h"

class HttpConn {
   public:
    static const int kFileNameLen = 200;
    static const int kReadBufferSize = 2048;
    static const int kWriteBufferSize = 1024;
    // HTTP请求方法
    enum METHOD { GET, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATH };
    // 解析HTTP请求时，主状态机所处的状态
    enum CHECK_STATE {
        CHECK_STATE_REQUESTLINE,  // 分析请求行
        CHECK_STATE_HEADER,       // 分析请求头
        CHECK_STATE_CONTENT       // 分析请求体
    };
    // 解析HTTP请求时，从状态机所处的状态
    enum LINE_STATUS {
        LINE_OK,   // 读取到一个完整的行
        LINE_BAD,  // 行出错
        LINE_MORE  // 行数据尚不完整
    };
    // 处理HTTP请求的可能结果
    enum HTTP_CODE {
        NO_REQUEST,         // 请求不完整，需要继续读取客户数据
        GET_REQUEST,        // 获得了一个完整的客户请求
        BAD_REQUEST = 404,  // 客户请求有语法错误或者请求资源不存在
        FORBIDDEN_REQUEST = 403,  // 客户对请求资源没有访问权限
        FILE_REQUEST = 200,       // 请求资源可以正常访问
        INTERNAL_ERROR = 500,     // 服务器内部错误
    };

   public:
    HttpConn() {}
    ~HttpConn() {}

   public:
    void set_timer(Timer* timer) { timer_ = timer; }
    Timer* timer() { return timer_; }
    int sockfd() { return sockfd_; }

   public:
    // 初始化新接受的连接
    void Init(int sockfd, const sockaddr_in& addr);
    // 关闭连接
    void CloseConn(bool real_close = true);
    // 处理客户请求（由工作线程调用）
    void Process();
    // 非阻塞读操作（由主线程调用）
    bool Read();
    // 非阻塞写操作（由主线程调用）
    bool Write();

    sockaddr_in* address() { return &address_; }
    static void LoadAccounts(DbConnPool* sql_conn_pool);

   private:
    // 初始化连接
    void Init();
    // 解析HTTP请求
    HTTP_CODE ProcessRead();
    // 填充HTTP应答
    bool ProcessWrite(HTTP_CODE ret);

    // 下面这一组函数被ProcessRead调用以分析HTTP请求
    char* GetLine() { return read_buf_ + start_line_; }
    LINE_STATUS ParseLine();
    HTTP_CODE ParseRequestLine(char* text);
    HTTP_CODE ParseHeaders(char* text);
    HTTP_CODE ParseContent(char* text);
    HTTP_CODE DoRequest();

    // 下面这一组函数被ProcessWrite调用以填充HTTP应答
    void Unmap();
    bool FillWriteBuffer(const char* format, ...);
    bool AddStatusLine(int status, const char* phrase);
    bool AddContent(const char* content);
    bool AddContentLength(int content_length);
    bool AddLinger();
    bool AddBlankLine();

   public:
    // 所有socket上的事件都被注册到同一个epoll内核事件表中
    static int epollfd_;
    // 统计客户数量
    static int client_count_;
    // 数据库连接
    MYSQL* db_conn_;

   private:
    // 该http连接的socket和对方的socket地址
    int sockfd_;
    sockaddr_in address_;
    Timer* timer_;

    // 读缓冲区
    char read_buf_[kReadBufferSize];
    // 标识读缓冲区中已经读入的客户数据的最后一个字节的下一个位置
    int read_buf_end_;
    // 当前正在分析的字符在读缓冲区中的位置
    int checked_idx_;
    // 当前正在解析的行的起始位置
    int start_line_;
    // 写缓冲区
    char write_buf_[kWriteBufferSize];
    // 标识写缓冲区中最后一个字节的下一个位置（写缓冲区中待发送的字节数）
    int write_buf_end_;

    // 主状态机当前所处的状态
    CHECK_STATE check_state_;
    METHOD method_;

    // 客户请求的目标文件的完整路径
    char file_path_[kFileNameLen];
    // 客户请求的目标文件名
    char* url_;
    // HTTP协议版本
    char* version_;
    // 主机名
    char* host_;
    // HTTP请求报文段中消息体的长度
    int content_length_;
    // HTTP请求是否要求保持连接
    bool linger_;
    // 客户请求的目标文件被mmap到内存的起始位置
    char* file_address_;
    // 目标文件的状态
    struct stat file_stat_;
    // 我们将采用writev来执行写操作，所以定义以下两个成员，其中iv_count_标识被写内存块的数量
    struct iovec iv_[2];
    int iv_count_;

    int cgi;                 // 是否需要执行cgi
    char* request_content_;  // 请求体
    int bytes_to_send_;      // 需要发送的字节数
    int bytes_have_send_;    // 已经发送的字节数
};

#endif