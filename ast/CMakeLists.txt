#
# AST library
#
add_library(ast STATIC
    ast_alloc.c
    ast_print.c
    ast_free.c
    ast_clone.c
    ast_yaml.c
    ast_graphviz.c
    ast_export.c
    ast_import.c
    ast_compare.c
)
target_include_directories(ast PUBLIC .)
target_link_libraries(ast libutil)
