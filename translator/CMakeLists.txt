#
# Translator library
#
add_library(translator STATIC
    translate.c
    symtab.c
    symtab_print.c
    structtab.c
    structtab_print.c
    typecheck.c
    type_utils.c
    const_convert.c
)
target_include_directories(translator PUBLIC .)
target_link_libraries(translator tac ast libutil)

#
# Tests for symtab
#
add_executable(symtab-tests
   symtab_tests.cpp
)
target_link_libraries(symtab-tests translator GTest::gtest_main)
gtest_discover_tests(symtab-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)

#
# Tests for structtab
#
add_executable(structtab-tests
   structtab_tests.cpp
)
target_link_libraries(structtab-tests translator GTest::gtest_main)
gtest_discover_tests(structtab-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)

#
# Tests for typecheck
#
add_executable(typecheck-tests
   typecheck_tests.cpp
)
target_link_libraries(typecheck-tests parser translator GTest::gtest_main)
gtest_discover_tests(typecheck-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)
