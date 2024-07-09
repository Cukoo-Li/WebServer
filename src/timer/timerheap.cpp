#include "timerheap.h"

TimerHeap::TimerHeap() {
    heap_.reserve(64);
}

TimerHeap::~TimerHeap() {
    Clear();
}

void TimerHeap::SiftUp(int i) {
    assert(i < heap_.size());
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

void TimerHeap::SwapNode(int i, int j) {
    assert(i < heap_.size());
    assert(j < heap_.size());
    std::swap(heap_[i], heap_[j]);
    ref_[heap_[i].id] = i;
    ref_[heap_[j].id] = j;
}

// 以 index 为根的堆仅在根结点位置不满足小根堆性质，需要调整（根结点不断下坠）
bool TimerHeap::SiftDown(int index, int n) {
    assert(index < heap_.size());
    assert(n <= heap_.size());
    int i = index;
    int j = i * 2 + 1;
    while (j < n) {
        if (j + 1 < n && heap_[j + 1] <= heap_[j]) {
            ++j;
        }
        if (heap_[i] <= heap_[j]) {
            break;
        }
        SwapNode(i, j);
        i = j;
        j = i * 2 + 1;
    }
    return i > index;
}

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

void TimerHeap::DoWork(int id) {
    if (heap_.empty() || ref_.count(id) == 0) {
        return;
    }
    int i = ref_[id];
    Timer timer = heap_[i];
    timer.cb();
    Remove(i);
}

void TimerHeap::Remove(int index){
    assert(!heap_.empty() && index < heap_.size());
    // 将要删除的结点换到堆尾，然后调整堆
    int i = index;
    int n = heap_.size() - 1;
    assert(i <= n);
    if (i < n) {
        SwapNode(i, n);
        if (!SiftDown(i, n)) {
            SiftUp(i);
        }
    }
    // 堆尾元素删除
    ref_.erase(heap_.back().id);
    heap_.pop_back();
}

// 更新指定结点的超时时间
void TimerHeap::Adjust(int id, int timeout) {
    // 调整指定 id 的结点
    assert(!heap_.empty() && ref_.count(id) > 0);
    heap_[ref_[id]].expires = Clock::now() + MS(timeout);
    SiftDown(ref_[id], heap_.size());
}

void TimerHeap::Tick() {
    // 处理超时的结点
    if (heap_.empty()) {
        return;
    }
    while (!heap_.empty()) {
        Timer timer = heap_.front();
        if (std::chrono::duration_cast<MS>(timer.expires - Clock::now()).count() > 0) {
            break;
        }
        timer.cb();
        Pop();
    }
}

void TimerHeap::Pop() {
    assert(!heap_.empty());
    Remove(0);
}

void TimerHeap::Clear() {
    ref_.clear();
    heap_.clear();
}

// 处理超时结点，并返回新堆顶结点的剩余超时时间（毫秒）
int TimerHeap::GetNextTick() {
    Tick();
    int res = -1;
    if (!heap_.empty()) {
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        res = std::max(res, 0);
    }
    return res;
}
