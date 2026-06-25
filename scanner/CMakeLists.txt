#
# Scanner library
#
add_library(scanner STATIC
    scanner.c
)
target_include_directories(scanner PUBLIC .)

#
# Tests for scanner (including the textbook chapter tests)
#
add_executable(scanner-tests EXCLUDE_FROM_ALL
    tests.cpp
    chapter1_tests.cpp
    chapter6_tests.cpp
    chapter11_tests.cpp
    chapter12_tests.cpp
    chapter13_tests.cpp
    chapter16_tests.cpp
)
target_link_libraries(scanner-tests scanner GTest::gtest_main)
gtest_discover_tests(scanner-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)
