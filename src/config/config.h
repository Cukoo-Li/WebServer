#ifndef CONFIG_H
#define CONFIG_H

struct Config {
    const char* work_dir;
    int port;
    bool enable_linger;
    int timeout;    // 客户连接超时时间
    const char* host;
    int sql_port;
    const char* sql_user;
    const char* sql_pwd;
    const char* db_name;
    int sql_conn_pool_size;
    int thread_pool_size;
    // bool enable_log;
    // int log_level;
    // int log_que_size;
};

#endif