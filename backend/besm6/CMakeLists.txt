#
# BESM-6 backend library
#
add_library(besm STATIC
    besm_alloc.c
    besm_free.c
    emit_madlen.c
    frame.c
    codegen.c
    emit.c
    static.c
    instr.c
    utf8_to_koi7.c
)
target_include_directories(besm PUBLIC .)
target_link_libraries(besm tac libutil)

#
# C runtime library: assemble *.madlen sources into libc.bin for the Dubna simulator.
#
set(LIBC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libc)
set(LIBC_MADLEN
    b_div b_eq b_ge b_gt b_le b_lt b_mod b_mul b_ne b_not
    b_ret b_save0 b_save b_tout b_true char exit lchar
    printd printo printf read write writeb flush
    b_udiv b_umod b_ult b_ule b_ugt b_uge b_dtoi b_dtou b_uneg
    b_lsh b_rsh b_uadd b_usub b_umul)
list(TRANSFORM LIBC_MADLEN PREPEND ${LIBC_DIR}/ OUTPUT_VARIABLE LIBC_SOURCES)
list(TRANSFORM LIBC_SOURCES APPEND .madlen)

add_custom_command(
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/libc.bin
    COMMAND besmc -c ${LIBC_SOURCES} -o ${CMAKE_CURRENT_BINARY_DIR}/libc.obj
    COMMAND ${CMAKE_COMMAND} -E rename
            ${CMAKE_CURRENT_BINARY_DIR}/libc.obj
            ${CMAKE_CURRENT_BINARY_DIR}/libc.bin
    DEPENDS ${LIBC_SOURCES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Assembling BESM-6 C runtime library (libc.bin)")

add_custom_target(besm-libc DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libc.bin)

#
# Tests for the BESM-6 backend
#
add_executable(besm-tests
    label_tests.cpp
    frame_tests.cpp
    codegen_tests.cpp
    arith_tests.cpp
    copy_tests.cpp
    flow_tests.cpp
    run_tests.cpp
    unary_tests.cpp
    convert_tests.cpp
    init_tests.cpp
    ptr_tests.cpp
    struct_tests.cpp
)
target_link_libraries(besm-tests besm parser translator semantic GTest::gtest_main)
add_dependencies(besm-tests besm-libc)
add_definitions(-DTEST_DIR="${CMAKE_CURRENT_BINARY_DIR}")
gtest_discover_tests(besm-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)
