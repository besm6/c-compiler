#include "symtab.h"

#include <inttypes.h>

#include "string_map.h"

extern StringMap symtab;

//
// Print a list of static initializers.
//
void print_static_initializer(FILE *fd, StaticInitializer *item)
{
    for (; item; item = item->next) {
        fprintf(fd, "        Init: ");
        switch (item->kind) {
        case INIT_CHAR:
            fprintf(fd, "i8 %d\n", item->u.char_val);
            break;
        case INIT_INT:
            fprintf(fd, "i32 %d\n", item->u.int_val);
            break;
        case INIT_LONG:
            fprintf(fd, "i64 %" PRId64 "\n", item->u.long_val);
            break;
        case INIT_UCHAR:
            fprintf(fd, "u8 %u\n", item->u.uchar_val);
            break;
        case INIT_UINT:
            fprintf(fd, "u32 %u\n", item->u.uint_val);
            break;
        case INIT_ULONG:
            fprintf(fd, "u64 %" PRIu64 "\n", item->u.ulong_val);
            break;
        case INIT_DOUBLE:
            fprintf(fd, "f64 %a\n", item->u.double_val);
            break;
        case INIT_STRING:
            fprintf(fd, "string \"%s", item->u.string_val.str);
            if (item->u.string_val.null_terminated)
                fprintf(fd, "\\0");
            fprintf(fd, "\n");
            break;
        case INIT_ZERO:
            fprintf(fd, "zero %d bytes\n", item->u.zero_bytes);
            break;
        case INIT_POINTER:
            fprintf(fd, "pointer %s\n", item->u.ptr_id);
            break;
        }
    }
}

//
// Print symbol.
//
void print_symbol(Symbol *sym)
{
    printf("    %s:", sym->name);
    switch (sym->kind) {
    case SYM_FUNC:
        printf(" function");
        if (sym->u.func.global)
            printf(" global");
        if (sym->u.func.defined)
            printf(" defined");
        printf("\n");
        break;
    case SYM_STATIC:
        printf(" static_var");
        if (sym->u.static_var.global)
            printf(" global");
        switch (sym->u.static_var.init_kind) {
        case INIT_NONE:
            printf("\n");
            break;
        case INIT_TENTATIVE:
            printf(" tentative\n");
            break;
        case INIT_INITIALIZED:
            printf(" initialized\n");
            print_static_initializer(stdout, sym->u.static_var.init_list);
            break;
        }
        break;
    case SYM_CONST:
        printf(" string\n");
        print_static_initializer(stdout, sym->u.const_init);
        break;
    case SYM_LOCAL:
        printf(" local\n");
        break;
    }
    print_type(stdout, sym->type, 8);
}

static void symtab_print_callback(intptr_t ptr, void *arg)
{
    print_symbol((Symbol *)ptr);
}

//
// Print all symbols.
//
void symtab_print()
{
    map_iterate(&symtab, symtab_print_callback, NULL);
}
