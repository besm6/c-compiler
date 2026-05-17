#
# Translator library
#
add_library(semantic STATIC
    symtab.c
    symtab_print.c
    structtab.c
    structtab_print.c
    typetab.c
    typetab_print.c
    typecheck.c
    expressions.c
    initializers.c
    statements.c
    declarations.c
    type_utils.c
    const_convert.c
    label_loops.c
)
target_include_directories(semantic PUBLIC .)
target_link_libraries(semantic tac ast libutil)

#
# Tests for semantic
#
add_executable(semantic-tests
   symtab_tests.cpp
   structtab_tests.cpp
   typetab_tests.cpp
   typecheck_tests.cpp
   typecheck_real_tests.cpp
   pipeline_tests.cpp
   label_loops_tests.cpp
   const_convert_tests.cpp
   coercion_tests.cpp
)
target_link_libraries(semantic-tests parser semantic GTest::gtest_main)
gtest_discover_tests(semantic-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)
