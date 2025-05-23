#
# Parser library
#
add_library(parser STATIC
    parser.c
    symtab.c
)
target_include_directories(parser PUBLIC .)
target_link_libraries(parser scanner ast libutil)

#
# Tests for parser
#
add_executable(parser-tests
    simple_tests.cpp
    statement_tests.cpp
    operator_tests.cpp
    type_tests.cpp
    struct_tests.cpp
    declaration_tests.cpp
    constant_tests.cpp
    serialize_tests.cpp
#   negative_tests.cpp
)
target_link_libraries(parser-tests parser GTest::gtest_main)
gtest_discover_tests(parser-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)
