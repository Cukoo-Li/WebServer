cmake_minimum_required(VERSION 3.0)

project(webserver)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLGS} -Wall")

set(CMAKE_BUILD_TYPE Release)

add_executable(webserver main.cpp ./server/webserver.cpp ./server/epoller.cpp ./http/httpconn.cpp ./http/httprequest.cpp ./http/httpresponse.cpp ./timer/timerheap.cpp ./pool/sqlconnpool.cpp ./pool/sqlconnguard.h ./pool/threadpool.h ./buffer/buffer.cpp ./config/config.h)

target_link_libraries(webserver spdlog fmt mysqlclient)
