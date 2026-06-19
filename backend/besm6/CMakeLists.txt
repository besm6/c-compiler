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
    peephole.c
    utf8_to_koi7.c
)
target_include_directories(besm PUBLIC .)
target_link_libraries(besm tac libutil)

#
# C runtime library: assemble the runtime helpers into libc.bin for the Dubna simulator.
#
# Two kinds of sources:
#   * hand-written Madlen helpers (LIBC_MADLEN) — calling convention, arithmetic,
#     comparisons, byte access, b/tout, exit;
#   * higher-level routines written in C (LIBC_C), compiled by our own toolchain
#     (parse -> lower -> genbesm) into Madlen and assembled alongside the helpers.
#
set(LIBC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libc)
set(LIBC_MADLEN
    b_div b_eq b_ge b_gt b_le b_lt b_mod b_mul b_ne b_not
    b_ret b_save0 b_save b_tout b_true char exit lchar
    b_udiv b_umod b_ult b_ule b_ugt b_uge b_dtoi b_dtou b_utod b_uneg
    b_lsh b_rsh b_uadd b_usub b_umul
    b_flt b_fle b_fgt b_fge b_stb b_padd b_pinc b_pdec b_pdiff)
list(TRANSFORM LIBC_MADLEN PREPEND ${LIBC_DIR}/ OUTPUT_VARIABLE LIBC_SOURCES)
list(TRANSFORM LIBC_SOURCES APPEND .madlen)

# C runtime routines compiled by our own pipeline.  Each NAME.c becomes NAME.madlen
# in the build directory via parse -> lower -> genbesm.  besmc only recognizes the
# .madlen extension, so genbesm writes there.
set(LIBC_C
    putbyte putch flush printf getch
    doprnt sprintf snprintf modf)
set(LIBC_C_MADLEN "")
foreach(name ${LIBC_C})
    set(c_src  ${LIBC_DIR}/${name}.c)
    set(c_ast  ${CMAKE_CURRENT_BINARY_DIR}/${name}.ast)
    set(c_tac  ${CMAKE_CURRENT_BINARY_DIR}/${name}.tac)
    set(c_mad  ${CMAKE_CURRENT_BINARY_DIR}/${name}.madlen)
    add_custom_command(
        OUTPUT  ${c_mad}
        COMMAND parse   ${c_src} ${c_ast}
        COMMAND lower   ${c_ast} ${c_tac}
        COMMAND genbesm ${c_tac} ${c_mad}
        DEPENDS ${c_src} parse lower genbesm
        COMMENT "Compiling libc/${name}.c -> ${name}.madlen")
    list(APPEND LIBC_C_MADLEN ${c_mad})
endforeach()

add_custom_command(
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/libc.bin
    COMMAND besmc -c ${LIBC_SOURCES} ${LIBC_C_MADLEN} -o ${CMAKE_CURRENT_BINARY_DIR}/libc.obj
    COMMAND ${CMAKE_COMMAND} -E rename
            ${CMAKE_CURRENT_BINARY_DIR}/libc.obj
            ${CMAKE_CURRENT_BINARY_DIR}/libc.bin
    DEPENDS ${LIBC_SOURCES} ${LIBC_C_MADLEN}
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
    char_tests.cpp
    peephole_tests.cpp
    printf_tests.cpp
    funcptr_tests.cpp
    chapter1_tests.cpp
    chapter2_tests.cpp
    chapter3_tests.cpp
    chapter4_tests.cpp
    chapter5_tests.cpp
    chapter6_tests.cpp
    chapter7_tests.cpp
    chapter8_tests.cpp
    chapter9_tests.cpp
    chapter10_tests.cpp
    chapter11_tests.cpp
    chapter12_tests.cpp
    chapter13_tests.cpp
    chapter14_tests.cpp
    chapter15_tests.cpp
    chapter16_tests.cpp
    chapter17_tests.cpp
    chapter18_tests.cpp
)
target_link_libraries(besm-tests besm parser translator semantic GTest::gtest_main)
add_dependencies(besm-tests besm-libc)
add_definitions(-DTEST_DIR="${CMAKE_CURRENT_BINARY_DIR}")
gtest_discover_tests(besm-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)
