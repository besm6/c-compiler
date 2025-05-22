#
# Parser library
#
add_library(parser STATIC
    parser.c
    ast_alloc.c
    ast_print.c
    ast_free.c
    ast_clone.c
    ast_yaml.c
    ast_graphviz.c
    ast_export.c
    ast_import.c
    ast_compare.c
    symtab.c
)
target_include_directories(parser PUBLIC .)
target_link_libraries(parser scanner wio libutil)

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
target_link_libraries(parser-tests parser scanner GTest::gtest_main)
gtest_discover_tests(parser-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)
