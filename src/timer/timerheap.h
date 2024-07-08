#ifndef TIMER_HEAP_H
#define TIMER_HEAP_H

#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h>
#include <functional>
#include <assert.h>
#include <chrono>

#include "../log/log.h"

using TimeoutCallBack = std::function<void()>;
using Clock = std::chrono::high_resolution_clock;
using MS = std::chrono::milliseconds;
using TimeStamp = Clock::time_point;

struct Timer {
    int id;
    TimeStamp expires;
    TimeoutCallBack cb;
    bool operator<=(const Timer& that) { return expires <= that.expires; }
};

class TimerHeap {
   public:
    TimerHeap();
    ~TimerHeap();

    void Adjust(int id, int new_expires);
    void Add(int id, int time_out, const TimeoutCallBack& cb);
    void DoWork(int id);
    void Clear();       // ???
    void Tick();        // ???
    void Pop();         // ???
    int GetNextTick();  // ???

   private:
    void Remove(size_t i);              // ???
    void SiftUp(size_t i);              // ???
    bool SiftDown(size_t i, size_t n);  // ???
    void SwapNode(size_t i, size_t j);  // ???

    std::vector<Timer> heap_;
    std::unordered_map<int, size_t> ref_;  // ???
};

#endif