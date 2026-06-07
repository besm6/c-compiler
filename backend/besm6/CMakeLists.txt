#
# BESM-6 backend library
#
add_library(besm STATIC
    besm_alloc.c
    besm_free.c
    emit_madlen.c
    frame.c
    codegen.c
)
target_include_directories(besm PUBLIC .)
target_link_libraries(besm tac libutil)

#
# Tests for the BESM-6 backend
#
add_executable(besm-tests #EXCLUDE_FROM_ALL
    label_tests.cpp
    frame_tests.cpp
    codegen_tests.cpp
    init_tests.cpp
)
target_link_libraries(besm-tests besm parser translator semantic GTest::gtest_main)
add_definitions(-DTEST_DIR="${CMAKE_CURRENT_BINARY_DIR}")
gtest_discover_tests(besm-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)
