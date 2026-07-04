#
# BESM-6 C runtime library: assemble the runtime helpers into libc.bin for the
# Dubna simulator.
#
# This is the target runtime, decoupled from the codegen backend (backend/besm6):
# it never links against the `besm` library, it only *runs the finished compiler*
# (parse -> lower -> genbesm -> besmc) over its own C and Madlen sources.
#
# Two kinds of sources:
#   * hand-written Madlen helpers (LIBC_MADLEN) — calling convention, arithmetic,
#     comparisons, byte access, b/tout, exit;
#   * higher-level routines written in C (LIBC_C), compiled by our own toolchain
#     (parse -> lower -> genbesm) into Madlen and assembled alongside the helpers.
#
set(LIBC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/madlen)
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
# Portable routines shared by every BESM-6 assembler backend, compiled from this
# directory (libc/besm6).  These have no Dubna dependency — they build on the
# putbyte/flush/getch seam or are pure computation — so libc/besm6/unix and
# libc/besm6/bemsh will reuse them once each supplies its own leaves below.
set(LIBC_C_PORTABLE
    printf puts putchar putch
    doprnt sprintf snprintf modf malloc
    fabs fmax fmin fma
    memcpy memset memcmp memmove memchr
    strlen strcpy strncpy strcat strncat
    strcmp strncmp strchr strrchr strstr
    strtok strerror
    atoi)

# Dubna monitor glue: the byte-emission/reading leaves, compiled from madlen/.
# A different assembler target reimplements just these three (plus the .madlen
# helpers) against its own OS.
set(LIBC_C_DUBNA
    putbyte flush getch)

# Compile one NAME.c (from ${srcdir}) through cpp -> parse -> lower -> genbesm into
# NAME.madlen in the build dir, and record it in LIBC_C_MADLEN.  A macro (not a
# function) so the list append lands in this directory's scope.
set(LIBC_C_MADLEN "")
macro(besm6_compile_libc_c name srcdir)
    set(c_src  ${srcdir}/${name}.c)
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
        COMMENT "Compiling ${name}.c -> ${name}.madlen")
    list(APPEND LIBC_C_MADLEN ${c_mad})
endmacro()

foreach(name ${LIBC_C_PORTABLE})
    besm6_compile_libc_c(${name} ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()
foreach(name ${LIBC_C_DUBNA})
    besm6_compile_libc_c(${name} ${LIBC_DIR})
endforeach()

add_custom_command(
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/libc.bin
    COMMAND besmc -c ${LIBC_SOURCES} ${LIBC_C_MADLEN} -o ${CMAKE_CURRENT_BINARY_DIR}/libc.obj
    COMMAND ${CMAKE_COMMAND} -E rename
            ${CMAKE_CURRENT_BINARY_DIR}/libc.obj
            ${CMAKE_CURRENT_BINARY_DIR}/libc.bin
    # Stage a copy next to besm-tests: that binary chdir()s into build/backend/besm6
    # at startup and the Dubna job links libc.bin from the current directory.  Done
    # here (in the command, not a POST_BUILD) so it refreshes whenever libc.bin is
    # regenerated.
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_BINARY_DIR}/libc.bin
            ${CMAKE_BINARY_DIR}/backend/besm6/libc.bin
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
# Standard-library header check: every header in include/ must preprocess (via the
# system cpp, located in the root CMakeLists) and parse.
#
add_test(
    NAME besm-headers
    COMMAND ${CMAKE_COMMAND} -E env
            ${CMAKE_SOURCE_DIR}/scripts/check_headers.sh
            ${SystemCpp}
            $<TARGET_FILE:parse>
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_BINARY_DIR}/header-check)
