#ifndef DB_CONN_RAII
#define DB_CONN_RAII

#include "db_conn_pool.h"

class DbConnRAII {
   public:
    DbConnRAII(MYSQL** db_conn, DbConnPool* db_conn_pool) {
        assert(db_conn_pool);
        *db_conn = db_conn_pool->GetConnection();
        db_conn_ = *db_conn;
        db_conn_pool_ = db_conn_pool;
    }
    ~DbConnRAII() { db_conn_pool_->ReturnConnection(db_conn_); }

   private:
    MYSQL* db_conn_ = nullptr;
    DbConnPool* db_conn_pool_ = nullptr;
};

#endif