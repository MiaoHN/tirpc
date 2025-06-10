file(GLOB TIRPC_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/common/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/coroutine/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/net/base/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/net/service_register/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/net/rpc/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/net/http/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/net/tcp/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/coroutine/coctx_swap.s
)

add_library(tirpc ${TIRPC_SOURCES})

target_link_libraries(tirpc
    protobuf
    dl
    pthread
    zookeeper_mt
    yaml-cpp
)

set(LIBS
    tirpc
    protobuf
    dl
    pthread
    zookeeper_mt
    yaml-cpp
)
