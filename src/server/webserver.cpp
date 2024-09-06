// Author: Cukoo
// Date: 2024-07-02

#include "server/webserver.h"

int WebServer::SetFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}

WebServer::WebServer(Config config)
    : kWorkDir_(config.work_dir),
      kPort_(config.port),
      kEnableLinger_(config.enable_linger),
      kTimeout_(config.timeout) {
    spdlog::set_level(config.log_level);
    HttpConn::kWorkDir_ = kWorkDir_;
    is_closed_ = false;

    // 初始化一系列资源
    // 定时器容器、线程池、epoll 内核事件表、数据库连接池、监听 socket
    timer_heap_.reset(new TimerHeap());
    thread_pool_.reset(new ThreadPool(config.thread_pool_size));
    epoller_.reset(new Epoller(65535));
    SqlConnPool::Instance()->Init(config.host, config.sql_port, config.sql_user,
                                  config.sql_pwd, config.db_name,
                                  config.sql_conn_pool_size);
    listenfd_event_ = EPOLLET | EPOLLRDHUP;
    connfd_event_ = EPOLLONESHOT | EPOLLET | EPOLLRDHUP;
    if (!InitListenSocket()) {
        is_closed_ = true;
    }

    spdlog::info("========== Server init ==========");
    spdlog::info("Port: {}, Enable Linger: {}", kPort_,
                 kEnableLinger_ ? "true" : "false");
    spdlog::info("Work Directory: {}", kWorkDir_);
    spdlog::info("SQL Connection Pool Size: {}", config.sql_conn_pool_size);
    spdlog::info("Thread Pool Size: {}", config.thread_pool_size);
}

WebServer::~WebServer() {
    close(listenfd_);
    is_closed_ = true;
}

void WebServer::Startup() {
    int timeout = -1;
    while (!is_closed_) {
        if (kTimeout_ > 0) {
            timeout = timer_heap_->Tick();
        }
        int event_cnt = epoller_->Wait(timeout);
        for (int i = 0; i < event_cnt; ++i) {
            // 处理事件
            int fd = epoller_->GetEventFd(i);
            uint32_t events = epoller_->GetEvents(i);
            // 处理新客户的连接请求
            if (fd == listenfd_) {
                HandleListenFdEvent();
            }
            // 处理 connfd 上的错误
            else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(clients_.count(fd));
                CloseConn(&clients_[fd]);
            }
            // 处理 connfd 上的可读事件
            else if (events & EPOLLIN) {
                assert(clients_.count(fd));
                HandleReadableEvent(&clients_[fd]);
            }
            // 处理 connfd 上的可写事件
            else if (events & EPOLLOUT) {
                assert(clients_.count(fd));
                HandleWritableEvent(&clients_[fd]);
            }
            // 未定义事件
            else {
                spdlog::error("Unexpected event");
            }
        }
    }
}

// 创建 listenfd_
bool WebServer::InitListenSocket() {
    // 1. 创建 socket
    listenfd_ = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd_ >= 0);

    // 2. 命名 socket
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(kPort_);
    // 优雅关闭：直到所剩数据发送完毕或超时
    struct linger opt_linger {};
    if (kEnableLinger_) {
        opt_linger.l_onoff = 1;
        opt_linger.l_linger = 1;
    }
    int ret;
    ret = setsockopt(listenfd_, SOL_SOCKET, SO_LINGER, &opt_linger,
                     sizeof(opt_linger));
    assert(ret >= 0);
    // 端口复用
    int opt_reuse = 1;
    ret = setsockopt(listenfd_, SOL_SOCKET, SO_REUSEADDR, &opt_reuse,
                     sizeof(opt_reuse));
    assert(ret >= 0);
    ret = bind(listenfd_, (sockaddr*)&addr, sizeof(addr));
    assert(ret >= 0);

    // 3. 监听 socket
    ret = listen(listenfd_, 5);
    assert(ret >= 0);

    // 注册事件
    ret = epoller_->Add(listenfd_, listenfd_event_ | EPOLLIN);
    assert(ret == 1);

    SetFdNonblock(listenfd_);
    spdlog::info("Server port: {}", kPort_);
    return true;
}

void WebServer::SendError(int fd, const char* message) {
    assert(fd >= 0);
    int ret = send(fd, message, strlen(message), 0);
    if (ret < 0) {
        spdlog::error("send error to client[{}] error!", fd);
    }
    close(fd);
}

void WebServer::CloseConn(HttpConn* client) {
    assert(client);
    epoller_->Remove(client->sockfd());
    client->Close();
}

void WebServer::HandleListenFdEvent() {
    sockaddr_in addr{};
    socklen_t addr_len = sizeof(addr);
    while (true) {
        int fd = accept(listenfd_, (sockaddr*)&addr, &addr_len);
        // listenfd_ 不可读了，此时 errno == EAGAIN || errno == EWOULDBLOCK
        if (fd == -1) {
            if (!(errno == EAGAIN || errno == EWOULDBLOCK)) {
                spdlog::error("accept occur unknown error.");
            }
            return;
        }
        if (HttpConn::client_count_ >= kMaxFd_) {
            SendError(fd, "Server is busy!");
            spdlog::warn("server is full!");
            return;
        }
        // 创建 HttpConn
        clients_[fd].Init(fd, addr);
        // 创建定时器
        if (kTimeout_ > 0) {
            timer_heap_->Add(
                fd, kTimeout_,
                std::bind(&WebServer::CloseConn, this, &clients_[fd]));
        }
        // 注册事件
        epoller_->Add(fd, EPOLLIN | connfd_event_);
        SetFdNonblock(fd);
        spdlog::info("Client[{}]({}:{}) enter. \t[client count:{}]",
                     clients_[fd].sockfd(), clients_[fd].ip(),
                     clients_[fd].port(), HttpConn::client_count_);
    }
}

void WebServer::HandleReadableEvent(HttpConn* client) {
    assert(client);
    ResetTimer(client);
    thread_pool_->AddTask(std::bind(&WebServer::OnRead, this, client));
}

void WebServer::HandleWritableEvent(HttpConn* client) {
    assert(client);
    ResetTimer(client);
    thread_pool_->AddTask(std::bind(&WebServer::OnWrite, this, client));
}

void WebServer::ResetTimer(HttpConn* client) {
    assert(client);
    if (kTimeout_ > 0) {
        timer_heap_->Adjust(client->sockfd(), kTimeout_);
    }
}

// 由工作线程负责执行
// 从 connfd 中读数据
void WebServer::OnRead(HttpConn* client) {
    assert(client);
    int ret = -1;
    int read_errno = 0;
    ret = client->Read(&read_errno);
    // 发生错误
    if (ret == -1 && read_errno != EAGAIN) {
        CloseConn(client);
        return;
    }
    // 对方已关闭连接
    if (ret == 0) {
        CloseConn(client);
        return;
    }
    // 处理（解析）刚刚读到的数据
    OnProcess(client);
}

// 由工作线程负责执行
// 紧跟在 OnRead 之后执行
// 处理实际包含两个部分工作：解析请求报文、生成响应报文内容（并将其填充到写缓冲区中）
void WebServer::OnProcess(HttpConn* client) {
    if (client->Process()) {
        // 处理完成，响应报文已经准备完毕（响应报文已经被填充到用户读缓冲区中），接下来就只要准备写了
        epoller_->Modify(client->sockfd(), connfd_event_ | EPOLLOUT);
    } else {
        // 未处理完成，说明请求报文不完整，接下来还得继续读
        epoller_->Modify(client->sockfd(), connfd_event_ | EPOLLIN);
    }
}

// 由工作线程负责执行
// 往 connfd 中写数据
void WebServer::OnWrite(HttpConn* client) {
    assert(client);
    int ret = -1;
    int write_errno = 0;
    ret = client->Write(&write_errno);
    if (client->ToWriteBytes() == 0 && client->IsKeepAlive()) {
        // 传输完成，但需要保持连接
        OnProcess(client);  // 这个调用的效果实际上只是重新初始化了 HttpRequst 对象
        return;
    }
    if (ret == -1 && write_errno == EAGAIN) {
        // 未传输完成，需要等下一次可写
        epoller_->Modify(client->sockfd(), connfd_event_ | EPOLLOUT);
        return;
    }
    // 非预期情况，关闭连接
    CloseConn(client);
}
