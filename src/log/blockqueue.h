#ifndef BLOCK_QUEUE_h
#define BLOCK_QUEUE_h

#include <sys/time.h>
#include <condition_variable>
#include <mutex>
#include <queue>

template <typename T>
class BlockQueue {
   public:
    explicit BlockQueue(int capacity = 1000);
    ~BlockQueue();
    void Clear();
    bool Empty();
    bool Full();
    T Front();
    T Back();
    void PushBack(const T& item);
    void PushFront(const T& item);
    bool PopFront(T& item);
    bool PopFront(T& item, int timeout);
    void Flush();
    int size();
    int capacity();

   private:
    std::deque<T> deq_;
    std::mutex mtx_;
    std::condition_variable cond_consumer_;
    std::condition_variable cond_producer_;

    bool is_closed_;
    int capacity_;
};

template<typename T>
BlockQueue<T>::BlockQueue(int capacity) {
    capacity_ = capacity;
    is_closed_ = false;
}

template <typename T>
BlockQueue<T>::~BlockQueue() {
    std::unique_lock<std::mutex> locker(mtx_);
    deq_.clear();
    is_closed_ = true;
    locker.unlock();
    cond_producer_.notify_all();
    cond_consumer_.notify_all();
}



// 唤醒一个消费者，让它将东西都拿走
template <typename T>
void BlockQueue<T>::Flush() {
    cond_consumer_.notify_one();
}

template <typename T>
void BlockQueue<T>::Clear() {
    std::unique_lock<std::mutex> locker(mtx_);
    deq_.clear();
}

template <typename T>
T BlockQueue<T>::Front() {
    std::unique_lock<std::mutex> locker(mtx_);
    return deq_.front();
}


template <typename T>
T BlockQueue<T>::Back() {
    std::unique_lock<std::mutex> locker(mtx_);
    return deq_.back();
}


template <typename T>
int BlockQueue<T>::size() {
    std::unique_lock<std::mutex> locker(mtx_);
    return deq_.size();
}

template <typename T>
int BlockQueue<T>::capacity() {
    return capacity_;
}

// 向队尾添加一个元素
// 这是生产者干的事
template <typename T>
void BlockQueue<T>::PushBack(const T& item) {
    std::unique_lock<std::mutex> locker(mtx_);
    // 必须有空位，生产者才能放东西
    while (deq_.size() >= capacity_) {
        cond_producer_.wait(locker);
    }
    deq_.push_back(item);
    locker.unlock();
    cond_consumer_.notify_one();
}

// 向队尾添加一个元素
// 这是生产者干的事
template <typename T>
void BlockQueue<T>::PushFront(const T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    // 必须有空位，生产者才能放东西
    while (deq_.size() >= capacity_) {
        cond_producer_.wait(locker);
    }
    deq_.push_front(item);
    locker.unlock();
    cond_consumer_.notify_one();
}

// 弹出队头元素
// 这是消费者干的事
template <typename T>
bool BlockQueue<T>::PopFront(T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    // 必须有东西，消费者才能拿东西
    while (deq_.empty()) {
        cond_consumer_.wait(locker);
        if (is_closed_) {
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    locker.unlock();
    cond_producer_.notify_one();
    return true;
}

// 弹出队头元素（在规定时间内未等到直接返回）
// 这是消费者干的事
template <typename T>
bool BlockQueue<T>::PopFront(T& item, int timeout) {
    std::unique_lock<std::mutex> locker(mtx_);
    // 必须有东西，消费者才能拿东西
    while (deq_.empty()) {
        if (cond_consumer_.wait_for(locker, std::chrono::seconds(timeout)) == std::cv_status::timeout) {
            return false;
        }
        if (is_closed_) {
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    locker.unlock();
    cond_producer_.notify_one();
    return true;
}

template <typename T>
bool BlockQueue<T>::Empty() {
    std::unique_lock<std::mutex> locker(mtx_);
    return deq_.empty();
}

template <typename T>
bool BlockQueue<T>::Full() {
    std::unique_lock<std::mutex> locker(mtx_);
    return deq_.size() >= capacity_;
}
#endif