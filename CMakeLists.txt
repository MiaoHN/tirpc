cmake_minimum_required(VERSION 3.0)

project(tirpc)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -std=c++11 -Wall -Wno-deprecated -Wno-unused-but-set-variable -fno-omit-frame-pointer")
enable_language(ASM)

include_directories(${PROJECT_SOURCE_DIR})

# *.cc
file(GLOB COMM ${PROJECT_SOURCE_DIR}/tirpc/common/*.cpp)
aux_source_directory(${PROJECT_SOURCE_DIR}/tirpc/coroutine COR)
aux_source_directory(${PROJECT_SOURCE_DIR}/tirpc/net/base NETBASE)
aux_source_directory(${PROJECT_SOURCE_DIR}/tirpc/net/rpc RPC)
aux_source_directory(${PROJECT_SOURCE_DIR}/tirpc/net/http HTTP)
aux_source_directory(${PROJECT_SOURCE_DIR}/tirpc/net/tcp TCP)
aux_source_directory(${PROJECT_SOURCE_DIR}/tirpc/net/tinypb TINYPB)

set(COXTX ${PROJECT_SOURCE_DIR}/tirpc/coroutine/coctx_swap.s)

add_library(tirpc ${COMM} ${TCP} ${COXTX} ${COR} ${NETBASE} ${HTTP} ${TINYPB} ${RPC})


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

set(coroutine ${PROJECT_SOURCE_DIR}/example/coroutine.cpp)

add_executable(coroutine ${coroutine})
target_link_libraries(coroutine ${LIBS})

set(http_server
  ${PROJECT_SOURCE_DIR}/example/http_server.cpp
  ${PROJECT_SOURCE_DIR}/example/rpc_server.pb.cc
)

add_executable(http_server ${http_server})
target_link_libraries(http_server ${LIBS})

# tinypb server/client
set(rpc_server
  ${PROJECT_SOURCE_DIR}/example/rpc_server.cpp
  ${PROJECT_SOURCE_DIR}/example/rpc_server.pb.cc
)
add_executable(rpc_server ${rpc_server})
target_link_libraries(rpc_server ${LIBS})

set(rpc_client
  ${PROJECT_SOURCE_DIR}/example/rpc_client.cpp
  ${PROJECT_SOURCE_DIR}/example/rpc_server.pb.cc
)
add_executable(rpc_client ${rpc_client})
target_link_libraries(rpc_client ${LIBS})