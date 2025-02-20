cmake_minimum_required(VERSION 3.10)

project(tirpc VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -Wall -Wno-deprecated -Wno-unused-but-set-variable -fno-omit-frame-pointer")
enable_language(ASM)

# 查找 ZooKeeper 库
find_library(ZOOKEEPER_LIBRARY zookeeper_mt)
find_path(ZOOKEEPER_INCLUDE_DIR zookeeper/zookeeper.h)

# 检查是否找到 ZooKeeper 库和头文件
if(NOT ZOOKEEPER_LIBRARY OR NOT ZOOKEEPER_INCLUDE_DIR)
    message(FATAL_ERROR "ZooKeeper library not found!")
endif()


find_package(Protobuf REQUIRED)
if (${PROTOBUF_FOUND})
    message("protobuf found")
else ()
    message(FATAL_ERROR "Cannot find Protobuf")
endif ()

include(FetchContent)

FetchContent_Declare(
    tinyxml2
    GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
    GIT_TAG 10.0.0
)

FetchContent_GetProperties(tinyxml2)

if (NOT tinyxml2_POPULATED)
    FetchContent_Populate(tinyxml2)
    add_subdirectory(${tinyxml2_SOURCE_DIR} ${tinyxml2_BINARY_DIR})
endif ()


include_directories(${PROJECT_SOURCE_DIR} ${ZOOKEEPER_INCLUDE_DIR})

# *.cc
file(GLOB COMM ${PROJECT_SOURCE_DIR}/tirpc/common/*.cpp)
aux_source_directory(${PROJECT_SOURCE_DIR}/tirpc/coroutine COR)
aux_source_directory(${PROJECT_SOURCE_DIR}/tirpc/net/base NETBASE)
aux_source_directory(${PROJECT_SOURCE_DIR}/tirpc/net/service_register SERVICE)
aux_source_directory(${PROJECT_SOURCE_DIR}/tirpc/net/rpc RPC)
aux_source_directory(${PROJECT_SOURCE_DIR}/tirpc/net/http HTTP)
aux_source_directory(${PROJECT_SOURCE_DIR}/tirpc/net/tcp TCP)

set(COXTX ${PROJECT_SOURCE_DIR}/tirpc/coroutine/coctx_swap.s)

add_library(tirpc ${COMM} ${TCP} ${COXTX} ${COR} ${NETBASE} ${SERVICE} ${HTTP} ${RPC})

target_link_libraries(tirpc
    protobuf
    tinyxml2
    dl
    pthread
    zookeeper_mt
)

set(LIBS
    tirpc
    protobuf
    tinyxml2
    dl
    pthread
    zookeeper_mt
)

add_subdirectory(test)
add_subdirectory(example)
