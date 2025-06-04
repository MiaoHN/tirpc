# *.cc
file(GLOB COMM common/*.cpp)
aux_source_directory(coroutine COR)
aux_source_directory(net/base NETBASE)
aux_source_directory(net/service_register SERVICE)
aux_source_directory(net/rpc RPC)
aux_source_directory(net/http HTTP)
aux_source_directory(net/tcp TCP)

set(COXTX coroutine/coctx_swap.s)

add_library(tirpc ${COMM} ${TCP} ${COXTX} ${COR} ${NETBASE} ${SERVICE} ${HTTP} ${RPC})

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