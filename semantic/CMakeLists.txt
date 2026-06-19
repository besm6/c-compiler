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
    resolve_labels.c
    target.c
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
   real_tests.cpp
   pipeline_tests.cpp
   label_loops_tests.cpp
   const_convert_tests.cpp
   coercion_tests.cpp
   chapter3_tests.cpp
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
target_link_libraries(semantic-tests parser semantic GTest::gtest_main)
gtest_discover_tests(semantic-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)
