cmake_minimum_required(VERSION 3.10)

project(tirpc VERSION 1.0.1)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -Wall -Wno-deprecated -Wno-unused-but-set-variable -fno-omit-frame-pointer")
enable_language(ASM)

# 查找 ZooKeeper 库
find_library(ZOOKEEPER_LIBRARY zookeeper_mt)
find_path(ZOOKEEPER_INCLUDE_DIR zookeeper/zookeeper.h)

# 查找 yaml-cpp 库
find_package(yaml-cpp REQUIRED)

# 检查是否找到 ZooKeeper 库和头文件
if(NOT ZOOKEEPER_LIBRARY OR NOT ZOOKEEPER_INCLUDE_DIR)
    message(FATAL_ERROR "ZooKeeper library not found!")
endif()


find_package(Protobuf REQUIRED)
if(${PROTOBUF_FOUND})
    message("protobuf found")
else()
    message(FATAL_ERROR "Cannot find Protobuf")
endif()

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include_directories(${PROJECT_SOURCE_DIR} ${ZOOKEEPER_INCLUDE_DIR})

enable_testing()

add_subdirectory(tirpc)

add_subdirectory(tests)

add_subdirectory(examples)
