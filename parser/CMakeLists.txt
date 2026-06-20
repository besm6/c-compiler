#
# Parser library
#
add_library(parser STATIC
    parser.c
    expr.c
    decl.c
    stmt.c
    nametab.c
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
    negative_tests.cpp
)
target_link_libraries(parser-tests parser GTest::gtest_main)
gtest_discover_tests(parser-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)

#
# Book tests for parser (built and run only via `make book_tests`)
#
add_executable(parser-book-tests EXCLUDE_FROM_ALL
    chapter1_tests.cpp
    chapter2_tests.cpp
    chapter3_tests.cpp
    chapter4_tests.cpp
    chapter5_tests.cpp
    chapter6_tests.cpp
    chapter7_tests.cpp
    chapter8_tests.cpp
    chapter9_tests.cpp
    chapter10_tests.cpp
    chapter11_tests.cpp
    chapter12_tests.cpp
    chapter13_tests.cpp
    chapter14_tests.cpp
    chapter15_tests.cpp
    chapter16_tests.cpp
    chapter17_tests.cpp
    chapter18_tests.cpp
)
target_link_libraries(parser-book-tests parser GTest::gtest_main)
gtest_discover_tests(parser-book-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10 LABELS "book")
