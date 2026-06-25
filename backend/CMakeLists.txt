add_subdirectory(besm6)

#
# `genbesm` executable
#
add_executable(genbesm
    main.c
)
target_link_libraries(genbesm tac besm)

install(PROGRAMS $<TARGET_FILE:genbesm> DESTINATION bin RENAME b6codegen)
