#include "timer_pile.h"

#include <stdio.h>
#include <unistd.h>

Timer::Timer(HttpConn* http_conn, void (*cb_func)(HttpConn*), time_t deadline)
    : http_conn_(http_conn),
      cb_func_(cb_func),
      deadline_(deadline),
      deleted_(false) {}

void Timer::Delete() {
    deleted_ = true;
    http_conn_ = nullptr;
    cb_func_ = nullptr;
}

TimerPile::~TimerPile() {
    while (!timer_que_.empty()) {
        Timer* timer = timer_que_.top();
        timer_que_.pop();
        delete timer;
    }
}
void TimerPile::AddTimer(Timer* timer) {
    timer_que_.push(timer);
}
void TimerPile::HandleTimer() {
    while (!timer_que_.empty()) {
        Timer* timer = timer_que_.top();
        // 无效定时器
        if (timer->IsDeleted()) {
            printf("无效定时器.\n");
            timer_que_.pop();
            delete timer;
        }
        // 过期定时器
        else if (timer->IsExpired()) {
            printf("处理过期定时器.\n");
            timer_que_.pop();
            timer->Execute();
            delete timer;
        }
        // 未过期定时器
        else {
            printf("未过期定时器\n");
            break;
        }
    }
}
time_t TimerPile::Countdown() {
    Timer* timer = timer_que_.top();
    return timer->deadline() - time(nullptr);
}