#
# Parser library
#
add_library(libutil STATIC
    string_map.c
    xalloc.c
    wio.c
)
target_include_directories(libutil PUBLIC .)
target_link_libraries(libutil m)

#
# Tests for string map
#
add_executable(libutil-tests
    string_map_tests.cpp
)
target_link_libraries(libutil-tests libutil GTest::gtest_main)
gtest_discover_tests(libutil-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)

#
# Tests for wio
#
add_executable(wio-tests
    wio_tests.cpp
)
target_link_libraries(wio-tests libutil GTest::gtest_main)
gtest_discover_tests(wio-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)
