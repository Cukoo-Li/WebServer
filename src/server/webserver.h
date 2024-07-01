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

class WebServer {
    public:
    WebServer(int port, int timeout, bool enable_linger, int sql_port, const char* sql_user, const char* sql_pwd, const char* db_name, int sql_conn_pool_size, int thread_pool_size, bool enable_log, int log_level, int log_que_size);
    ~WebServer();

    void Startup();

    private:
    bool InitSocket();
    void InitEventMode();
    void AddClient(int fd, sockaddr_in addr);

    void DealListen();
    void DealRead(HttpConn* client);
    void DealWrite(HttpConn* client);

    void SendError(int fd, const char* info);
    void ExtentTime(HttpConn* client);
    void CloseConn(HttpConn* client);

    void OnRead(HttpConn* client);
    void OnWrite(HttpConn* client);
    void OnProcess(HttpConn* client);

    static const int kMaxFd = 65536;

    static int SetFdNonblock(int fd);

    int port_;
    bool open_linger_;
    int timeout_;
    bool is_closed_;
    int listen_fd_;
    const char* src_dir_;

    uint32_t listen_event_;
    uint32_t conn_event_;

    std::unique_ptr<TimerHeap> timer_heap_;
    std::unique_ptr<ThreadPool> thread_pool_;
    std::unique_ptr<Epoller> epoller_;
    std::unordered_map<int, HttpConn> clients_;

};

#endif