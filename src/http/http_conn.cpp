#include "http_conn.h"

#include <mysql/mysql.h>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include "../itc/itc.h"
#include "../pool/db_conn_raii.h"

// 定义http响应的一些状态信息
std::unordered_map<int, std::pair<const char*, const char*>> status_map = {
    {200, {"OK", ""}},
    {400,
     {"Bad Request",
      "Your request has bad syntax or is inherently impossible to staisfy.\n"}},
    {403,
     {"Forbidden",
      "You do not have permission to get file form this server.\n"}},
    {404, {"Not Found", "The requested file was not found on this server.\n"}},
    {500,
     {"Internal Error",
      "There was an unusual problem serving the request file.\n"}}};

const char* root = "/home/Cukoo/Documents/WebServer/docs";

std::unordered_map<char, const char*> resource_map = {{'0', "/register.html"},
                                                      {'1', "/log.html"},
                                                      {'5', "/picture.html"},
                                                      {'6', "/video.html"},
                                                      {'7', "/fans.html"}};

// 将表中的用户名和密码放入map
std::unordered_map<std::string, std::string> account_map;
Locker account_map_locker;

// 这个应该设置成静态函数
void HttpConn::LoadAccounts(DbConnPool* sql_conn_pool) {
    // 先从连接池中取一个连接
    MYSQL* mysql = nullptr;
    DbConnRAII mysql_conn(&mysql, sql_conn_pool);

    // 查询user表
    if (mysql_query(mysql, "SELECT user,passwd FROM account")) {
        printf("SELECT error:%s\n", mysql_error(mysql));
    }
    MYSQL_RES* result = mysql_store_result(mysql);

    // 存入共享的哈希表中
    while (MYSQL_ROW row = mysql_fetch_row(result))
        account_map[std::string(row[0])] = std::string(row[1]);
}

// 将fd设置为非阻塞的
int SetNonBlocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 往epoll内核事件表中注册fd上的读事件
void AddFd(int epollfd, int fd, bool one_shot) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN || EPOLLET || EPOLLRDHUP;
    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    SetNonBlocking(fd);
}

// 删除文件描述符
void RemoveFd(int epollfd, int fd) {
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
    sockfd_ = sockfd;
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
    db_conn_ = nullptr;
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
    memset(file_path_, '\0', kFileNameLen);
}

// 循环读取客户数据，直到无数据可读或者对方关闭连接
bool HttpConn::Read() {
    if (read_buf_end_ >= kReadBufferSize)
        return false;
    int bytes_read = 0;
    while (true) {
        bytes_read = recv(sockfd_, &read_buf_[read_buf_end_],
                          kReadBufferSize - read_buf_end_, 0);
        if (bytes_read == -1) {
            // 无数据可读了
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            // 发生错误
            printf("errno: %d", errno);
            return false;
        }
        // 对方已关闭连接
        else if (bytes_read == 0)
            return false;
        read_buf_end_ += bytes_read;
    }
    return true;
}

// 从状态机，用于分析出一行的内容
HttpConn::LINE_STATUS HttpConn::ParseLine() {
    char cur;
    // 分析[checked_idx_, read_buf_end_)部分的字节
    for (; checked_idx_ < read_buf_end_; ++checked_idx_) {
        cur = read_buf_[checked_idx_];
        // 当前字符是回车符，可能读取到一个完整的行
        if (cur == '\r') {
            // 读到末尾，需要继续读客户数据
            if (checked_idx_ + 1 == read_buf_end_) {
                return LINE_MORE;
            }
            // 下一个字符是换行符，成功读取一个完整的行
            else if (read_buf_[checked_idx_ + 1] == '\n') {
                read_buf_[checked_idx_++] = '\0';
                read_buf_[checked_idx_++] = '\0';
                return LINE_OK;
            }
            // 否则就是语法错误
            return LINE_BAD;
        }
        // 当前字符是换行符，也可能读取到一个完整的行
        else if (cur == '\n') {
            if (checked_idx_ > 1 && read_buf_[checked_idx_ - 1] == '\r') {
                read_buf_[checked_idx_ - 1] = '\0';
                read_buf_[checked_idx_++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    printf("LINE_MORE");
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
            check_state_ = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    } else if (strncasecmp(text, "Connection:", 11) == 0) {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0) {
            printf("keep-alive\n");
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

// 解析HTTP请求的入口函数
// 主线程每进行一次读操作后，某个工作线程被唤醒执行该函数，处理读取的数据
// 返回HTTP请求处理结果
HttpConn::HTTP_CODE HttpConn::ProcessRead() {
    LINE_STATUS line_status = LINE_OK;  // 当前行读取状态
    HTTP_CODE ret = NO_REQUEST;         // HTTP请求处理结果
    char* text = nullptr;
    // 主状态机调用从状态机
    // 每成功取出一个完整的行，就处理之
    // 当且仅当从状态机返回LINE_OK，主状态机才会根据当前所处的状态进行相应操作
    while ((check_state_ == CHECK_STATE_CONTENT && line_status == LINE_OK) ||
           ((line_status = ParseLine()) == LINE_OK)) {
        text = GetLine();
        start_line_ = checked_idx_;
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
HttpConn::HTTP_CODE HttpConn::DoRequest() {
    strcpy(file_path_, root);
    int root_len = strlen(root);
    char flag = strrchr(url_, '/')[1];

    // 处理cgi
    // 实现登录和注册校验
    if (cgi == 1 && (flag == '2' || flag == '3')) {
        strcat(file_path_, "/");
        strcat(file_path_, &url_[2]);

        // 将用户名和密码提取出来
        // user=123&password=123
        char* delimiter = strchr(request_content_, '&');
        std::string user(&request_content_[5],
                         delimiter - &request_content_[5]);
        std::string passwd(delimiter + 10);

        // 注册
        if (flag == '3') {
            // 没有重名
            if (account_map.find(user) == account_map.end()) {
                std::string sql_insert =
                    "INSERT INTO account(user, passwd) VALUES('" + user +
                    "', '" + passwd + "')";
                account_map_locker.Lock();
                int ret = mysql_query(db_conn_, sql_insert.c_str());
                account_map.insert({user, passwd});
                account_map_locker.Unlock();
                if (ret == 0)
                    strcpy(url_, "/log.html");
                else {
                    std::cout << "ret = " << ret << std::endl;
                    strcpy(url_, "/registerError.html");
                }
            }
            // 重名
            else
                strcpy(url_, "/registerError.html");
        }

        // 登录
        if (flag == '2') {
            // 用户名存在且密码正确
            if (account_map.find(user) != account_map.end() &&
                account_map[user] == passwd)
                strcpy(url_, "/welcome.html");
            // 用户名不存在或密码错误
            else
                strcpy(url_, "/logError.html");
        }
    }

    if (resource_map.find(flag) != resource_map.end())
        strcpy(&file_path_[root_len], resource_map[flag]);
    else
        strcpy(&file_path_[root_len], url_);

    // 检查文件状态
    if (stat(file_path_, &file_stat_) < 0)
        // return NO_RESOURCE;
        return BAD_REQUEST;
    if (!(file_stat_.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;
    if (S_ISDIR(file_stat_.st_mode))
        return BAD_REQUEST;

    // 将文件映射到内存中
    int fd = open(file_path_, O_RDONLY);
    file_address_ = static_cast<char*>(
        mmap(0, file_stat_.st_size, PROT_READ, MAP_PRIVATE, fd, 0));
    close(fd);
    return FILE_REQUEST;
}

// 对内存映射区执行mupmap操作
void HttpConn::Unmap() {
    if (file_address_) {
        munmap(file_address_, file_stat_.st_size);
        file_address_ = nullptr;
    }
}

// 写HTTP响应报文
bool HttpConn::Write() {
    int byte_count = 0;

    // 一般不会出现这种情况
    if (bytes_to_send_ == 0) {
        ResetOneShot(epollfd_, sockfd_, EPOLLIN);
        Init();
        return true;
    }

    // ET模式下，一直写，直至文件描述符的写缓冲区满
    while (true) {
        byte_count = writev(sockfd_, iv_, iv_count_);
        if (byte_count == -1) {
            // 无空位可写令
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                ResetOneShot(epollfd_, sockfd_, EPOLLOUT);
                break;
            }
            // 发生错误
            Unmap();
            return false;
        }

        bytes_have_send_ += byte_count;
        bytes_to_send_ -= byte_count;
        // 这里作者是不是写错了
        // if (bytes_have_send_ >= iv_[0].iov_len) {
        if (bytes_have_send_ >= write_buf_end_) {
            iv_[0].iov_len = 0;
            if (iv_count_ == 2) {
                iv_[1].iov_base =
                    file_address_ + (bytes_have_send_ - write_buf_end_);
                iv_[1].iov_len = bytes_to_send_;
            }
        } else {
            iv_[0].iov_base = write_buf_ + bytes_have_send_;
            iv_[0].iov_len -= byte_count;
        }

        // 数据发送完毕
        if (bytes_to_send_ <= 0) {
            printf("数据发送完毕.\n");
            Unmap();
            ResetOneShot(epollfd_, sockfd_, EPOLLIN);
            if (linger_) {
                Init();
                return true;
            } else {
                return false;
            }
        }
    }
    return true;
}

// 往写缓冲区中写入待发送的数据
bool HttpConn::FillWriteBuffer(const char* format, ...) {
    if (write_buf_end_ >= kWriteBufferSize)
        return false;
    va_list args;
    va_start(args, format);
    int bytes_write =
        vsnprintf(&write_buf_[write_buf_end_],
                  kWriteBufferSize - 1 - write_buf_end_, format, args);
    if (bytes_write >= kWriteBufferSize - 1 - write_buf_end_) {
        va_end(args);
        return false;
    }
    write_buf_end_ += bytes_write;
    va_end(args);
    return true;
}

bool HttpConn::AddStatusLine(int status, const char* phrase) {
    return FillWriteBuffer("%s %d %s\r\n", "HTTP/1.1", status, phrase);
}

bool HttpConn::AddContentLength(int content_length) {
    return FillWriteBuffer("Content-Length:%d\r\n", content_length);
}

bool HttpConn::AddLinger() {
    return FillWriteBuffer("Connection:%s\r\n",
                           linger_ == true ? "keep-alive" : "close");
}

bool HttpConn::AddBlankLine() {
    return FillWriteBuffer("%s", "\r\n");
}

bool HttpConn::AddContent(const char* content) {
    return FillWriteBuffer("%s", content);
}

// 根据HTTP请求的处理结果，决定返回给客户的内容
bool HttpConn::ProcessWrite(HTTP_CODE ret) {
    switch (ret) {
        case FILE_REQUEST:
            printf("need file: %s\n", file_path_);
            AddStatusLine(ret, status_map[ret].first);
            // 文件非空，需要发送
            if (file_stat_.st_size != 0) {
                AddContentLength(file_stat_.st_size);
                AddLinger();
                AddBlankLine();
                iv_[0].iov_base = write_buf_;
                iv_[0].iov_len = write_buf_end_;
                iv_[1].iov_base = file_address_;
                iv_[1].iov_len = file_stat_.st_size;
                iv_count_ = 2;
                bytes_to_send_ = write_buf_end_ + file_stat_.st_size;
                return true;
            }
            // 文件为空，没必要发送
            else {
                const char* ok_string = "<html><body></body></html>";
                AddContentLength(strlen(ok_string));
                AddLinger();
                AddBlankLine();
                if (!AddContent(ok_string))
                    return false;
            }
            break;
        // 这三种情况都不用发送文件内容
        case INTERNAL_ERROR:
        case BAD_REQUEST:
        case FORBIDDEN_REQUEST:
            AddStatusLine(ret, status_map[ret].first);
            AddContentLength(strlen(status_map[ret].second));
            AddLinger();
            AddBlankLine();
            if (!AddContent(status_map[ret].second))
                return false;
            printf("no need file\n");
            break;
        default:
            return false;
    }
    iv_[0].iov_base = write_buf_;
    iv_[0].iov_len = write_buf_end_;
    iv_count_ = 1;
    bytes_to_send_ = write_buf_end_;
    return true;
}

// 由工作线程调用，这是处理HTTP请求的入口函数
void HttpConn::Process() {
    HTTP_CODE read_ret = ProcessRead();
    if (read_ret == NO_REQUEST) {
        ResetOneShot(epollfd_, sockfd_, EPOLLIN);
        return;
    }
    bool write_ret = ProcessWrite(read_ret);
    if (!write_ret)
        CloseConn();
    ResetOneShot(epollfd_, sockfd_, EPOLLOUT);
}