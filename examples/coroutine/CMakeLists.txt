set(coroutine ${CMAKE_CURRENT_SOURCE_DIR}/coroutine.cpp)

add_executable(coroutine ${coroutine})
target_link_libraries(coroutine ${LIBS})