#ifndef THREAD_DATA_H
#define THREAD_DATA_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <functional>
#include <assert.h>

// 后面改成不 detach 吧
class ThreadPool {
   public:
    explicit ThreadPool(int thread_num = 8) : data_(std::make_shared<Data>()) {
        assert(thread_num > 0);
        for (int i = 0; i < thread_num; ++i) {
            std::thread([data = data_] {
                std::unique_lock<std::mutex> locker(data->mtx);
                while (true) {
                    if (data->is_stopped)
                        break;
                    if (!data->tasks_.empty()) {
                        auto task =
                            std::move(data->tasks_.front());  // 有必要 move 吗？
                        data->tasks_.pop();
                        locker.unlock();
                        task();
                        locker.lock();
                    } else {
                        data->cv.wait(locker);
                    }
                }
            }).detach();
        }
    }

    ThreadPool() = default;

    ThreadPool(ThreadPool&&) = default;

    ~ThreadPool() {
        if (data_) {
            std::unique_lock<std::mutex> locker(data_->mtx);
            data_->is_stopped = true;
            locker.unlock();
            data_->cv.notify_all();
        }
    }

    // 还完美转发？
    template<typename F>
    void AddTask(F&& task) {
        std::unique_lock<std::mutex> locker(data_->mtx);
        data_->tasks_.emplace(std::forward<F>(task));
        locker.unlock();
        data_->cv.notify_one();
    }

   private:
    struct Data {
        std::mutex mtx;
        std::condition_variable cv;
        bool is_stopped;
        std::queue<std::function<void()>> tasks_;
    };
    std::shared_ptr<Data> data_;
};

#endif