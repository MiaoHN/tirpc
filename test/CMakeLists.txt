add_executable(test_zookeeper test_zookeeper.cpp)
target_link_libraries(test_zookeeper ${LIBS})

add_executable(test_raw_zk_util test_raw_zk_util.cpp)
add_executable(test_raw_zookeeper test_raw_zookeeper.cpp)

target_link_libraries(test_raw_zookeeper ${LIBS})
target_link_libraries(test_raw_zk_util ${LIBS})


add_custom_command(TARGET test_zookeeper POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/conf
        $<TARGET_FILE_DIR:test_zookeeper>/conf
    COMMENT "Copying configuration files to executable directory"
)