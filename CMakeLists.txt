cmake_minimum_required(VERSION 3.5)

project(syncclient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
ADD_DEFINITIONS("-DELPP_THREAD_SAFE")
ADD_DEFINITIONS("-DELPP_NO_DEFAULT_LOG_FILE")
SET(SYNC_MESS_H ${PROJECT_SOURCE_DIR}/sync_mess.pb.h)
SET(SYNC_MESS_SOURCE ${PROJECT_SOURCE_DIR}/sync_mess.pb.cpp)
add_custom_command(OUTPUT ${SYNC_MESS_SOURCE} ${SYNC_MESS_H} COMMAND ./protoc.sh WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
if(NOT MSVC)
  set(CMAKE_CXX_FLAGS "-Wall -Werror -Wextra -std=c++17 -ggdb -O0 -Wfatal-errors " )
endif()
SET(SOURCE
  easylogging++.cpp easylogging++.h
  sync_client.cpp  sync_client.h
  sync_package.cpp sync_package.h
  ${SYNC_MESS_H} ${SYNC_MESS_SOURCE}
  buffer.cpp buffer.h
  uv_client.cpp uv_client.h
  decoder.h)
if (WIN32)
  include_directories("D://path/include")
  include_directories("D://path/boost_1_70_0")
  link_directories("D://path/lib")
  link_directories("D://path/boost_1_70_0/stage/lib")
ELSE()
  include_directories("/boost_1_72_0")
  link_directories("/boost_1_72_0/stage/lib")
endif()
find_library(UV_FOUND uv)
IF(WIN32)
  find_library(PROTOBUF_FOUND libprotobuf)
  ADD_DEFINITIONS("-D_ITERATOR_DEBUG_LEVEL=0")
  ADD_DEFINITIONS("-D_ALLOW_ITERATOR_DEBUG_LEVEL_MISMATCH")
ELSE()
  find_library(PROTOBUF_FOUND protobuf)
  find_library(BOOST_FILESYSTEM_FOUND name boost_filesystem PATHS /boost_1_72_0/stage/lib)
ENDIF(WIN32)
IF(NOT UV_FOUND)
  MESSAGE(FATAL ERROR "uv not found")
ENDIF(NOT UV_FOUND)
MESSAGE(STATUS ${UV_FOUND} " found")

IF(NOT PROTOBUF_FOUND)
  MESSAGE(FATAL ERROR "protobuf not found")
ENDIF(NOT PROTOBUF_FOUND)
MESSAGE(STATUS ${PROTOBUF_FOUND} " found")

IF(NOT BOOST_FILESYSTEM_FOUND)
  MESSAGE(FATAL ERROR "boost_filesystem not found")
ENDIF(NOT BOOST_FILESYSTEM_FOUND)
  MESSAGE(STATUS ${BOOST_FILESYSTEM_FOUND} " found")

add_executable(syncclient main.cpp ${SOURCE})
if (WIN32)
  target_link_libraries(syncclient ${UV_FOUND} ${PROTOBUF_FOUND} libboost_filesystem-vc142-mt-gd-x64-1_70)
ELSE()
  target_link_libraries(syncclient ${UV_FOUND} ${PROTOBUF_FOUND} ${BOOST_FILESYSTEM_FOUND})
ENDIF()
