#ifndef EPOLLER_H
#define EPOLLER_H

#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <vector>
#include <errno.h>

class Epoller {
   public:
    explicit Epoller(int max_event_num = 1024);
    ~Epoller();
    
    bool Add(int fd, uint32_t events);
    bool Modify(int fd, uint32_t events);
    bool Remove(int fd);
    int Wait(int timeout = -1);
    int GetEventFd(size_t i) const;
    uint32_t GetEvents(size_t i) const;

   private:
    int epoll_fd_;
    std::vector<epoll_event> events_;
};

#endif