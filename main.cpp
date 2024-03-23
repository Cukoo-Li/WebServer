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

#include "timer/sort_timer_list.h"

const int kMaxFd = 65536;           // 最大文件描述符
const int kMaxEventNumber = 10000;  // 最大事件数

#define SYNLOG  // 同步写日志
// #define ASYNLOG     // 异步写日志

// 在http_conn.cpp中定义
extern int AddFd(int epollfd, int fd, bool one_shot);
extern int RemoveFd(int epollfd, int fd);
extern int SetNonBlocking(int fd);

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
    sort_timer_list.tick();
    alarm(kTimingCircle);
}

// 处理非活动连接（定时任务，用作定时器回调函数）
// 删除非活动连接在socket上的注册事件，并关闭
void HandleInactiveConn(ClientData* client_datas) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, client_datas->sockfd, 0);
    assert(client_datas);
    close(client_datas->sockfd);
    // http_conn::m_user_count--;
    // LOG_INFO("close fd %d", user_data->sockfd);
    // Log::get_instance()->flush();
}

void ShowError(int connfd, const char* info) {
    printf("%s", info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int main(int argc, char* argv[]) {
    // 初始化日志
    if (argc == 1)
        printf("usage: %s port_number\n", basename(argv[0]));

    int port = atoi(argv[1]);

    AddSig(SIGPIPE, SIG_IGN);  // 忽略SIGPIPE信号

    // 创建数据库连接池

    // 创建线程池

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

    // 往内核事件表中注册监听socket读事件
    AddFd(epollfd, listenfd, false);
    // http_conn::m_epollfd = epollfd;

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
    ClientData* client_datas = new ClientData[kMaxFd];
    // http_conn *users = new http_conn[kMaxFd];
    // assert(users);
    bool timeout = false;
    alarm(kTimingCircle);

    // 进入主循环
    while (!stop_server) {
        int number = epoll_wait(epollfd, events, kMaxEventNumber, -1);
        if (number < 0 && errno != EINTR) {
            // LOG_ERROR("%s", "epoll failure");
            printf("epoll failure");
            break;
        }

        for (int i = 0; i < number; ++i) {
            int sockfd = events[i].data.fd;

            // 处理新到的客户连接
            if (sockfd == listenfd) {
                sockaddr_in client_adress;
                socklen_t client_adress_len = sizeof(client_adress);
                while (true) {
                    int connfd = accept(listenfd, (sockaddr*)&client_adress,
                                        &client_adress_len);

                    if (connfd < 0) {
                        // 连接已经接受完毕
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            printf("accept later\n");
                            break;
                        }
                        // 连接错误（是不是要退出程序、释放各种资源了？）
                        printf("%s:errno is:%d", "accept error", errno);
                        break;
                    }
                    // // 连接数超过上限
                    // if (http_conn::m_user_count >= MAX_FD)
                    // {
                    //     show_error(connfd, "Internal server busy");
                    //     LOG_ERROR("%s", "Internal server busy");
                    //     break;
                    // }

                    // // 保存客户连接
                    // users[connfd].init(connfd, client_address);

                    // 初始化客户数据（主要是创建定时器）
                    client_datas[connfd].address = client_adress;
                    client_datas[connfd].sockfd = connfd;
                    Timer* timer = new Timer();
                    timer->client_data = &client_datas[connfd];
                    timer->cb_func = HandleInactiveConn;
                    timer->expire = time(nullptr) + 3 * kTimingCircle;
                    client_datas[connfd].timer = timer;
                    sort_timer_list.AddTimer(timer);
                }
            }
            // 处理
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
            }
            // 处理信号
            else if (sockfd == pipefd[0] && events[i].events & EPOLLIN) {
            }

            // 处理客户连接上接收到的数据
            else if (events[i].events & EPOLLIN) {
            }
            // 最后处理定时事件（因为I/O事件有更高的优先级，当然，这样做会导致定时任务被延迟执行）
            if (timeout) {
                TimerHandler();
                timeout = false;
            }
        }
    }
    close(epollfd);
    close(listenfd);
    close(pipefd[1]);
    close(pipefd[0]);
    // delete[] users;
    delete[] client_datas;
    // delete pool;
    return 0;
}