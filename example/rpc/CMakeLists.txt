set(rpc_http_server
  ${CMAKE_CURRENT_SOURCE_DIR}/rpc_http_server.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/rpc_server.pb.cc
)

add_executable(rpc_http_server ${rpc_http_server})
target_link_libraries(rpc_http_server ${LIBS})

# tinypb server/client
set(rpc_server
  ${CMAKE_CURRENT_SOURCE_DIR}/rpc_server.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/rpc_server.pb.cc
)
add_executable(rpc_server ${rpc_server})
target_link_libraries(rpc_server ${LIBS})

set(rpc_benchmark
  ${CMAKE_CURRENT_SOURCE_DIR}/rpc_benchmark.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/rpc_server.pb.cc
)
add_executable(rpc_benchmark ${rpc_benchmark})
target_link_libraries(rpc_benchmark ${LIBS})

set(rpc_client
  ${CMAKE_CURRENT_SOURCE_DIR}/rpc_client.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/rpc_server.pb.cc
)
add_executable(rpc_client ${rpc_client})
target_link_libraries(rpc_client ${LIBS})

# 复制配置文件到可执行文件所在目录
add_custom_command(TARGET rpc_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/conf
        $<TARGET_FILE_DIR:rpc_server>/conf
    COMMENT "Copying configuration files to executable directory"
)

add_custom_command(TARGET rpc_http_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/conf
        $<TARGET_FILE_DIR:rpc_http_server>/conf
    COMMENT "Copying configuration files to executable directory"
)