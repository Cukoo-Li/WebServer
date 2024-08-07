// Author: Cukoo
// Date: 2024-07-03


#ifndef SQL_CONN_GUARD_H
#define SQL_CONN_GUARD_H
#include "pool/sqlconnpool.h"
#include <assert.h>

class SqlConnGuard {
   public:
    SqlConnGuard(MYSQL** sql, SqlConnPool* conn_pool) {
        assert(conn_pool);
        *sql = conn_pool->BorrowConn();
        sql_ = *sql;
        conn_pool_ = conn_pool;
    }

    ~SqlConnGuard() {
        if (sql_) {
            conn_pool_->ReturnConn(sql_);
        }
    }

   private:
    MYSQL* sql_;
    SqlConnPool* conn_pool_;
};

#endif