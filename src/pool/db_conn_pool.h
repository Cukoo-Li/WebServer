#ifndef DB_CONN_POOL
#define DB_CONN_POOL

#include <mysql/mysql.h>
#include <queue>
#include <string>
#include <assert.h>
#include "../itc/itc.h"

class DbConnPool {
   public:
    // 获取连接
    MYSQL* GetConnection();
    // 归还连接
    bool ReturnConnection(MYSQL* conn);
    // 初始化数据库连接池
    void Init(const char host[],
              const char user[],
              const char passwd[],
              const char db[],
              int port,
              int conn_count);

    // 单例模式
   public:
    static DbConnPool* Instance();
    DbConnPool(const DbConnPool&) = delete;
    DbConnPool& operator=(const DbConnPool&) = delete;

   private:
    DbConnPool();
    virtual ~DbConnPool();

   private:
    Locker que_locker_;
    Sem conn_resource_;

   private:
    const char* host_;             // 主机名
    const char* user_;             // 用户名
    const char* passwd_;           // 密码
    const char* db_;               // 数据库名称
    int port_;                     // 数据库端口号
    int conn_count_;               // 连接数量
    std::queue<MYSQL*> conn_que_;  // 连接池/连接队列
};

#endif