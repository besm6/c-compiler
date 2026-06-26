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
    b_ret b_save0 b_save b_tout exit
    b_udiv b_umod b_ult b_ule b_ugt b_uge b_dtoi b_dtou b_utod b_uneg
    b_lsh b_rsh b_uadd b_usub b_umul
    b_flt b_fle b_fgt b_fge b_stb b_padd b_pinc b_pdec b_pdiff
    ldexp frexp)
list(TRANSFORM LIBC_MADLEN PREPEND ${LIBC_DIR}/ OUTPUT_VARIABLE LIBC_SOURCES)
list(TRANSFORM LIBC_SOURCES APPEND .madlen)

# The system cpp (SystemCpp) and the BESM-6 include dir (BESM6_INCLUDE_DIR) are
# located in the root CMakeLists.txt so every test target can reach them; the libc
# C sources #include the project's own standard headers and so must be run through
# a C preprocessor before our front end.

# Standard headers consumed by the libc sources; an edit to any of them rebuilds.
file(GLOB BESM6_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

# C runtime routines compiled by our own pipeline.  Each NAME.c becomes NAME.madlen
# in the build directory via cpp -> parse -> lower -> genbesm.  besmc only recognizes
# the .madlen extension, so genbesm writes there.
set(LIBC_C
    putbyte putch flush printf getch puts putchar
    doprnt sprintf snprintf modf malloc
    fabs fmax fmin fma
    memcpy memset memcmp memmove memchr
    strlen strcpy strncpy strcat strncat
    strcmp strncmp strchr strrchr strstr
    strtok strerror
    atoi)
set(LIBC_C_MADLEN "")
foreach(name ${LIBC_C})
    set(c_src  ${LIBC_DIR}/${name}.c)
    set(c_pre  ${CMAKE_CURRENT_BINARY_DIR}/${name}.i)
    set(c_ast  ${CMAKE_CURRENT_BINARY_DIR}/${name}.ast)
    set(c_tac  ${CMAKE_CURRENT_BINARY_DIR}/${name}.tac)
    set(c_mad  ${CMAKE_CURRENT_BINARY_DIR}/${name}.madlen)
    add_custom_command(
        OUTPUT  ${c_mad}
        COMMAND ${SystemCpp} -E -nostdinc -I${CMAKE_CURRENT_SOURCE_DIR}/include
                ${c_src} -o ${c_pre}
        COMMAND parse   ${c_pre} ${c_ast}
        COMMAND lower   ${c_ast} ${c_tac}
        COMMAND genbesm ${c_tac} ${c_mad}
        DEPENDS ${c_src} ${BESM6_HEADERS} parse lower genbesm
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

# Built by the default `make`/`make all` (ALL) so the runtime library is always
# available, independent of the EXCLUDE_FROM_ALL test executables.
add_custom_target(besm-libc ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libc.bin)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libc.bin DESTINATION share/besm6/lib)

# Target C11 standard headers, installed alongside the runtime library.
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION share/besm6/include
        FILES_MATCHING PATTERN "*.h")

#
# Tests for the BESM-6 backend
#
add_executable(besm-tests EXCLUDE_FROM_ALL
    test/label_tests.cpp
    test/frame_tests.cpp
    test/codegen_tests.cpp
    test/arith_tests.cpp
    test/copy_tests.cpp
    test/flow_tests.cpp
    test/run_tests.cpp
    test/unary_tests.cpp
    test/convert_tests.cpp
    test/init_tests.cpp
    test/ptr_tests.cpp
    test/struct_tests.cpp
    test/char_tests.cpp
    test/peephole_tests.cpp
    test/printf_tests.cpp
    test/funcptr_tests.cpp
    test/stdarg_tests.cpp
    test/mem_tests.cpp
    test/str_tests.cpp
    test/typedef_tests.cpp
    test/math_tests.cpp
    test/chapter1_tests.cpp
    test/chapter2_tests.cpp
    test/chapter3_tests.cpp
    test/chapter4_tests.cpp
    test/chapter5_tests.cpp
    test/chapter6_tests.cpp
    test/chapter7_tests.cpp
    test/chapter8_tests.cpp
    test/chapter9_tests.cpp
    test/chapter10_tests.cpp
    test/chapter11_tests.cpp
    test/chapter12_tests.cpp
    test/chapter13_tests.cpp
    test/chapter14_tests.cpp
    test/chapter15_tests.cpp
    test/chapter16_tests.cpp
    test/chapter17_tests.cpp
    test/chapter18_tests1.cpp
    test/chapter18_tests2.cpp
    test/chapter18_tests3.cpp
    test/chapter18_tests4.cpp
    test/chapter19_tests.cpp
    test/chapter20_tests.cpp
)
target_link_libraries(besm-tests besm parser translator semantic GTest::gtest_main test_util)
add_dependencies(besm-tests besm-libc)
add_definitions(-DTEST_DIR="${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(besm-tests PRIVATE
    BESM6_CPP="${SystemCpp}"
    BESM6_INCLUDE_DIR="${BESM6_INCLUDE_DIR}")
test_chdir_to_bindir(besm-tests)
gtest_discover_tests(besm-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)

#
# Standard-library header check: every header in include/ must preprocess (via the
# system cpp, located above) and parse.
#
add_test(
    NAME besm-headers
    COMMAND ${CMAKE_COMMAND} -E env
            ${CMAKE_SOURCE_DIR}/scripts/check_headers.sh
            ${SystemCpp}
            $<TARGET_FILE:parse>
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_BINARY_DIR}/header-check)
