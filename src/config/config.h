#ifndef CONFIG_H
#define CONFIG_H

#include <string>

struct Config {
    std::string work_dir = "/home/cukoo/文档/GitHub-Repositories/WebServer-Modern-Cpp/resources";
    int port = 1027;
    bool enable_linger = true;
    int timeout = 10000;    // 客户连接超时时间
    const char* host = "localhost";
    int sql_port = 3306;
    const char* sql_user = "root";
    const char* sql_pwd = "768787";
    const char* db_name = "webserver";
    int sql_conn_pool_size = 8;
    int thread_pool_size = 8;
};

#endif