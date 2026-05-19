#
# BESM-6 backend library
#
add_library(besm6 STATIC
    besm_alloc.c
    besm_free.c
)
target_include_directories(besm6 PUBLIC .)
target_link_libraries(besm6 libutil)
