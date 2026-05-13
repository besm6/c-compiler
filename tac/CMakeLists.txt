#
# TAC library
#
add_library(tac STATIC
    tac_alloc.c
    tac_print.c
    tac_free.c
    tac_compare.c
    tac_yaml.c
# TODO:
#   tac_import.c
    tac_export.c
    tac_graphviz.c
)
target_include_directories(tac PUBLIC .)
target_link_libraries(tac libutil)

#
# Tests for tac_yaml
#
add_executable(tac-yaml-tests
    tac_yaml_tests.cpp
)
target_link_libraries(tac-yaml-tests tac GTest::gtest_main)
gtest_discover_tests(tac-yaml-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)
