cmake_minimum_required(VERSION 3.5)

project(syncclient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
ADD_DEFINITIONS("-DELPP_THREAD_SAFE")
ADD_DEFINITIONS("-DELPP_NO_DEFAULT_LOG_FILE")
SET(SOURCE easylogging++.cpp)
add_library(sync SHARED ${SOURCE})
target_link_libraries(sync uv)
add_executable(syncclient main.cpp)
target_link_libraries(syncclient sync)
