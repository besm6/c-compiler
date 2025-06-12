#include <inttypes.h>

#include "string_map.h"
#include "typetab.h"

extern StringMap typetab;

//
// Print struct/union.
//
void print_struct(StructDef *def)
{
    printf("    struct %s: size %d bytes, alignment %d\n", def->tag, def->size, def->alignment);
    for (FieldDef *field = def->members; field; field = field->next) {
        printf("        field %s: offset %d\n", field->name, field->offset);
        print_type(stdout, field->type, 12);
    }
}

static void typetab_print_callback(intptr_t ptr, void *arg)
{
    print_struct((StructDef *)ptr);
}

//
// Print all structs.
//
void typetab_print()
{
    map_iterate(&typetab, typetab_print_callback, NULL);
}
