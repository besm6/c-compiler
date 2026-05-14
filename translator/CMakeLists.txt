#
# Translator library
#
add_library(translator STATIC
    translate.c
)
target_include_directories(translator PUBLIC .)
target_link_libraries(translator semantic tac ast libutil)

#
# Tests for translate
#
add_executable(translate-tests
   translate_tests.cpp
)
target_link_libraries(translate-tests parser translator GTest::gtest_main)
gtest_discover_tests(translate-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)
