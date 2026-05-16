//
// Type-checking for initializers.
//
#include <stdio.h>
#include <string.h>

#include "semantic.h"
#include "structtab.h"
#include "symtab.h"
#include "typecheck.h"
#include "xalloc.h"

// Create a zero initializer for a type.
static Initializer *make_zero_init(Type *t)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    if (t->kind == TYPE_ARRAY) {
        Initializer *init = new_initializer(INITIALIZER_COMPOUND);
        init->type        = clone_type(t, __func__, __FILE__, __LINE__);

        InitItem **tail = &init->u.items;
        size_t size     = get_array_size(t);
        for (size_t i = 0; i < size; i++) {
            InitItem *item = new_init_item(NULL, make_zero_init(t->u.array.element));
            *tail          = item;
            tail           = &item->next;
        }
        return init;
    }
    if (t->kind == TYPE_STRUCT) {
        Initializer *init = new_initializer(INITIALIZER_COMPOUND);
        init->type        = clone_type(t, __func__, __FILE__, __LINE__);

        InitItem **tail   = &init->u.items;
        FieldDef *members = structtab_find(t->u.struct_t.name)->members;
        for (; members; members = members->next) {
            InitItem *item = new_init_item(NULL, make_zero_init(members->type));
            *tail          = item;
            tail           = &item->next;
        }
        return init;
    }

    Initializer *init  = new_initializer(INITIALIZER_SINGLE);
    init->type         = clone_type(t, __func__, __FILE__, __LINE__);
    init->u.expr       = new_expression(EXPR_LITERAL);
    init->u.expr->type = clone_type(t, __func__, __FILE__, __LINE__);
    switch (t->kind) {
    case TYPE_CHAR:
        init->u.expr->u.literal = new_literal(LITERAL_CHAR);
        break;
    case TYPE_INT:
        init->u.expr->u.literal = new_literal(LITERAL_INT);
        break;
    case TYPE_LONG:
        init->u.expr->u.literal = new_literal(LITERAL_INT); // Simplified
        break;
    case TYPE_DOUBLE:
        init->u.expr->u.literal = new_literal(LITERAL_FLOAT);
        break;
    case TYPE_POINTER:
        init->u.expr->u.literal = new_literal(LITERAL_INT); // Null pointer
        break;
    default:
        fatal_error("Unsupported type for zero init: %d", t->kind);
    }
    return init;
}

// Count top-level elements in a brace initializer list.
static size_t count_init_items(const InitItem *items)
{
    size_t n = 0;
    for (; items; items = items->next) {
        n++;
    }
    return n;
}

// Convert an initializer to a Tac_StaticInit list for global/static variables.
Tac_StaticInit *build_static_init(Type *var_type, const Initializer *init)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }

    // Handle null initializer: initialize with zeros.
    if (!init) {
        Tac_StaticInit *zero_init = tac_new_static_init(TAC_STATIC_INIT_ZERO);
        zero_init->u.zero_bytes   = get_size(var_type);
        return zero_init;
    }

    // Handle array initialized with a string literal.
    if (var_type->kind == TYPE_ARRAY && init->kind == INITIALIZER_SINGLE &&
        init->u.expr->kind == EXPR_LITERAL && init->u.expr->u.literal->kind == LITERAL_STRING) {
        const Type *element_type = var_type->u.array.element;
        if (element_type->kind != TYPE_CHAR && element_type->kind != TYPE_SCHAR &&
            element_type->kind != TYPE_UCHAR) {
            fatal_error("String literal can only initialize character array");
        }
        const char *string_val = init->u.expr->u.literal->u.string_val;
        size_t string_length   = strlen(string_val);
        size_t array_size;
        if (!var_type->u.array.size) {
            // Array size is not specified - use string size.
            array_size = string_length + 1;
            set_array_size(var_type, array_size);
        } else {
            array_size = get_array_size(var_type);
            if (string_length > array_size) {
                fatal_error("String literal too long for array");
            }
        }

        Tac_StaticInit *string_init           = tac_new_static_init(TAC_STATIC_INIT_STRING);
        string_init->u.string.val             = xstrdup(string_val);
        string_init->u.string.null_terminated = (array_size >= string_length + 1);
        if (array_size > string_length + 1) {
            Tac_StaticInit *zero_padding = tac_new_static_init(TAC_STATIC_INIT_ZERO);
            zero_padding->u.zero_bytes =
                (array_size - (string_length + 1)) * get_size(element_type);
            string_init->next = zero_padding;
        }
        return string_init;
    }

    // Handle pointer initialized with a string literal.
    if (var_type->kind == TYPE_POINTER && init->kind == INITIALIZER_SINGLE &&
        init->u.expr->kind == EXPR_LITERAL && init->u.expr->u.literal->kind == LITERAL_STRING) {
        if (var_type->u.pointer.target->kind != TYPE_CHAR) {
            fatal_error("String literal can only initialize pointer to char");
        }
        const char *string_val       = init->u.expr->u.literal->u.string_val;
        char *string_id              = symtab_add_string(string_val);
        Tac_StaticInit *pointer_init = tac_new_static_init(TAC_STATIC_INIT_POINTER);
        pointer_init->u.pointer_name = string_id;
        return pointer_init;
    }

    // Handle pointer initialized with array name.
    if (var_type->kind == TYPE_POINTER && init->kind == INITIALIZER_SINGLE &&
        init->u.expr->kind == EXPR_VAR) {
        const Symbol *sym = symtab_get(init->u.expr->u.var);
        if (sym->type->kind != TYPE_ARRAY) {
            fatal_error("Pointer can be only initialized by array");
        }
        if (!compare_type(var_type->u.pointer.target, sym->type->u.array.element)) {
            fatal_error("Initialization of pointer with incompatible array");
        }
        Tac_StaticInit *pointer_init = tac_new_static_init(TAC_STATIC_INIT_POINTER);
        pointer_init->u.pointer_name = xstrdup(init->u.expr->u.var);
        return pointer_init;
    }

    // Handle scalar initialized with a literal.
    if (init->kind == INITIALIZER_SINGLE && init->u.expr->kind == EXPR_LITERAL) {
        const Literal *literal = init->u.expr->u.literal;
        if (is_zero_int(literal)) {
            Tac_StaticInit *zero_init = tac_new_static_init(TAC_STATIC_INIT_ZERO);
            zero_init->u.zero_bytes   = get_size(var_type);
            return zero_init;
        }
        if (var_type->kind != TYPE_CHAR && var_type->kind != TYPE_INT &&
            var_type->kind != TYPE_LONG && var_type->kind != TYPE_DOUBLE) {
            fatal_error("Static initializer requires arithmetic type");
        }
        return new_static_init_from_literal(var_type, literal);
    }

    // Handle array with compound initializer.
    if (var_type->kind == TYPE_ARRAY && init->kind == INITIALIZER_COMPOUND) {
        bool size_specified = var_type->u.array.size != NULL;
        size_t array_size;
        if (!size_specified) {
            array_size = count_init_items(init->u.items);
            set_array_size(var_type, array_size);
        } else {
            array_size = get_array_size(var_type);
        }
        Type *element_type         = var_type->u.array.element;
        Tac_StaticInit *array_init = NULL;
        Tac_StaticInit **current   = &array_init;
        int element_count          = 0;

        for (const InitItem *item = init->u.items; item; item = item->next) {
            if (size_specified && element_count >= (int)array_size) {
                fatal_error("Too many elements in array initializer");
            }
            Tac_StaticInit *element_init = build_static_init(element_type, item->init);
            *current                     = element_init;
            while (*current) {
                current = &(*current)->next;
            }
            element_count++;
        }

        if (element_count < (int)array_size) {
            Tac_StaticInit *zero_padding = tac_new_static_init(TAC_STATIC_INIT_ZERO);
            zero_padding->u.zero_bytes = ((int)array_size - element_count) * get_size(element_type);
            *current                   = zero_padding;
        }
        return array_init;
    }

    // Handle struct with compound initializer.
    if (var_type->kind == TYPE_STRUCT && init->kind == INITIALIZER_COMPOUND) {
        const StructDef *struct_def = structtab_find(var_type->u.struct_t.name);
        const FieldDef *field       = struct_def->members;
        Tac_StaticInit *struct_init = NULL;
        Tac_StaticInit **current    = &struct_init;
        int current_offset          = 0;

        for (const InitItem *item = init->u.items; item; item = item->next) {
            if (!field) {
                fatal_error("Too many elements in struct initializer");
            }
            if (current_offset < field->offset) {
                Tac_StaticInit *zero_padding = tac_new_static_init(TAC_STATIC_INIT_ZERO);
                zero_padding->u.zero_bytes   = field->offset - current_offset;
                *current                     = zero_padding;
                current                      = &zero_padding->next;
            }
            Tac_StaticInit *field_init = build_static_init(field->type, item->init);
            *current                   = field_init;
            while (*current) {
                current = &(*current)->next;
            }
            current_offset = field->offset + get_size(field->type);
            field          = field->next;
        }

        if (current_offset < struct_def->size) {
            Tac_StaticInit *zero_padding = tac_new_static_init(TAC_STATIC_INIT_ZERO);
            zero_padding->u.zero_bytes   = struct_def->size - current_offset;
            *current                     = zero_padding;
        }
        return struct_init;
    }

    // Handle invalid cases.
    if (var_type->kind == TYPE_ARRAY && init->kind == INITIALIZER_SINGLE) {
        fatal_error("Cannot initialize array with scalar value");
    }
    fatal_error("Unsupported initializer for type %s", type_kind_str[var_type->kind]);
}

// Type-check an initializer against a target type.
Initializer *typecheck_init(Type *target_type, Initializer *init)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }

    // Handle null initializer.
    if (!init) {
        return NULL;
    }

    // Update initializer type.
    free_type(init->type);
    init->type = clone_type(target_type, __func__, __FILE__, __LINE__);

    // Handle array initialized with a string literal.
    if (target_type->kind == TYPE_ARRAY && init->kind == INITIALIZER_SINGLE &&
        init->u.expr->kind == EXPR_LITERAL && init->u.expr->u.literal->kind == LITERAL_STRING) {
        const Type *element_type = target_type->u.array.element;
        if (element_type->kind != TYPE_CHAR && element_type->kind != TYPE_SCHAR &&
            element_type->kind != TYPE_UCHAR) {
            fatal_error("String literal can only initialize character array");
        }
        const char *string_val = init->u.expr->u.literal->u.string_val;
        size_t string_length   = strlen(string_val);
        if (!target_type->u.array.size) {
            set_array_size(target_type, string_length + 1);
        } else {
            size_t array_size = get_array_size(target_type);
            if (string_length > array_size) {
                fatal_error("String literal too long for array");
            }
        }
        init->u.expr = typecheck_string(init->u.expr);
        return init;
    }

    // Handle scalar initialized with a single expression.
    if (init->kind == INITIALIZER_SINGLE) {
        Expr *expression = typecheck_and_decay(init->u.expr);
        expression       = coerce_for_assignment(expression, target_type);
        init->u.expr     = expression;
        return init;
    }

    // Handle array with compound initializer.
    if (target_type->kind == TYPE_ARRAY && init->kind == INITIALIZER_COMPOUND) {
        bool size_specified = target_type->u.array.size != NULL;
        size_t array_size;
        if (!size_specified) {
            array_size = count_init_items(init->u.items);
            set_array_size(target_type, array_size);
        } else {
            array_size = get_array_size(target_type);
        }
        Type *element_type  = target_type->u.array.element;
        InitItem *new_items = NULL;
        InitItem **current  = &new_items;
        int element_count   = 0;

        for (const InitItem *item = init->u.items; item; item = item->next) {
            if (size_specified && element_count >= (int)array_size) {
                fatal_error("Too many elements in array initializer");
            }
            InitItem *new_item = new_init_item(NULL, typecheck_init(element_type, item->init));
            *current           = new_item;
            current            = &new_item->next;
            element_count++;
        }

        for (int i = element_count; i < (int)array_size; i++) {
            InitItem *zero_item = new_init_item(NULL, make_zero_init(element_type));
            *current            = zero_item;
            current             = &zero_item->next;
        }

        // Free old InitItem shells only — sub-inits are now owned by new_items.
        for (InitItem *item = init->u.items, *nx; item; item = nx) {
            nx = item->next;
            free_designator(item->designators);
            xfree(item);
        }
        init->u.items = new_items;
        return init;
    }

    // Handle struct with compound initializer.
    if (target_type->kind == TYPE_STRUCT && init->kind == INITIALIZER_COMPOUND) {
        const StructDef *struct_def = structtab_find(target_type->u.struct_t.name);
        const FieldDef *field       = struct_def->members;
        InitItem *new_items         = NULL;
        InitItem **current          = &new_items;

        for (const InitItem *item = init->u.items; item; item = item->next) {
            if (!field) {
                fatal_error("Too many elements in struct initializer");
            }
            InitItem *new_item = new_init_item(NULL, typecheck_init(field->type, item->init));
            *current           = new_item;
            current            = &new_item->next;
            field              = field->next;
        }

        for (; field; field = field->next) {
            InitItem *zero_item = new_init_item(NULL, make_zero_init(field->type));
            *current            = zero_item;
            current             = &zero_item->next;
        }

        // Free old InitItem shells only — sub-inits are now owned by new_items.
        for (InitItem *item = init->u.items, *nx; item; item = nx) {
            nx = item->next;
            free_designator(item->designators);
            xfree(item);
        }
        init->u.items = new_items;
        return init;
    }

    fatal_error("Cannot initialize scalar type with compound initializer");
}
