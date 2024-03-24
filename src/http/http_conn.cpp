#include "http_conn.h"
// #include "../log/log.h"
#include <mysql/mysql.h>
#include <fstream>
#include <map>

// 定义http响应的一些状态信息
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form =
    "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form =
    "You do not have permission to get file form this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form =
    "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form =
    "There was an unusual problem serving the request file.\n";

const char* root = "/home/Cukoo/Documents/WebServer/docs/";

// //将表中的用户名和密码放入map
// map<string, string> users;
// locker m_lock;

// void http_conn::initmysql_result(connection_pool *connPool)
// {
//     //先从连接池中取一个连接
//     MYSQL *mysql = NULL;
//     connectionRAII mysqlcon(&mysql, connPool);

//     //在user表中检索username，passwd数据，浏览器端输入
//     if (mysql_query(mysql, "SELECT username,passwd FROM user"))
//     {
//         LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
//     }

//     //从表中检索完整的结果集
//     MYSQL_RES *result = mysql_store_result(mysql);

//     //返回结果集中的列数
//     int num_fields = mysql_num_fields(result);

//     //返回所有字段结构的数组
//     MYSQL_FIELD *fields = mysql_fetch_fields(result);

//     //从结果集中获取下一行，将对应的用户名和密码，存入map中
//     while (MYSQL_ROW row = mysql_fetch_row(result))
//     {
//         string temp1(row[0]);
//         string temp2(row[1]);
//         users[temp1] = temp2;
//     }
// }

// 将fd设置为非阻塞的
int SetNonBlocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 往epoll内核事件表中注册fd上的读事件
int AddFd(int epollfd, int fd, bool one_shot) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN || EPOLLET || EPOLLRDHUP;
    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    SetNonBlocking(fd);
}

// 删除文件描述符
int RemoveFd(int epollfd, int fd) {
    // 从内核事件表中删除有关fd的事件
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
}

// 重置EPOLLONESHOT
void ResetOneShot(int epollfd, int fd, int ev) {
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

int HttpConn::client_count_ = 0;
int HttpConn::epollfd_ = -1;

// 初始化新接受的连接
void HttpConn::Init(int sockfd, const sockaddr_in& address) {
    sockfd_ = sockfd_;
    address_ = address;
    AddFd(epollfd_, sockfd, true);
    ++client_count_;
    Init();
}

// 关闭连接
void HttpConn::CloseConn(bool real_close) {
    if (real_close && sockfd_ != -1) {
        RemoveFd(epollfd_, sockfd_);
        sockfd_ = -1;
        --client_count_;
    }
}

// 初始化连接
void HttpConn::Init() {
    sql = nullptr;
    bytes_to_send_ = 0;
    bytes_have_send_ = 0;
    check_state_ = CHECK_STATE_REQUESTLINE;
    linger_ = false;
    method_ = GET;
    url_ = nullptr;
    version_ = nullptr;
    content_length_ = 0;
    host_ = nullptr;
    start_line_ = 0;
    cheked_idx_ = 0;
    read_buf_end_ = 0;
    write_buf_end_ = 0;
    cgi = 0;
    memset(read_buf_, '\0', kReadBufferSize);
    memset(write_buf_, '\0', kWriteBufferSize);
    memset(real_file_, '\0', kFileNameLen);
}

// 从状态机，用于分析出一行的内容
HttpConn::LINE_STATUS HttpConn::ParseLine() {}

// 循环读取客户数据，直到无数据可读或者对方关闭连接
bool HttpConn::Read() {}

// 解析HTTP请求行，获取请求方法、目标URL、HTTP版本号
HttpConn::HTTP_CODE HttpConn::ParseRequentLine(char* text) {}

// 解析HTTP请求头
HttpConn::HTTP_CODE HttpConn::ParseHeaders(char* text) {}

// 没有真正解析HTTP请求体，只是判断它是否被完整读入了
HttpConn::HTTP_CODE HttpConn::ParseContent(char* text) {}

// 主状态机
HttpConn::HTTP_CODE HttpConn::ProcessRead() {}

// 当得到一个完整、正确的HTTP请求时，我们就分析目标文件的属性
// 如果目标文件存在、对所有用户可读，且不是目录，则使用mmap将其映射到内存地址file_address_处，并告诉调用者获取文件成功
HttpConn::HTTP_CODE HttpConn::DoRequest() {}

// 对内存映射区执行mupmap操作
void HttpConn::Unmap() {}

// 写HTTP响应报文
bool HttpConn::Write() {}

// 往写缓冲中写入待发送的数据
bool HttpConn::AddResponse(const char* format, ...) {}

bool HttpConn::AddStatusLine(int status, const char* title) {}
bool HttpConn::AddHeaders(int content_length) {}
bool HttpConn::AddContentLength(int content_length) {}
bool HttpConn::AddContentType() {}
bool HttpConn::AddLinger() {}
bool HttpConn::AddBlankLine() {}
bool HttpConn::AddContent(const char* content) {}

// 根据服务器处理HTTP请求的结果，决定返回给客户的内容
bool HttpConn::ProcessWrite(HTTP_CODE ret) {

}

// 由工作线程调用，这是处理HTTP请求的入口函数
void HttpConn::Process() {

}