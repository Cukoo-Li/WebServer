#include "db_conn_pool.h"

DbConnPool::DbConnPool() : conn_count_(0) {}

DbConnPool::~DbConnPool() {
    // 如果没归还完怎么办？
    que_locker_.Lock();
    while(!conn_que_.empty()) {
        mysql_close(conn_que_.front());
        conn_que_.pop();
    }
    que_locker_.Unlock();
}

DbConnPool* DbConnPool::Instance() {
    static DbConnPool instance;
    return &instance;
}

// 初始化数据库连接池
void DbConnPool::Init(const char host[],
                      const char user[],
                      const char passwd[],
                      const char db[],
                      int port,
                      int conn_count) {
    host_ = host;
    user_ = user;
    passwd_ = passwd;
    db_ = db;
    port_ = port;
    conn_count_ = conn_count;

    que_locker_.Lock();
    for (int i = 0; i < conn_count_; ++i) {
        MYSQL* conn = nullptr;
        conn = mysql_init(conn);
        assert(conn);
        conn = mysql_real_connect(conn, host, user, passwd, db, port, nullptr, 0);
        assert(conn);
        conn_que_.push(conn);
        conn_resource_.Post();
    }
    que_locker_.Unlock();
}

// 获取连接
MYSQL* DbConnPool::GetConnection() {
    if (conn_count_ <= 0)
        return nullptr;
    conn_resource_.Wait();
    que_locker_.Lock();
    MYSQL* conn = conn_que_.front();
    conn_que_.pop();
    que_locker_.Unlock();
    return conn;
}

// 归还连接
bool DbConnPool::ReturnConnection(MYSQL* conn) {
    if (!conn)
        return false;
    que_locker_.Lock();
    conn_que_.push(conn);
    que_locker_.Unlock();
    conn_resource_.Post();
    return true;
}