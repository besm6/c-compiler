add_library(optimize STATIC
    optimize.c
    const_fold.c
    cfg.c
    unreachable.c
    copy_prop.c
    dead_store.c
)
target_include_directories(optimize PUBLIC .)
target_link_libraries(optimize tac libutil)

add_executable(optimizer-tests EXCLUDE_FROM_ALL
    optimizer_tests.cpp
)
target_link_libraries(optimizer-tests optimize tac GTest::gtest_main)
gtest_discover_tests(optimizer-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)
