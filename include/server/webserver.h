// Author: Cukoo
// Date: 2024-07-02

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <unordered_map>
#include <fcntl.h>       
#include <unistd.h>      
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <spdlog/spdlog.h>

#include "epoller.h"
#include "timer/timerheap.h"
#include "pool/sqlconnpool.h"
#include "pool/threadpool.h"
#include "pool/sqlconnguard.h"
#include "http/httpconn.h"
#include "config/config.h"

class WebServer {
    public:
    WebServer(Config config);
    ~WebServer();

    void Startup();

    private:
    static const int kMaxFd_ = 65536;
    static int SetFdNonblock(int fd);

    bool InitListenSocket();

    void HandleListenFdEvent();
    void HandleReadableEvent(HttpConn* client);
    void HandleWritableEvent(HttpConn* client);

    void SendError(int fd, const char* info);
    void ResetTimer(HttpConn* client);
    void CloseConn(HttpConn* client);

    void OnRead(HttpConn* client);
    void OnWrite(HttpConn* client);
    void OnProcess(HttpConn* client);

    const int kPort_;
    const int kTimeout_;
    const bool kEnableLinger_;
    const std::string kWorkDir_;
    bool is_closed_;
    int listenfd_;

    uint32_t listenfd_event_;
    uint32_t connfd_event_;

    std::unique_ptr<TimerHeap> timer_heap_;
    std::unique_ptr<ThreadPool> thread_pool_;
    std::unique_ptr<Epoller> epoller_;
    std::unordered_map<int, HttpConn> clients_;
};

#endif