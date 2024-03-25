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
    checked_idx_ = 0;
    read_buf_end_ = 0;
    write_buf_end_ = 0;
    cgi = 0;
    memset(read_buf_, '\0', kReadBufferSize);
    memset(write_buf_, '\0', kWriteBufferSize);
    memset(real_file_, '\0', kFileNameLen);
}

// 循环读取客户数据，直到无数据可读或者对方关闭连接
bool HttpConn::Read() {}

// 从状态机，用于分析出一行的内容
HttpConn::LINE_STATUS HttpConn::ParseLine() {
    char cur;
    // 分析[checked_idx_, read_buf_end_)部分的字节
    for (; checked_idx_ < read_buf_end_; ++checked_idx_) {
        cur = read_buf_[checked_idx_];
        // 当前字符是回车符，可能读取到一个完整的行
        if (cur = CR) {
            // 读到末尾，需要继续读客户数据
            if (checked_idx_ + 1 == read_buf_end_) {
                return LINE_MORE;
            }
            // 下一个字符是换行符，成功读取一个完整的行
            else if (read_buf_[checked_idx_ + 1] == LF) {
                read_buf_[checked_idx_++] = '\0';
                read_buf_[checked_idx_++] = '\0';
                return LINE_OK;
            }
            // 否则就是语法错误
            return LINE_BAD;
        }
        // 当前字符是换行符，也可能读取到一个完整的行
        else if (cur == LF) {
            if (checked_idx_ > 1 && read_buf_[checked_idx_ - 1] == CR) {
                read_buf_[checked_idx_ - 1] == '\0';
                read_buf_[checked_idx_++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_MORE;
}

// 解析HTTP请求行，获取请求方法、目标URL、HTTP版本号
HttpConn::HTTP_CODE HttpConn::ParseRequestLine(char* text) {
    // 查找url
    url_ = strpbrk(text, " \t");
    if (!url_) {
        return BAD_REQUEST;
    }

    // 分割method和url
    *url_++ = '\0';

    // 获取方法
    char* method = text;
    if (strcasecmp(method, "GET") == 0)
        method_ = GET;
    else if (strcasecmp(method, "POST") == 0) {
        method_ = POST;
        cgi = 1;
    } else
        return BAD_REQUEST;

    url_ += strspn(url_, " \t");
    version_ = strpbrk(url_, " \t");
    if (!version_)
        return BAD_REQUEST;

    // 获取HTTP版本
    *version_++ = '\0';
    version_ += strspn(version_, " \t");
    if (strcasecmp(version_, "HTTP/1.1") != 0)
        return BAD_REQUEST;

    // 获取url
    if (strncasecmp(url_, "http://", 7) == 0) {
        url_ += 7;
        url_ = strchr(url_, '/');
    }
    if (strncasecmp(url_, "https://", 8) == 0) {
        url_ += 8;
        url_ = strchr(url_, '/');
    }

    if (!url_ || url_[0] != '/')
        return BAD_REQUEST;
    // 当url为/时，显示判断界面
    if (strlen(url_) == 1)
        strcat(url_, "judge.html");
    check_state_ = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

// 解析HTTP请求头
// 一次只会提取到一个字段
HttpConn::HTTP_CODE HttpConn::ParseHeaders(char* text) {
    // 遇到了一个空行
    if (text[0] == '\0') {
        // 检查是否含请求体，有的话继续读
        if (content_length_ != 0) {
            check_state_ == CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    } else if (strncasecmp(text, "Connection:", 11) == 0) {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0) {
            linger_ = true;
        }
    } else if (strncasecmp(text, "Content-length:", 15) == 0) {
        text += 15;
        text += strspn(text, " \t");
        content_length_ = atol(text);
    } else if (strncasecmp(text, "Host:", 5) == 0) {
        text += 5;
        text += strspn(text, " \t");
        host_ = text;
    } else {
        printf("oop!unknow header: %s\n", text);
        // LOG_INFO("oop!unknow header: %s", text);
        // Log::get_instance()->flush();
    }
    return NO_REQUEST;
}

// 解析HTTP请求体
HttpConn::HTTP_CODE HttpConn::ParseContent(char* text) {
    if (checked_idx_ + content_length_ <= read_buf_end_) {
        text[content_length_] = '\0';
        // POST请求中最后为输入的用户名和密码
        request_content_ = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

// 主状态机（解析HTTP请求的入口函数）每进行一次读操作后都会被调用
// 主状态机会调用从状态机
// 当且仅当从状态机返回LINE_OK，主状态机才会根据当前所处的状态进行相应操作
HttpConn::HTTP_CODE HttpConn::ProcessRead() {
    LINE_STATUS line_status = LINE_OK;      // 当前行读取状态
    HTTP_CODE ret = NO_REQUEST;             // HTTP请求处理结果
    char* text = nullptr;
    // 主状态机，调用从状态机
    // 每成功取出一个完整的行，就处理之
    while ((check_state_ == CHECK_STATE_CONTENT && line_status == LINE_OK) ||
           ((line_status = ParseLine()) == LINE_OK)) {
        text = GetLine();
        start_line_ = checked_idx_;
        // LOG_INFO("%s", text);
        // Log::get_instance()->flush();
        switch (check_state_) {
            // 解析请求行
            case CHECK_STATE_REQUESTLINE:
                ret = ParseRequestLine(text);
                if (ret == BAD_REQUEST)
                    return BAD_REQUEST;
                break;
            // 解析请求头
            case CHECK_STATE_HEADER:
                ret = ParseHeaders(text);
                if (ret == BAD_REQUEST)
                    return BAD_REQUEST;
                else if (ret == GET_REQUEST)
                    return DoRequest();
                break;
            // 解析请求体
            case CHECK_STATE_CONTENT:
                ret = ParseContent(text);
                if (ret == GET_REQUEST)
                    return DoRequest();
                line_status = LINE_MORE;
                break;
            default:
                return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}


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
bool HttpConn::ProcessWrite(HTTP_CODE ret) {}

// 由工作线程调用，这是处理HTTP请求的入口函数
void HttpConn::Process() {}