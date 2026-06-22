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
# const_fold.c queries the active Target (target_config) to wrap folded integer
# results to the target's signed/unsigned widths.  semantic provides target.{c,h}
# and does not depend on optimize, so this adds no link cycle.
target_link_libraries(optimize tac libutil semantic)

add_executable(optimizer-tests
    const_fold_tests.cpp
    type_conv_tests.cpp
    jump_unreachable_tests.cpp
    copy_prop_tests.cpp
    dead_store_tests.cpp
    pipeline_tests.cpp
)
target_link_libraries(optimizer-tests optimize parser translator tac GTest::gtest_main)
target_compile_definitions(optimizer-tests PRIVATE
    BESM6_CPP="${SystemCpp}"
    BESM6_INCLUDE_DIR="${BESM6_INCLUDE_DIR}")
gtest_discover_tests(optimizer-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)

#
# Book tests for optimizer (built and run only via `make book_tests`)
#
add_executable(optimizer-book-tests EXCLUDE_FROM_ALL
    chapter19_tests1.cpp
    chapter19_tests2.cpp
    chapter19_tests3.cpp
    chapter19_tests4.cpp
)
target_link_libraries(optimizer-book-tests optimize parser translator tac GTest::gtest_main)
target_compile_definitions(optimizer-book-tests PRIVATE
    BESM6_CPP="${SystemCpp}"
    BESM6_INCLUDE_DIR="${BESM6_INCLUDE_DIR}")
gtest_discover_tests(optimizer-book-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10 LABELS "book")
