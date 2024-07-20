// Author: Cukoo
// Date: 2024-07-03

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

    MYSQL* BorrowConn();
    void ReturnConn(MYSQL* sql);

    void Init(const char* host, int port, const char* user, const char* pwd, const char* db_name, int size = 8);

    private:
    SqlConnPool() = default;
    ~SqlConnPool();

    std::queue<MYSQL*> conns_que_;
    std::mutex mtx_;
    std::condition_variable cv_;

};

#endif
