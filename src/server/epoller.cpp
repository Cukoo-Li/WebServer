#include "epoller.h"

Epoller::Epoller(int max_event_num)
    : epoll_fd_(epoll_create(1)), events_(max_event_num) {
    assert(epoll_fd_ >= 0 && events_.size() > 0);
}

Epoller::~Epoller() {
    close(epoll_fd_);
}

bool Epoller::AddFd(int fd, uint32_t events) {
    assert(fd >= 0);
    epoll_event ev{};
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);
}

bool Epoller::ModFd(int fd, uint32_t events) {
    assert(fd >= 0);
    epoll_event ev{};
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
}

bool Epoller::DelFd(int fd) {
    assert(fd >= 0);
    return 0 == epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
}

int Epoller::Wait(int timeout) {
    return epoll_wait(epoll_fd_, &events_[0], static_cast<int>(events_.size()),
                      timeout);
}

int Epoller::GetEventFd(size_t i) const {
    assert(i < events_.size());
    return events_[i].data.fd;
}

uint32_t Epoller::GetEvents(size_t i) const {
    assert(i < events_.size());
    return events_[i].events;
}
