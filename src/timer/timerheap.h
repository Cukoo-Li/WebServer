#ifndef TIMER_HEAP_H
#define TIMER_HEAP_H

#include <arpa/inet.h>
#include <assert.h>
#include <time.h>
#include <algorithm>
#include <chrono>
#include <functional>
#include <queue>
#include <unordered_map>
#include <spdlog/spdlog.h>


using TimeoutCallBack = std::function<void()>;
using Clock = std::chrono::high_resolution_clock;
using MS = std::chrono::milliseconds;
using TimeStamp = Clock::time_point;

struct Timer {
    int id;
    TimeStamp expires;
    TimeoutCallBack cb;
    bool operator<=(const Timer& that) { return expires <= that.expires; }
    bool operator<(const Timer& that) { return expires < that.expires; }
};

class TimerHeap {
   public:
    TimerHeap();
    ~TimerHeap();

    void Adjust(int id, int new_expires);
    void Add(int id, int time_out, const TimeoutCallBack& cb);
    void DoWork(int id);
    void Clear();
    int Tick();
    void Pop();

   private:
    void Remove(int idx);
    void SiftUp(int idx);
    bool SiftDown(int idx, int last);
    void SwapNode(int i, int j);

    std::vector<Timer> heap_;           // 小根堆
    std::unordered_map<int, int> ref_;  // fd / id -> idx
};

#endif
