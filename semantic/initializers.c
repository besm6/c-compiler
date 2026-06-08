//
// Type-checking for initializers.
//
#include <assert.h>
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
        init->u.expr->u.literal = new_literal(LITERAL_LONG);
        break;
    case TYPE_LONG_LONG:
        init->u.expr->u.literal = new_literal(LITERAL_LONG_LONG);
        break;
    case TYPE_ULONG:
        init->u.expr->u.literal = new_literal(LITERAL_ULONG);
        break;
    case TYPE_ULONG_LONG:
        init->u.expr->u.literal = new_literal(LITERAL_ULONG_LONG);
        break;
    case TYPE_FLOAT:
        init->u.expr->u.literal = new_literal(LITERAL_FLOAT);
        break;
    case TYPE_DOUBLE:
        init->u.expr->u.literal = new_literal(LITERAL_DOUBLE);
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
        pointer_init->u.pointer.name = string_id;
        return pointer_init;
    }

    // Handle pointer initialized with array or function name.
    if (var_type->kind == TYPE_POINTER && init->kind == INITIALIZER_SINGLE &&
        init->u.expr->kind == EXPR_VAR) {
        const Symbol *sym = symtab_get(init->u.expr->u.var);
        if (sym->type->kind == TYPE_ARRAY) {
            if (!compatible_type(var_type->u.pointer.target, sym->type->u.array.element)) {
                fatal_error("Initialization of pointer with incompatible array");
            }
        } else if (sym->type->kind == TYPE_FUNCTION) {
            if (!compatible_type(var_type->u.pointer.target, sym->type)) {
                fatal_error("Initialization of pointer with incompatible function");
            }
        } else {
            fatal_error("Pointer can only be initialized by array or function");
        }
        Tac_StaticInit *pointer_init = tac_new_static_init(TAC_STATIC_INIT_POINTER);
        pointer_init->u.pointer.name = xstrdup(init->u.expr->u.var);
        return pointer_init;
    }

    // Handle pointer initialized with address-of a variable (&var).
    if (var_type->kind == TYPE_POINTER && init->kind == INITIALIZER_SINGLE &&
        init->u.expr->kind == EXPR_UNARY_OP &&
        init->u.expr->u.unary_op.op == UNARY_ADDRESS &&
        init->u.expr->u.unary_op.expr->kind == EXPR_VAR) {
        const char *var_name = init->u.expr->u.unary_op.expr->u.var;
        const Type *ptr_target = var_type->u.pointer.target;
        bool is_fat = (ptr_target->kind == TYPE_CHAR  || ptr_target->kind == TYPE_SCHAR ||
                       ptr_target->kind == TYPE_UCHAR || ptr_target->kind == TYPE_VOID);
        if (is_fat) {
            const Symbol *sym = symtab_get(var_name);
            bool byte_sized   = (sym->type->kind == TYPE_CHAR  ||
                                 sym->type->kind == TYPE_SCHAR ||
                                 sym->type->kind == TYPE_UCHAR);
            Tac_StaticInit *fi       = tac_new_static_init(TAC_STATIC_INIT_FAT_POINTER);
            fi->u.pointer.name       = xstrdup(var_name);
            fi->u.pointer.byte_offset = byte_sized ? 5 : 0;
            return fi;
        }
        Tac_StaticInit *pointer_init = tac_new_static_init(TAC_STATIC_INIT_POINTER);
        pointer_init->u.pointer.name = xstrdup(var_name);
        return pointer_init;
    }

    // Handle pointer initialized with address-of an array element (&arr[N]).
    if (var_type->kind == TYPE_POINTER && init->kind == INITIALIZER_SINGLE &&
        init->u.expr->kind == EXPR_UNARY_OP &&
        init->u.expr->u.unary_op.op == UNARY_ADDRESS &&
        init->u.expr->u.unary_op.expr->kind == EXPR_SUBSCRIPT &&
        init->u.expr->u.unary_op.expr->u.subscript.left->kind == EXPR_VAR) {
        const Expr *subscript    = init->u.expr->u.unary_op.expr;
        const char *arr_name     = subscript->u.subscript.left->u.var;
        const Expr *index_expr   = subscript->u.subscript.right;
        long index;
        if (!try_eval_const_int(index_expr, &index))
            fatal_error("Array subscript in static initializer must be a compile-time constant");
        const Symbol *arr_sym = symtab_get(arr_name);
        if (arr_sym->type->kind != TYPE_ARRAY)
            fatal_error("Subscript of non-array type in static initializer");
        if (!compatible_type(var_type->u.pointer.target, arr_sym->type->u.array.element))
            fatal_error("Incompatible types in pointer initialization with array element");
        const Type *ptr_target = var_type->u.pointer.target;
        bool is_fat = (ptr_target->kind == TYPE_CHAR  || ptr_target->kind == TYPE_SCHAR ||
                       ptr_target->kind == TYPE_UCHAR || ptr_target->kind == TYPE_VOID);
        int byte_offset = (int)index * (int)get_size(arr_sym->type->u.array.element);
        if (is_fat) {
            Tac_StaticInit *fi        = tac_new_static_init(TAC_STATIC_INIT_FAT_POINTER);
            fi->u.pointer.name        = xstrdup(arr_name);
            fi->u.pointer.byte_offset = byte_offset;
            return fi;
        }
        Tac_StaticInit *pointer_init    = tac_new_static_init(TAC_STATIC_INIT_POINTER);
        pointer_init->u.pointer.name    = xstrdup(arr_name);
        pointer_init->u.pointer.byte_offset = byte_offset;
        return pointer_init;
    }

    // Handle pointer initialized with an integer constant expression (e.g. cast from integer).
    if (var_type->kind == TYPE_POINTER && init->kind == INITIALIZER_SINGLE) {
        long val;
        if (try_eval_const_int(init->u.expr, &val)) {
            if (val == 0) {
                Tac_StaticInit *zero_init = tac_new_static_init(TAC_STATIC_INIT_ZERO);
                zero_init->u.zero_bytes   = get_size(var_type);
                return zero_init;
            }
            Tac_StaticInit *ptr_init = tac_new_static_init(TAC_STATIC_INIT_I64);
            ptr_init->u.long_val     = val;
            return ptr_init;
        }
    }

    // Handle scalar initialized with a literal.
    if (init->kind == INITIALIZER_SINGLE && init->u.expr->kind == EXPR_LITERAL) {
        const Literal *literal = init->u.expr->u.literal;
        if (is_zero_int(literal)) {
            Tac_StaticInit *zero_init = tac_new_static_init(TAC_STATIC_INIT_ZERO);
            zero_init->u.zero_bytes   = get_size(var_type);
            return zero_init;
        }
        if (!is_arithmetic(var_type)) {
            fatal_error("Static initializer requires arithmetic type");
        }
        return new_static_init_from_literal(var_type, literal);
    }

    // Handle integer scalar initialized with a constant expression (e.g. -1, ~0, sizeof(T)).
    if (init->kind == INITIALIZER_SINGLE && is_integer(var_type)) {
        const Expr *expr = typecheck_and_decay(init->u.expr);
        long val;
        if (try_eval_const_int(expr, &val)) {
            Literal lit;
            if (get_size(var_type) == 8 && is_signed(var_type)) {
                lit = (Literal){ .kind = LITERAL_LONG_LONG, .u.long_long_val = (long long)val };
            } else if (get_size(var_type) == 8) {
                lit = (Literal){ .kind = LITERAL_ULONG_LONG, .u.ulong_long_val = (unsigned long long)(unsigned long)val };
            } else {
                lit = (Literal){ .kind = LITERAL_INT, .u.int_val = (int)val };
            }
            return new_static_init_from_literal(var_type, &lit);
        }
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
            assert(field);
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
            assert(field);
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
