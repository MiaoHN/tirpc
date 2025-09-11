include(GoogleTest)

add_executable(simple_test simple_test.cpp)
target_link_libraries(simple_test GTest::gtest_main pthread)
add_test(NAME simple_test COMMAND simple_test)
gtest_discover_tests(simple_test)

add_executable(coroutine_test coroutine_test.cpp)
target_link_libraries(coroutine_test GTest::gtest_main tirpc)
add_test(NAME coroutine_test COMMAND coroutine_test)
gtest_discover_tests(coroutine_test)

add_executable(test_config test_config.cpp)
target_link_libraries(test_config GTest::gtest_main tirpc)
add_test(NAME test_config COMMAND test_config)
gtest_discover_tests(test_config)
