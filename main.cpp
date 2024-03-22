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

#define MAX_FD 65536            // 最大文件描述符
#define MAX_EVENT_NUMBER 10000  // 最大事件数

#define SYNLOG  // 同步写日志
// #define ASYNLOG     // 异步写日志

#define ET_MODE
// #define LT_MODE

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
    assert(sigaction(sig, &sa, nullptr) != -1);
}

// 处理SIGALRM信号（定时事件）
void TimerHandler() {
    sort_timer_list.tick();
    alarm(kTimingCircle);
}

// 处理非活动连接（定时任务，用作定时器回调函数）
// 删除非活动连接在socket上的注册事件，并关闭
void HandleInactiveConn(ClientData* client_data) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, client_data->sockfd, 0);
    assert(client_data);
    close(client_data->sockfd);
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


    return 0;
}