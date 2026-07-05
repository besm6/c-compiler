#
# Unix (b6as) BESM-6 C runtime library: assemble the reusable portable routines and the
# hand-written runtime helpers into libc.a with the v7besm toolchain (b6as, b6ar).
#
# This mirrors ../CMakeLists.txt (the Madlen/libc.bin build) but targets the Unix
# assembler.  The reusable portable C sources (LIBC_C_PORTABLE, inherited from the parent
# scope) are compiled through cpp -> parse -> lower -> genbesm --unix -> .s and assembled
# with b6as.  The hand-written Madlen helpers (LIBC_MADLEN) are replaced here by
# hand-written b6as .s files with the same stems; the three Dubna leaves
# (putbyte/flush/getch) are stubbed C in this directory.  The tools are invoked by bare
# name on PATH, exactly like dubna/besmc in the Madlen path.
#
# NB: b6ranlib is intentionally NOT run.  It corrupts any archive of >= 6 members (b6ld
# then rejects it with "bad magic") — see backend/besm6/TODO.md, task "Fix b6ranlib".
# b6ld reads each member's own symbol table, so an un-indexed archive still links.
#

set(UNIX_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(PORTABLE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

# The three Dubna monitor leaves, reimplemented for the Unix target (stubs for now).
set(UNIX_LEAVES putbyte flush getch)

# Shipped standard headers the portable C sources #include; an edit to any of them rebuilds.
file(GLOB BESM6_HEADERS ${PORTABLE_DIR}/include/*.h)

# Compile one NAME.c (portable from ${PORTABLE_DIR}, or a leaf from here) through
# cpp -> parse -> lower -> genbesm --unix -> b6as into NAME.o, and record it in UNIX_OBJS.
set(UNIX_OBJS "")
macro(besm6_unix_compile_c name srcdir)
    set(c_src ${srcdir}/${name}.c)
    set(c_pre ${CMAKE_CURRENT_BINARY_DIR}/${name}.i)
    set(c_ast ${CMAKE_CURRENT_BINARY_DIR}/${name}.ast)
    set(c_tac ${CMAKE_CURRENT_BINARY_DIR}/${name}.tac)
    set(c_asm ${CMAKE_CURRENT_BINARY_DIR}/${name}.s)
    set(c_obj ${CMAKE_CURRENT_BINARY_DIR}/${name}.o)
    add_custom_command(
        OUTPUT  ${c_obj}
        COMMAND ${SystemCpp} -E -nostdinc -I${PORTABLE_DIR}/include ${c_src} -o ${c_pre}
        COMMAND parse   ${c_pre} ${c_ast}
        COMMAND lower   ${c_ast} ${c_tac}
        COMMAND genbesm --unix ${c_tac} ${c_asm}
        COMMAND b6as -o ${c_obj} ${c_asm}
        DEPENDS ${c_src} ${BESM6_HEADERS} parse lower genbesm
        COMMENT "Compiling ${name}.c -> ${name}.o (Unix)")
    list(APPEND UNIX_OBJS ${c_obj})
endmacro()

# Assemble one hand-written NAME.s runtime helper into NAME.o.
macro(besm6_unix_assemble name)
    set(s_src ${UNIX_DIR}/${name}.s)
    set(s_obj ${CMAKE_CURRENT_BINARY_DIR}/${name}.o)
    add_custom_command(
        OUTPUT  ${s_obj}
        COMMAND b6as -o ${s_obj} ${s_src}
        DEPENDS ${s_src}
        COMMENT "Assembling ${name}.s -> ${name}.o (Unix)")
    list(APPEND UNIX_OBJS ${s_obj})
endmacro()

foreach(name ${LIBC_C_PORTABLE})
    besm6_unix_compile_c(${name} ${PORTABLE_DIR})
endforeach()
foreach(name ${UNIX_LEAVES})
    besm6_unix_compile_c(${name} ${UNIX_DIR})
endforeach()
foreach(name ${LIBC_MADLEN})
    besm6_unix_assemble(${name})
endforeach()

add_custom_command(
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/libc.a
    # Rebuild from scratch so a replaced member never leaves a stale copy behind.
    COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_CURRENT_BINARY_DIR}/libc.a
    COMMAND b6ar rc ${CMAKE_CURRENT_BINARY_DIR}/libc.a ${UNIX_OBJS}
    # Stage a copy next to besm-tests (mirrors the Madlen libc.bin copy) for U3.
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_BINARY_DIR}/libc.a
            ${CMAKE_BINARY_DIR}/backend/besm6/libc.a
    DEPENDS ${UNIX_OBJS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Archiving BESM-6 Unix C runtime library (libc.a)")

# Built by the default `make`/`make all` (ALL) alongside the Madlen libc.bin.
add_custom_target(besm-libc-unix ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libc.a)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libc.a DESTINATION share/besm6/lib)
