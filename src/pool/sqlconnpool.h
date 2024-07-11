#ifndef SQL_CONN_POOL_H
#define SQL_CONN_POOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <spdlog/spdlog.h>

class SqlConnPool {
    public:
    static SqlConnPool* Instance();

    void BorrowConn(MYSQL* conn);
    MYSQL* ReturnConn();

    void Init(const char* host, int port, const char* user, const char* pwd, const char* db_name, int conn_num = 8);

    private:
    SqlConnPool() = default;
    ~SqlConnPool();

    int conn_num_{};

    std::queue<MYSQL*> conns_que_;
    std::mutex mtx_;
    std::condition_variable cv_;

};

#endif
