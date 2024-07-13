// Author: Cukoo
// Date: 2024-07-03


#include "sqlconnpool.h"

#include <assert.h>

SqlConnPool* SqlConnPool::Instance() {
    static SqlConnPool conn_pool;
    return &conn_pool;
}

void SqlConnPool::Init(const char* host,
                       int port,
                       const char* user,
                       const char* pwd,
                       const char* db_name,
                       int size) {
    for (int i = 0; i < size; ++i) {
        MYSQL* sql = nullptr;
        sql = mysql_init(sql);
        if (!sql) {
            spdlog::error("MySQL init error!");
            continue;
        }
        sql = mysql_real_connect(sql, host, user, pwd, db_name, port, nullptr, 0);
        if (!sql) {
            spdlog::error("MySQL connect error!");
            continue;
        }
        conns_que_.push(sql);
    }
}

MYSQL* SqlConnPool::BorrowConn() {
    MYSQL* sql = nullptr;
    std::unique_lock<std::mutex> locker(mtx_);
    while (conns_que_.empty()) {
        cv_.wait(locker);
    }
    sql = conns_que_.front();
    conns_que_.pop();
    locker.unlock();
    return sql;
}

void SqlConnPool::ReturnConn(MYSQL* sql) {
    assert(sql);
    std::unique_lock<std::mutex> locker(mtx_);
    conns_que_.push(sql);
    locker.unlock();
    cv_.notify_one();
}

SqlConnPool::~SqlConnPool() {
    // 由于线程池先销毁，所有子线程都已经结束，这就说明外借的连接都已悉数归还
    // 这样就保证里所有的连接都会被 close
    std::unique_lock<std::mutex> locker(mtx_);
    while (!conns_que_.empty()) {
        auto conn = conns_que_.front();
        conns_que_.pop();
        mysql_close(conn);
    }
    mysql_server_end();
}
