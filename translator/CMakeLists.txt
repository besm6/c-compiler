#
# Translator library
#
add_library(translator STATIC
    translator.c
#   symtab.c
)
target_include_directories(translator PUBLIC .)
target_link_libraries(translator tac ast libutil)

#
# Tests for translator
#
#add_executable(translator-tests
#   tac_type_tests.cpp
#   tac_grammar_tests.cpp
#)
#target_link_libraries(translator-tests translator GTest::gtest_main)
#gtest_discover_tests(translator-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)
