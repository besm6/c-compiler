#
# Translator library
#
add_library(translator STATIC
    translator.c
    symtab.c
    typetab.c
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
# Tests for typetab
#
add_executable(typetab-tests
   typetab_tests.cpp
)
target_link_libraries(typetab-tests translator GTest::gtest_main)
gtest_discover_tests(typetab-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)

#
# Tests for typecheck
#
add_executable(typecheck-tests
   typecheck_tests.cpp
)
target_link_libraries(typecheck-tests parser translator GTest::gtest_main)
gtest_discover_tests(typecheck-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)
