#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <iostream>

class ThreadPool {
   public:
    explicit ThreadPool(int size) : data_(new Data()) {
        for (int i = 0; i < size; ++i) {
            data_->threads.emplace_back([data = data_]() -> void {
                while (true) {
                    if (data->is_stopped)
                        break;
                    std::unique_lock<std::mutex> locker(data->mtx);
                    while (data->tasks.empty())
                        data->cv.wait(locker);
                    auto task = data->tasks.front();
                    data->tasks.pop();
                    locker.unlock();
                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        std::unique_lock<std::mutex> locker(data_->mtx);
        data_->is_stopped = true;
        locker.unlock();
        data_->cv.notify_all();
        for (auto& t : data_->threads)
            t.join();
    }

    void AddTask(std::function<void()> task) {
        std::unique_lock<std::mutex> locker(data_->mtx);
        data_->tasks.push(task);
        locker.unlock();
        data_->cv.notify_one();
    }

   private:
    struct Data {
        std::mutex mtx;
        std::condition_variable cv;
        std::vector<std::thread> threads;
        std::queue<std::function<void()>> tasks;
        bool is_stopped;
    };
    std::shared_ptr<Data> data_;
};
