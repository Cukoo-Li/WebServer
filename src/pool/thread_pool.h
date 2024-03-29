#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <cstdio>
#include <queue>
#include <stdexcept>
#include <vector>
#include "../itc/itc.h"
// #include "../CGImysql/sql_connection_pool.h"

template <typename T>
class ThreadPool {
   public:
    ThreadPool(/*connection_pool *connPool, */ int thread_number = 8,
               int max_task_number = 10000);
    ~ThreadPool();
    // 往任务队列中添加任务
    bool append(T* task);

   private:
    // 工作线程运行的函数，它不断从任务队列中取出任务并执行之
    static void* worker(void* arg);
    void run();

   private:
    int thread_number_;    // 线程数量
    int max_task_number_;  // 任务队列中允许容纳的最大任务数量
    std::vector<pthread_t> threads_;  // 描述线程池的数组
    std::queue<T*> task_que_;         // 任务队列
    Locker que_locker_;               // 保护任务队列的互斥锁
    Sem que_stat_;  // 信号量，表示任务队列中任务数量
    bool stop_;     // 是否结束线程
    // connection_pool *m_connPool;  //数据库
};

template <typename T>
ThreadPool<T>::ThreadPool(int thread_number, int max_task_number)
    : thread_number_(thread_number),
      max_task_number_(max_task_number),
      stop_(false) {
    if (thread_number <= 0 || max_task_number <= 0)
        throw std::runtime_error("invalid args for ThreadPool()");
    threads_.resize(thread_number_);
    // 创建线程，并设置成脱离线程
    for (int i = 0; i < thread_number_; ++i) {
        if (pthread_create(&threads_[i], nullptr, worker, this) != 0)
            throw std::runtime_error("pthread_create failed.");
        if (pthread_detach(threads_[i]) != 0)
            throw std::runtime_error("pthread_detach failed.");
    }
}

template <typename T>
ThreadPool<T>::~ThreadPool() {
    stop_ = true;
}

template <typename T>
bool ThreadPool<T>::append(T* task) {
    // 操作任务队列时一定要加锁
    que_locker_.Lock();
    if (task_que_.size() >= max_task_number_) {
        que_locker_.Unlock();
        return false;
    }
    task_que_.push(task);
    que_locker_.Unlock();
    que_stat_.Post();
    return true;
}

template <typename T>
void* ThreadPool<T>::worker(void* arg) {
    ThreadPool* pool = static_cast<ThreadPool*>(arg);
    pool->run();
    return pool;
}

template <typename T>
void ThreadPool<T>::run() {
    while (!stop_) {
        que_stat_.Wait();
        que_locker_.Lock();
        if (task_que_.empty()) {
            que_locker_.Unlock();
            continue;
        }
        T* task = task_que_.front();
        task_que_.pop();
        que_locker_.Unlock();
        assert(task);
        task->Process();
    }
}

#endif