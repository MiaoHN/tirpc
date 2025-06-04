set(http_server
  ${CMAKE_CURRENT_SOURCE_DIR}/http_server.cpp
)

add_executable(http_server ${http_server})
target_link_libraries(http_server ${LIBS})

add_custom_command(TARGET http_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/conf
        $<TARGET_FILE_DIR:http_server>/conf
    COMMENT "Copying configuration files to executable directory"
)

add_custom_command(TARGET http_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/root
        $<TARGET_FILE_DIR:http_server>/root
    COMMENT "Copying configuration files to executable directory"
)