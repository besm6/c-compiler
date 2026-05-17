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
# Tests for string map, wio and xalloc
#
add_executable(libutil-tests EXCLUDE_FROM_ALL
    string_map_tests.cpp
    wio_tests.cpp
    xalloc_tests.cpp
)
target_link_libraries(libutil-tests libutil GTest::gtest_main)
gtest_discover_tests(libutil-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)
