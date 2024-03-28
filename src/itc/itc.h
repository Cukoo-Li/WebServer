#ifndef ITC_H
#define ITC_H

#include <pthread.h>
#include <semaphore.h>
#include <stdexcept>

class Sem {
   public:
    Sem(int num = 0) {
        if (sem_init(&sem_, 0, num) != 0)
            throw std::runtime_error("sem_init failed in Sem().");
    }
    ~Sem() { sem_destroy(&sem_); }
    bool Wait() { return sem_wait(&sem_) == 0; }
    bool Post() { return sem_post(&sem_) == 0; }

   private:
    sem_t sem_;
};

class Locker {
   public:
    Locker() {
        if (pthread_mutex_init(&mutex_, nullptr) != 0)
            throw std::runtime_error("pthread_mutex_init failed in Locker().");
    }
    ~Locker() { pthread_mutex_destroy(&mutex_); }
    bool Lock() { return pthread_mutex_lock(&mutex_) == 0; }
    bool Unlock() { return pthread_mutex_unlock(&mutex_) == 0; }

   private:
    pthread_mutex_t mutex_;
};

class Cond {
   public: 
    Cond() {
        if (pthread_mutex_init(&mutex_, nullptr) != 0)
            throw std::runtime_error("pthread_mutex_init failed in Cond().");
        if (pthread_cond_init(&cond_, nullptr) != 0) {
            pthread_mutex_destroy(&mutex_);
            throw std::runtime_error("pthread_cond_init failed in Cond().");
        }
    }
    ~Cond() {
        pthread_mutex_destroy(&mutex_);
        pthread_cond_destroy(&cond_);
    }
    bool Wait(pthread_mutex_t* mutex) {
        int ret = 0;
        pthread_mutex_lock(&mutex_);
        ret = pthread_cond_wait(&cond_, &mutex_);
        pthread_mutex_unlock(&mutex_);
        return ret == 0;
    }
    bool Signal() { return pthread_cond_signal(&cond_) == 0; }

   private:
    pthread_cond_t cond_;
    pthread_mutex_t mutex_;
};

#endif