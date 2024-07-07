#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <functional>
#include <assert.h>

// 后面改成不 detach 吧
class ThreadPool {
   public:
    explicit ThreadPool(int thread_num = 8) : pool_(std::make_shared<Pool>()) {
        assert(thread_num > 0);
        for (int i = 0; i < thread_num; ++i) {
            std::thread([pool = pool_] {
                std::unique_lock<std::mutex> locker(pool->mtx);
                while (true) {
                    if (pool->is_stopped)
                        break;
                    if (!pool->tasks_que_.empty()) {
                        auto task =
                            std::move(pool->tasks_que_.front());  // 有必要 move 吗？
                        pool->tasks_que_.pop();
                        locker.unlock();
                        task();
                        locker.lock();
                    } else {
                        pool->cv.wait(locker);
                    }
                }
            }).detach();
        }
    }

    ThreadPool() = default;

    ThreadPool(ThreadPool&&) = default;

    ~ThreadPool() {
        if (pool_) {
            std::unique_lock<std::mutex> locker(pool_->mtx);
            pool_->is_stopped = true;
            locker.unlock();
            pool_->cv.notify_all();
        }
    }

    // 还完美转发？
    template<typename F>
    void AddTask(F&& task) {
        std::unique_lock<std::mutex> locker(pool_->mtx);
        pool_->tasks_que_.emplace(std::forward<F>(task));
        locker.unlock();
        pool_->cv.notify_one();
    }

   private:
    struct Pool {
        std::mutex mtx;
        std::condition_variable cv;
        bool is_stopped;
        std::queue<std::function<void()>> tasks_que_;
    };
    std::shared_ptr<Pool> pool_;
};

#endif