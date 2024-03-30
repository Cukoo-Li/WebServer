#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cassert>

#include "http/http_conn.h"
#include "pool/thread_pool.h"
#include "timer/sort_timer_list.h"
#include "pool/db_conn_pool.h"

const int kMaxFd = 65536;           // 最大文件描述符
const int kMaxEventNumber = 10000;  // 最大事件数

// #define SYNLOG  // 同步写日志
// #define ASYNLOG     // 异步写日志

// 在http_conn.cpp中定义
extern int SetNonBlocking(int fd);
extern int AddFd(int epollfd, int fd, bool one_shot);
extern int RemoveFd(int epollfd, int fd);

int pipefd[2];  // 统一事件源用到的管道
int epollfd;    // epoll内核事件表

// 基于升序链表的定时器
SortTimerList sort_timer_list;
const int kTimingCircle = 5;  // 定时周期

// 信号处理函数（统一事件源）
void SigHandler(int sig) {
    // 保留原来的errno,在函数最后恢复，以保证函数的可重入性
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char*)&msg, 1, 0);  // 将信号值通过管道发送给主循环
    errno = save_errno;
}

// 为信号设置信号处理函数
void AddSig(int sig, void SigHandler(int), bool restart = true) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = SigHandler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);  // 不屏蔽任何信号
    int ret = sigaction(sig, &sa, nullptr);
    assert(ret != -1);
}

// 处理SIGALRM信号（定时事件）
void TimerHandler() {
    sort_timer_list.Tick();
    alarm(kTimingCircle);
}

// 删除指定连接在socket上的注册事件，并关闭该连接
// 用作定时器的回调函数
// 每个连接都对应一个定时器，当定时器超时时，说明该连接长时间不活动，故处理之
void CloseConn(ClientData* client_data) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, client_data->sockfd, 0);
    assert(client_data);
    close(client_data->sockfd);
    --HttpConn::client_count_;
    // LOG_INFO("close fd %d", user_data->sockfd);
    // Log::get_instance()->flush();
}

void ShowError(int connfd, const char* info) {
    printf("%s", info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int main() {
    // 初始化日志

    int port = 1027;

    AddSig(SIGPIPE, SIG_IGN);  // 忽略SIGPIPE信号

    // 创建数据库连接池
    DbConnPool* db_conn_pool = DbConnPool::Instance();
    db_conn_pool->Init("localhost", "root", "root", "webserdb", 3306, 8);

    // 创建线程池
    ThreadPool<HttpConn>* pool = new ThreadPool<HttpConn>();

    // 初始化数据库读取表

    // 创建socket
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    // 命名socket
    sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    int ret = 0;
    ret = bind(listenfd, (sockaddr*)(&address), sizeof(address));
    assert(ret >= 0);

    // 监听socket
    ret = listen(listenfd, 5);
    assert(ret >= 0);

    // 创建epoll内核事件表
    epollfd = epoll_create(5);
    assert(epollfd != -1);

    // 将创建好的epoll内核事件表共享给HttpConn类
    HttpConn::epollfd_ = epollfd;

    // 往内核事件表中注册监听socket读事件
    AddFd(epollfd, listenfd, false);

    // 统一事件源
    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    SetNonBlocking(pipefd[1]);
    AddFd(epollfd, pipefd[0], false);
    AddSig(SIGALRM, SigHandler, false);  // 定时事件，用SIGALRM信号实现定时
    AddSig(SIGTERM, SigHandler, false);  // 终止信号

    // 进入主循环前的准备工作
    bool stop_server = false;
    epoll_event events[kMaxEventNumber];
    ClientData* client_data = new ClientData[kMaxFd];
    HttpConn* clients = new HttpConn[kMaxFd];
    assert(clients);
    bool timeout = false;
    alarm(kTimingCircle);

    // 进入主循环
    while (!stop_server) {
        int number = epoll_wait(epollfd, events, kMaxEventNumber, -1);
        if (number < 0 && errno != EINTR) {
            printf("epoll failure");
            break;
        }

        for (int i = 0; i < number; ++i) {
            int sockfd = events[i].data.fd;

            // 处理新到的客户连接
            if (sockfd == listenfd) {
                sockaddr_in client_address;
                socklen_t client_adress_len = sizeof(client_address);
                while (true) {
                    int connfd = accept(listenfd, (sockaddr*)&client_address,
                                        &client_adress_len);

                    if (connfd < 0) {
                        // 连接已经接受完毕
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            printf("accept later.\n");
                            break;
                        }
                        // 连接错误（是不是要退出程序、释放各种资源了？）
                        printf("%s:errno is:%d\n", "accept error", errno);
                        break;
                    }
                    // 连接数超过上限
                    if (HttpConn::client_count_ >= kMaxFd) {
                        printf("client_count_ >= kMaxFd\n");
                        break;
                    }

                    // 保存客户连接
                    clients[connfd].Init(connfd, client_address);

                    // 初始化客户数据（主要是创建定时器）
                    client_data[connfd].address = client_address;
                    client_data[connfd].sockfd = connfd;
                    Timer* timer = new Timer();
                    timer->client_data = &client_data[connfd];
                    timer->cb_func = CloseConn;
                    timer->expire = time(nullptr) + 3 * kTimingCircle;
                    client_data[connfd].timer = timer;
                    sort_timer_list.AddTimer(timer);
                }
            }

            // 处理文件描述符上的错误
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                // 关闭连接，移除对应的定时器
                Timer* timer = client_data[sockfd].timer;
                assert(timer);
                timer->cb_func(&client_data[sockfd]);
                sort_timer_list.DeleteTimer(timer);
            }

            // 处理信号
            else if (sockfd == pipefd[0] && events[i].events & EPOLLIN) {
                char signals[1024];
                // ET模式不是应该这么写吗？可是作者不是这么写的
                while (true) {
                    ret = recv(pipefd[0], signals, sizeof(signals), 0);
                    if (ret < 0) {
                        // 信号已经接收完毕
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            printf("recv pipefd[0] later\n");
                            break;
                        }
                        // 发生错误（是不是要退出程序、释放各种资源了？）
                        printf("%s:errno is:%d", "accept error", errno);
                        break;
                    }
                    for (int i = 0; i < ret; ++i) {
                        switch (signals[i]) {
                            case SIGALRM:
                                timeout = true;
                                break;
                            case SIGTERM:
                                stop_server = true;
                                break;
                            default:
                                break;
                        }
                    }
                }

            }

            // 处理客户连接上的读就绪事件
            else if (events[i].events & EPOLLIN) {
                Timer* timer = client_data[sockfd].timer;
                // 根据读的结果，决定是将任务添加到线程池，还是关闭连接
                if (clients[sockfd].Read()) {
                    // LOG_INFO("deal with the client(%s)",
                    //          inet_ntoa(users[sockfd].get_address()->sin_addr));
                    // Log::get_instance()->flush();
                    // 若监测到读就绪事件，将该事件放入请求队列
                    pool->append(&clients[sockfd]);

                    // 该连接活动了，重置超时时间
                    // 并对新的定时器在链表上的位置进行调整
                    if (timer) {
                        time_t cur = time(nullptr);
                        timer->expire = cur + 3 * kTimingCircle;
                        // LOG_INFO("%s", "adjust timer once");
                        // Log::get_instance()->flush();
                        sort_timer_list.AdjustTimer(timer);
                    }
                }
                // 关闭连接
                else {
                    CloseConn(&client_data[sockfd]);
                    sort_timer_list.DeleteTimer(timer);
                }
            }

            // 处理客户连接上的写就绪事件
            else if (events[i].events & EPOLLOUT) {
                Timer* timer = client_data[sockfd].timer;
                assert(timer);
                // 根据写操作的返回值，决定是否保持连接
                if (clients[sockfd].Write()) {
                    // LOG_INFO("send data to the client(%s)",
                    //          inet_ntoa(users[sockfd].get_address()->sin_addr));
                    // Log::get_instance()->flush();

                    // 该连接活动了，重置超时时间
                    // 并对新的定时器在链表上的位置进行调整
                    time_t cur = time(nullptr);
                    timer->expire = cur + 3 * kTimingCircle;
                    // LOG_INFO("%s", "adjust timer once");
                    // Log::get_instance()->flush();
                    sort_timer_list.AdjustTimer(timer);
                }
                // 关闭连接
                else {
                    CloseConn(&client_data[sockfd]);
                    sort_timer_list.DeleteTimer(timer);
                }
            }

            // 最后处理定时事件（因为I/O事件有更高的优先级，当然，这样做会导致定时任务被延迟执行）
            if (timeout) {
                printf("处理定时事件\n");
                TimerHandler();
                timeout = false;
            }
        }
    }
    close(epollfd);
    close(listenfd);
    close(pipefd[1]);
    close(pipefd[0]);
    delete[] clients;
    delete[] client_data;
    delete pool;
    return 0;
}