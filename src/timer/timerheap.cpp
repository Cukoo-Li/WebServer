#include "timer/timerheap.h"

// 构造函数中预申请一些空间
TimerHeap::TimerHeap() {
    heap_.reserve(64);
}

// trivial deconstructor
TimerHeap::~TimerHeap() {

}

// 向上调整
void TimerHeap::SiftUp(int idx) {
    assert(idx < heap_.size());
    int i = idx;
    int j = (i - 1) / 2;
    while (j >= 0) {
        if (heap_[j] <= heap_[i]) {
            break;
        }
        SwapNode(i, j);
        i = j;
        j = (i - 1) / 2;
    }
}

// 向下调整（仅在 i 处不满足堆性质）
bool TimerHeap::SiftDown(int idx, int last) {
    assert(idx < heap_.size());
    assert(last <= heap_.size());
    int i = idx;
    int j = i * 2 + 1;
    while (j < last) {
        if (j + 1 < last && heap_[j + 1] < heap_[j]) {
            ++j;
        }
        if (heap_[i] <= heap_[j]) {
            break;
        }
        SwapNode(i, j);
        i = j;
        j = i * 2 + 1;
    }
    return i > idx;
}

// 交换两个结点的位置
void TimerHeap::SwapNode(int i, int j) {
    assert(i < heap_.size());
    assert(j < heap_.size());
    std::swap(heap_[i], heap_[j]);
    ref_[heap_[i].id] = i;
    ref_[heap_[j].id] = j;
}

// 添加结点
void TimerHeap::Add(int id, int timeout, const TimeoutCallBack& cb) {
    assert(id >= 0);
    int i;
    if (ref_.count(id) == 0) {
        // 新结点：堆尾插入，调整堆
        i = heap_.size();
        ref_[id] = i;
        heap_.push_back({id, Clock::now() + MS(timeout), cb});
        SiftUp(i);
    } else {
        // 已有结点：调整堆
        i = ref_[id];
        heap_[i].expires = Clock::now() + MS(timeout);
        heap_[i].cb = cb;
        if (!SiftDown(i, heap_.size())) {
            SiftUp(i);
        }
    }
}

// 触发指定的定时器
void TimerHeap::DoWork(int id) {
    if (heap_.empty() || ref_.count(id) == 0) {
        return;
    }
    int i = ref_[id];
    Timer timer = heap_[i];
    timer.cb();
    Remove(i);
}

// 删除结点
void TimerHeap::Remove(int idx) {
    assert(!heap_.empty() && idx < heap_.size());
    // 将要删除的结点换到堆尾，然后调整堆
    int i = idx;
    int n = heap_.size() - 1;
    assert(i <= n);
    if (i < n) {
        SwapNode(i, n);
        if (!SiftDown(i, n)) {
            SiftUp(i);
        }
    }
    // 删除堆尾元素
    ref_.erase(heap_.back().id);
    heap_.pop_back();
}

// 更新指定结点的超时时间（这里只考虑超时时间延长的情形）
void TimerHeap::Adjust(int id, int timeout) {
    assert(!heap_.empty() && ref_.count(id) > 0);
    heap_[ref_[id]].expires = Clock::now() + MS(timeout);
    SiftDown(ref_[id], heap_.size());
}

// 处理当前所有超时的结点，并返回最小结点的超时值
int TimerHeap::Tick() {
    while (!heap_.empty()) {
        Timer timer = heap_.front();
        if (std::chrono::duration_cast<MS>(timer.expires - Clock::now())
                .count() > 0) {
            break;
        }
        timer.cb();
        Pop();
    }
    int res = -1;
    if (!heap_.empty()) {
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        res = std::max(res, 0);
    }
    return res;
}

// 弹出堆顶结点
void TimerHeap::Pop() {
    assert(!heap_.empty());
    Remove(0);
}

// 删除所有结点
void TimerHeap::Clear() {
    ref_.clear();
    heap_.clear();
}
