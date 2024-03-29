#ifndef SORT_TIMER_LIST
#define SORT_TIMER_LIST

#include <time.h>
#include <netinet/in.h>
// #include "../log/log.h"

class Timer;

// 用户数据结构
struct ClientData {
    sockaddr_in address;  // 客户socket地址
    int sockfd;           // 与客户的连接socket
    // char buf[64];         // 读缓存
    Timer* timer;
};

// 定时器类
struct Timer {
    time_t expire;
    void (*cb_func)(ClientData*);  // 定时器回调函数
    ClientData* client_data;
    Timer* prev = nullptr;
    Timer* next = nullptr;
};

// 定时器升序链表类
class SortTimerList {
   public:
    SortTimerList() : head_(nullptr), tail_(nullptr) {}
    ~SortTimerList();
    void AddTimer(Timer* timer);
    void AdjustTimer(Timer* timer);
    void DeleteTimer(Timer* timer);
    void Tick();

   private:
    void AddTimer(Timer* timer, Timer* lst_head);

   private:
    Timer* head_;
    Timer* tail_;
};

#endif