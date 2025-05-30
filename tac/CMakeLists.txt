#
# TAC library
#
add_library(tac STATIC
    tac_alloc.c
    tac_print.c
    tac_free.c
    tac_compare.c
# TODO:
#   tac_yaml.c
#   tac_graphviz.c
#   tac_export.c
#   tac_import.c
)
target_include_directories(tac PUBLIC .)
target_link_libraries(tac libutil)
