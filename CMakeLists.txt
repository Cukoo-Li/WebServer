cmake_minimum_required(VERSION 3.0)

project(webserver)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_COMPILER "g++")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLGS} -Wall")

set(CMAKE_BUILD_TYPE Release)

include_directories(include)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(webserver ${SOURCES})

target_link_libraries(webserver mysqlclient spdlog fmt)
