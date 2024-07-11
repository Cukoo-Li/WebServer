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
                       int conn_num) {
    assert(conn_num > 0);
    for (int i = 0; i < conn_num; ++i) {
        MYSQL* sql = nullptr;
        sql = mysql_init(sql);
        if (!sql) {
            spdlog::error("MySQL init error!");
        }
        assert(sql);
        sql = mysql_real_connect(sql, host, user, pwd, db_name, port, nullptr, 0);
        if (!sql) {
            spdlog::error("MySQL connect error!");
        }
        assert(sql);
        conns_que_.push(sql);
    }
    conn_num_ = conn_num;
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
    // 由于线程池先销毁（假定这意味着全部线程都已经结束），这就说明外借的连接都已悉数归还
    // 这样就保证里所有的连接都会被 close
    std::unique_lock<std::mutex> locker(mtx_);
    while (!conns_que_.empty()) {
        auto conn = conns_que_.front();
        conns_que_.pop();
        mysql_close(conn);
    }
    mysql_server_end();
}
