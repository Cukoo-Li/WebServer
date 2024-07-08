#include "webserver.h"

int WebServer::SetFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}

WebServer::WebServer(Config config)
    : kWorkDir_(config.work_dir),
      kPort_(config.port),
      kEnableLinger_(config.enable_linger),
      kTimeout_(config.timeout) {
    is_closed_ = false;
    timer_heap_.reset(new TimerHeap());
    thread_pool_.reset(new ThreadPool(config.thread_pool_size));
    epoller_.reset(new Epoller);
    SqlConnPool::Instance()->Init(config.host, config.sql_port, config.sql_user,
                                  config.sql_pwd, config.db_name,
                                  config.sql_conn_pool_size);
    listenfd_event_ = EPOLLET | EPOLLRDHUP;
    connfd_event_ = EPOLLONESHOT | EPOLLET | EPOLLRDHUP;
    if (!InitListenSocket()) {
        is_closed_ = true;
    }

    // if (config.enable_log) {
    //             Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
    //     if(isClose_) { LOG_ERROR("========== Server init error!==========");
    //     } else {
    //         LOG_INFO("========== Server init ==========");
    //         LOG_INFO("Port:%d, OpenLinger: %s", port_, OptLinger?
    //         "true":"false"); LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
    //                         (listenEvent_ & EPOLLET ? "ET": "LT"),
    //                         (connEvent_ & EPOLLET ? "ET": "LT"));
    //         LOG_INFO("LogSys level: %d", logLevel);
    //         LOG_INFO("srcDir: %s", HttpConn::srcDir);
    //         LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum,
    //         threadNum);
    //     }
    // }
}

WebServer::~WebServer() {
    close(listenfd_);
    is_closed_ = true;
}

void WebServer::Startup() {
    int timeout = -1;
    while (!is_closed_) {
        if (kTimeout_ > 0) {
            timeout = timer_heap_->GetNextTick();
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
                // LOG_ERROR("Unexpected event");
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
    // LOG_INFO("Server port:%d", port_);
    return true;
}

void WebServer::SendError(int fd, const char* message) {
    assert(fd >= 0);
    int ret = send(fd, message, strlen(message), 0);
    if (ret < 0) {
        // LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

void WebServer::CloseConn(HttpConn* client) {
    assert(client);
    // LOG_INFO("Client[%d] quit!", client->sockfd());
    epoller_->Remove(client->sockfd());
    client->Close();
}

void WebServer::HandleListenFdEvent() {
    sockaddr_in addr{};
    socklen_t addr_len = sizeof(addr);
    while (true) {
        int fd = accept(listenfd_, (sockaddr*)&addr, &addr_len);
        if (fd < 0) {
            // listenfd_ 不可读了，此时 errno == EAGAIN || errno == EWOULDBLOCK
            return;
        }
        if (HttpConn::client_count_ >= kMaxFd_) {
            SendError(fd, "Server is busy!");
            // LOG_WARN("Clients is full!");
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
        // LOG_INFO("Client[%d] in!", clients_[fd].sockfd());
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

void WebServer::OnRead(HttpConn* client) {
    assert(client);
    int ret = -1;
    int read_errno = 0;
    ret = client->Read(&read_errno);
    if (ret < 0 && read_errno != EAGAIN) {
        CloseConn(client);
        return;
    }
    OnProcess(client);
}

void WebServer::OnProcess(HttpConn* client) {
    if (client->Process()) {
        epoller_->Modify(client->sockfd(), connfd_event_ | EPOLLOUT);
    } else {
        epoller_->Modify(client->sockfd(), connfd_event_ | EPOLLIN);
    }
}

void WebServer::OnWrite(HttpConn* client) {
    assert(client);
    int ret = -1;
    int write_errno = 0;
    ret = client->Write(&write_errno);
    if (client->ToWriteBytes() == 0 && client->IsKeepAlive()) {
        // 传输完成，但需要保持连接
        OnProcess(client);  // 实际上只是清空了 HttpRequst 对象
        return;
    }
    if (ret == -1 && write_errno == EAGAIN) {
        // 未传输完成，需要等下一次可写
        epoller_->Modify(client->sockfd(), connfd_event_ || EPOLLOUT);
        return;
    }
    CloseConn(client);
}