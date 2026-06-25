#
# Translator library
#
add_library(translator STATIC
    translate.c
    expr.c
    stmt.c
)
target_include_directories(translator PUBLIC .)
target_link_libraries(translator optimize semantic tac ast libutil)

#
# Tests for translate
#
add_executable(translate-tests EXCLUDE_FROM_ALL
    decl_tests.cpp
    expr_tests.cpp
    stmt_tests.cpp
    cast_tests.cpp
    incdec_tests.cpp
    switch_tests.cpp
    ptr_tests.cpp
    struct_tests.cpp
)
target_link_libraries(translate-tests parser translator GTest::gtest_main)
target_compile_definitions(translate-tests PRIVATE
    BESM6_CPP="${SystemCpp}"
    BESM6_INCLUDE_DIR="${BESM6_INCLUDE_DIR}")
gtest_discover_tests(translate-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)
