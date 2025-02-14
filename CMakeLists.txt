cmake_minimum_required(VERSION 3.10)

project(tirpc VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -Wall -Wno-deprecated -Wno-unused-but-set-variable -fno-omit-frame-pointer")
enable_language(ASM)

include_directories(${PROJECT_SOURCE_DIR})

# *.cc
file(GLOB COMM ${PROJECT_SOURCE_DIR}/tirpc/common/*.cpp)
aux_source_directory(${PROJECT_SOURCE_DIR}/tirpc/coroutine COR)
aux_source_directory(${PROJECT_SOURCE_DIR}/tirpc/net/base NETBASE)
aux_source_directory(${PROJECT_SOURCE_DIR}/tirpc/net/rpc RPC)
aux_source_directory(${PROJECT_SOURCE_DIR}/tirpc/net/http HTTP)
aux_source_directory(${PROJECT_SOURCE_DIR}/tirpc/net/tcp TCP)

set(COXTX ${PROJECT_SOURCE_DIR}/tirpc/coroutine/coctx_swap.s)

add_library(tirpc ${COMM} ${TCP} ${COXTX} ${COR} ${NETBASE} ${HTTP} ${RPC})


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


set(LIBS
    tirpc
    ${Protobuf_LIBRARY}
    tinyxml2
    dl
    pthread
)

add_subdirectory(example)
