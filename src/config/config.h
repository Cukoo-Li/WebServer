// Author: Cukoo
// Date: 2024-07-09

#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <filesystem>
#include <spdlog/spdlog.h>

struct Config {
    std::string work_dir = std::filesystem::current_path().string() + "/resources";
    int port = 1027;
    bool enable_linger = true;
    int timeout = 10000;    // 单位：ms
    const char* host = "localhost";
    int sql_port = 3306;
    const char* sql_user = "root";
    const char* sql_pwd = "768787";
    const char* db_name = "webserver";
    int sql_conn_pool_size = 8;
    int thread_pool_size = 8;
    spdlog::level::level_enum log_level = spdlog::level::off;
};

#endif
