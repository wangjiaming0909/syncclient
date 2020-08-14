cmake_minimum_required(VERSION 3.5)

project(syncclient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
ADD_DEFINITIONS("-DELPP_THREAD_SAFE")
ADD_DEFINITIONS("-DELPP_NO_DEFAULT_LOG_FILE")
if(NOT MSVC)
    set(CMAKE_CXX_FLAGS "-Wall -Wextra -std=c++17 -ggdb -O0 -Wfatal-errors " )
endif()
SET(SOURCE
    easylogging++.cpp easylogging++.h
    sync_client.cpp  sync_client.h
    sync_package.cpp sync_package.h
    sync_mess.pb.cpp sync_mess.pb.h
    buffer.cpp buffer.h
    uv_client.cpp uv_client.h
    decoder.h)
if (WIN32)
	include_directories("D://path/include")
    include_directories("D://path/boost_1_70_0")
    link_directories("D://path/lib")
endif()
find_library(UV_FOUND uv)
IF(WIN32)
    find_library(PROTOBUF_FOUND libprotobuf)
ELSE()
    find_library(PROTOBUF_FOUND protobuf)
ENDIF(WIN32)
IF(NOT UV_FOUND)
    MESSAGE(FATAL ERROR "uv not found")
ENDIF(NOT UV_FOUND)
MESSAGE(STATUS ${UV_FOUND} " found")

IF(NOT PROTOBUF_FOUND)
    MESSAGE(FATAL ERROR "protobuf not found")
ENDIF(NOT PROTOBUF_FOUND)
MESSAGE(STATUS ${PROTOBUF_FOUND} " found")

add_executable(syncclient main.cpp ${SOURCE})
target_link_libraries(syncclient  uv libprotobuf)
