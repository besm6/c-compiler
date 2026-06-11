#
# Scanner library
#
add_library(scanner STATIC
    scanner.c
)
target_include_directories(scanner PUBLIC .)

#
# Tests for scanner
#
add_executable(scanner-tests
    tests.cpp
)
target_link_libraries(scanner-tests scanner GTest::gtest_main)
gtest_discover_tests(scanner-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)
