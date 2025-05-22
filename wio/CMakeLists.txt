#
# Word-oriented buffered I/O library
#
add_library(wio STATIC
    wio.c
)
target_include_directories(wio PUBLIC .)
target_link_libraries(wio libutil)

#
# Tests for wio
#
add_executable(wio-tests
    wio_tests.cpp
)
target_link_libraries(wio-tests libutil wio GTest::gtest_main)
gtest_discover_tests(wio-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)
