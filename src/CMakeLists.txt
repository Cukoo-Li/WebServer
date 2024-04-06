cmake_minimum_required(VERSION 3.0)

project(webserver)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLGS} -Wall")

set(CMAKE_BUILD_TYPE Debug)

add_executable(webserver main.cpp ./pool/thread_pool.h ./http/http_conn.cpp ./http/http_conn.h ./itc/itc.h ./timer/timer_pile.h ./timer/timer_pile.cpp ./pool/db_conn_pool.h ./pool/db_conn_pool.cpp ./pool/db_conn_raii.h)

target_link_libraries(webserver pthread mysqlclient)