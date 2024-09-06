// Author: Cukoo
// Date: 2024-07-02

#ifndef THREAD_DATA_H
#define THREAD_DATA_H

#include <assert.h>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

class ThreadPool {
   public:
    explicit ThreadPool(int size = 8) {
        for (int i = 0; i < size; ++i) {
            threads_.emplace_back([&]() -> void {
                std::unique_lock<std::mutex> lck(mtx_);
                while (true) {
                    // 检查 tasks_ 和 is_shutdown_ 时必须加锁
                    if (!tasks_.empty()) {
                        auto task = tasks_.front();
                        tasks_.pop();
                        lck.unlock();
                        task();
                        lck.lock();
                        continue;       // 优雅关闭的关键
                    }
                    if (is_shutdown_) {
                        break;
                    }
                    // 如果两个条件都不满足，就在条件变量上等待
                    cv_.wait(lck);
                }
            });
        }
    }

    ~ThreadPool() {
        // 修改 is_shutdown_，然后通知所有线程，并逐个 join
        std::unique_lock<std::mutex> lck(mtx_);
        is_shutdown_ = true;
        lck.unlock();
        cv_.notify_all();
        for (auto& t : threads_) {
            t.join();
        }
    }

    void AddTask(std::function<void()> task) {
        // 修改 tasks_，然后通知一个线程
        std::unique_lock<std::mutex> lck(mtx_);
        tasks_.push(task);
        lck.unlock();
        cv_.notify_one();
    }

   private:
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool is_shutdown_;
};

#endif