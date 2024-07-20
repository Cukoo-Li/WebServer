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
    explicit ThreadPool(int size = 8) : data_(std::make_shared<Data>()) {
        for (int i = 0; i < size; ++i) {
            data_->threads.emplace_back([data = data_]() {
                std::unique_lock<std::mutex> locker(data->mtx);
                while (true) {
                    if (!data->tasks.empty()) {
                        auto task = data->tasks.front();
                        data->tasks.pop();
                        locker.unlock();
                        task();
                        locker.lock();
                        continue;
                    }
                    if (data->is_stopped) {
                        break;
                    }
                    data->cv.wait(locker);
                }
            });
        }
    }

    ~ThreadPool() {
        std::unique_lock<std::mutex> locker(data_->mtx);
        data_->is_stopped = true;
        locker.unlock();
        data_->cv.notify_all();
        for (auto& t : data_->threads) {
            t.join();
        }
    }

    void AddTask(std::function<void()> task) {
        std::unique_lock<std::mutex> locker(data_->mtx);
        data_->tasks.emplace(task);
        locker.unlock();
        data_->cv.notify_one();
    }

   private:
    // 如果不 detach，最后要 join
    // 的话，就没必要封装一个结构体，再设置一个共享指针
    struct Data {
        std::vector<std::thread> threads;
        std::queue<std::function<void()>> tasks;
        std::mutex mtx;
        std::condition_variable cv;
        bool is_stopped;
    };
    std::shared_ptr<Data> data_;
};

#endif