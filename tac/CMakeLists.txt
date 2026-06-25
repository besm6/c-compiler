#
# TAC library
#
add_library(tac STATIC
    tac_alloc.c
    tac_print.c
    tac_free.c
    tac_compare.c
    tac_yaml.c
    tac_import.c
    tac_export.c
    tac_graphviz.c
)
target_include_directories(tac PUBLIC .)
target_link_libraries(tac libutil)

#
# Tests for tac binary export/import, tac_yaml and tac_graphviz
#
add_executable(tac-tests EXCLUDE_FROM_ALL
    binary_tests.cpp
    yaml_tests.cpp
    graphviz_tests.cpp
)
target_link_libraries(tac-tests tac GTest::gtest_main)
gtest_discover_tests(tac-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)
