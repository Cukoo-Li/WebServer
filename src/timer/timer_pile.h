#ifndef TIMER_PILE
#define TIMER_PILE

#include <netinet/in.h>
#include <time.h>
#include <queue>

class HttpConn;

// 定时器类
class Timer {
   public:
    Timer(HttpConn* http_conn, void (*cb_func)(HttpConn*), time_t deadline);
    ~Timer() {}
    void Delete();
    void set_deadline(time_t deadline) { deadline_ = deadline; }
    time_t deadline() { return deadline_; }
    bool IsDeleted() { return deleted_; }
    bool IsExpired() { return deadline_ <= time(nullptr); }
    void Execute() {cb_func_(http_conn_);}

   private:
    HttpConn* http_conn_;
    void (*cb_func_)(HttpConn*);  // 定时器回调函数
    time_t deadline_;
    bool deleted_;
};

struct TimerCmp {
    bool operator()(Timer* lhs, Timer* rhs) {
        return lhs->deadline() > rhs->deadline();
    }
};

class TimerPile {
   public:
    TimerPile() {}
    ~TimerPile();
    void AddTimer(Timer* timer);
    void HandleTimer();
    time_t Countdown();
    bool Empty() { return timer_que_.empty(); }

   private:
    std::priority_queue<Timer*, std::vector<Timer*>, TimerCmp> timer_que_;
};

#endif