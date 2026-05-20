add_subdirectory(besm6)

#
# `genbesm` executable
#
add_executable(genbesm
    main.c
)
target_link_libraries(genbesm tac besm)
