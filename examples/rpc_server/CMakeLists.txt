find_package(Protobuf REQUIRED)

set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/rpc_server.proto)
protobuf_generate_cpp(PROTO_SRC PROTO_HDR ${PROTO_FILE})

message(STATUS "Generated source: ${PROTO_SRC}")
message(STATUS "Generated header: ${PROTO_HDR}")

add_library(proto_lib STATIC ${PROTO_SRC} ${PROTO_HDR})

get_filename_component(PROTO_GENERATED_DIR ${PROTO_HDR} DIRECTORY)

target_include_directories(proto_lib PUBLIC
  ${PROTO_GENERATED_DIR}
  ${Protobuf_INCLUDE_DIRS}
)
target_link_libraries(proto_lib ${Protobuf_LIBRARIES})

add_executable(rpc_server rpc_server.cpp)
target_link_libraries(rpc_server proto_lib tirpc)

add_executable(rpc_client rpc_client.cpp)
target_link_libraries(rpc_client proto_lib tirpc)

# 复制 *.yml 到可执行文件所在目录
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/rpc_server.yml DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/rpc_client.yml DESTINATION ${CMAKE_CURRENT_BINARY_DIR})