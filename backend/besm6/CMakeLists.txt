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
add_executable(besm-tests
    madlen_tests.cpp
    frame_tests.cpp
    codegen_tests.cpp
)
target_link_libraries(besm-tests besm GTest::gtest_main)
gtest_discover_tests(besm-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)
