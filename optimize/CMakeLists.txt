add_library(optimize STATIC
    optimize.c
    const_fold.c
    cfg.c
    unreachable.c
    alias.c
    copy_prop.c
    dead_store.c
)
target_include_directories(optimize PUBLIC .)
target_link_libraries(optimize tac libutil)

add_executable(optimizer-tests EXCLUDE_FROM_ALL
    const_fold_tests.cpp
    type_conv_tests.cpp
    jump_unreachable_tests.cpp
    copy_prop_tests.cpp
    dead_store_tests.cpp
    pipeline_tests.cpp
)
target_link_libraries(optimizer-tests optimize parser translator tac GTest::gtest_main)
gtest_discover_tests(optimizer-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)
