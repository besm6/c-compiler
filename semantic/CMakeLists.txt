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
# Tests for symtab
#
add_executable(symtab-tests
   symtab_tests.cpp
)
target_link_libraries(symtab-tests semantic GTest::gtest_main)
gtest_discover_tests(symtab-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)

#
# Tests for structtab
#
add_executable(structtab-tests
   structtab_tests.cpp
)
target_link_libraries(structtab-tests semantic GTest::gtest_main)
gtest_discover_tests(structtab-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)

#
# Tests for typetab
#
add_executable(typetab-tests
   typetab_tests.cpp
)
target_link_libraries(typetab-tests semantic GTest::gtest_main)
gtest_discover_tests(typetab-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)

#
# Tests for typecheck
#
add_executable(typecheck-tests
   typecheck_tests.cpp
)
target_link_libraries(typecheck-tests parser semantic GTest::gtest_main)
gtest_discover_tests(typecheck-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)

#
# Tests for const_convert
#
add_executable(const-convert-tests
   const_convert_tests.cpp
)
target_link_libraries(const-convert-tests semantic GTest::gtest_main)
gtest_discover_tests(const-convert-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)
