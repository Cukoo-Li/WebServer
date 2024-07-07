#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>
#include <fcntl.h>       
#include <unistd.h>      
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "epoller.h"
#include "../log/log.h"
#include "../timer/timerheap.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"
#include "../pool/sqlconnguard.h"
#include "../http/httpconn.h"
#include "../config/config.h"

class WebServer {
    public:
    WebServer(Config config);
    ~WebServer();

    void Startup();

    private:
    bool InitSocket();
    void AddClient(int fd, sockaddr_in addr);

    void HandleListenFdEvent();
    void HandleReadableEvent(HttpConn* client);
    void HandleWritableEvent(HttpConn* client);

    void SendError(int fd, const char* info);
    void ExtentTime(HttpConn* client);
    void CloseConn(HttpConn* client);

    void OnRead(HttpConn* client);
    void OnWrite(HttpConn* client);
    void OnProcess(HttpConn* client);

    static const int kMaxFd = 65536;

    static int SetFdNonblock(int fd);

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