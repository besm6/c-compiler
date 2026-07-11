#
# Bemsh (Cyrillic autocode) BESM-6 C runtime library: assemble the hand-written runtime
# helpers into libbem.bin with besmc (Assembler БЕМШ) for the Dubna simulator.
#
# This mirrors ../CMakeLists.txt (the Madlen/libc.bin build) but targets the Bemsh
# dialect.  Task B4 is helpers-only: the ~41 hand-written Madlen helpers (LIBC_MADLEN,
# inherited from the parent scope) are hand-translated into Bemsh autocode here, with the
# same stems (b_NAME.bemsh).  The C-level libc (LIBC_C_PORTABLE and the three Dubna C
# leaves putbyte/flush/getch) is deliberately excluded: the C->Bemsh compile path needs
# the B1 emitter wired through genbesm plus B2 naming, which land later.
#
# Each .bemsh file is a self-contained Macro-Bemsh deck: the module (СТАРТ ... ФИНИШ) is
# wrapped in the ВВД$$$ ... КНЦ$$$ input/translate markers that the Dubna macro-assembler
# requires (besmc feeds the source verbatim after *bemsh and does not add them).  besmc
# assembles all decks into one relocatable object library, exactly like the Madlen
# libc.bin command in ../CMakeLists.txt.
#

set(BEMSH_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# The inherited LIBC_MADLEN helper stems, resolved to this directory's .bemsh sources.
list(TRANSFORM LIBC_MADLEN PREPEND ${BEMSH_DIR}/ OUTPUT_VARIABLE BEMSH_SOURCES)
list(TRANSFORM BEMSH_SOURCES APPEND .bemsh)

add_custom_command(
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/libbem.bin
    COMMAND besmc -c ${BEMSH_SOURCES} -o ${CMAKE_CURRENT_BINARY_DIR}/libbem.obj
    COMMAND ${CMAKE_COMMAND} -E rename
            ${CMAKE_CURRENT_BINARY_DIR}/libbem.obj
            ${CMAKE_CURRENT_BINARY_DIR}/libbem.bin
    # Stage a copy next to besm-tests (mirrors the Madlen libc.bin copy) for the future
    # Bemsh run harness (task B3), which chdir()s into build/backend/besm6.  A distinct
    # name (libbem.bin) keeps it separate from the Madlen libc.bin staged there.
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_BINARY_DIR}/libbem.bin
            ${CMAKE_BINARY_DIR}/backend/besm6/libbem.bin
    DEPENDS ${BEMSH_SOURCES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Assembling BESM-6 Bemsh C runtime library (libbem.bin)")

# Built by the default `make`/`make all` (ALL) alongside the Madlen libc.bin and the Unix
# libc.a.
add_custom_target(besm-libc-bemsh ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libbem.bin)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libbem.bin DESTINATION share/besm6/lib)
