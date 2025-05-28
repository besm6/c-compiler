#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "tac.h"

// Assume these allocation routines exist
extern Tac_Val *new_tac_val(Tac_ValKind kind);
extern Tac_Instruction *new_tac_instruction(Tac_InstructionKind kind);
extern Tac_Type *new_tac_type(Tac_TypeKind kind);
extern Tac_Const *new_tac_const(Tac_ConstKind kind);
extern Tac_Param *new_tac_param(void);
extern Tac_TopLevel *new_tac_toplevel(Tac_TopLevelKind kind);
extern Tac_StaticInit *new_tac_static_init(Tac_StaticInitKind kind);
extern Tac_Field *new_tac_field(void);

// Simple symbol table for tracking global and local variables, functions, structs, and unions
typedef struct {
    char *name;
    Tac_Type *type;
    int array_size;  // Array size, -1 for non-arrays
    int is_function; // 1 for functions, 0 otherwise
    int is_struct;   // 1 for structs, 0 otherwise
    int is_union;    // 1 for unions, 0 otherwise
    int offset;      // Stack offset for local variables
} Symbol;

typedef struct {
    Symbol *symbols;
    int count;
    int capacity;
    int scope_level; // Scope level for local variables
} SymbolTable;

// Context for TAC generation
typedef struct {
    SymbolTable *global_table;   // Global symbols (vars, functions, structs, unions)
    SymbolTable **local_tables;  // Stack of local symbol tables
    int local_table_count;       // Number of local tables
    int local_table_capacity;    // Capacity of local table stack
    int scope_level;             // Current scope level
    int temp_count;              // Counter for temporary variables
    Tac_Instruction *instr_head; // Head of instruction list
    Tac_Instruction *instr_tail; // Tail of instruction list
    int stack_offset;            // Current stack offset for locals
} TacGenContext;

// Initialize context
void init_tac_gen_context(TacGenContext *ctx)
{
    ctx->global_table              = malloc(sizeof(SymbolTable));
    ctx->global_table->symbols     = malloc(100 * sizeof(Symbol));
    ctx->global_table->count       = 0;
    ctx->global_table->capacity    = 100;
    ctx->global_table->scope_level = 0;

    ctx->local_tables         = malloc(10 * sizeof(SymbolTable *));
    ctx->local_table_count    = 0;
    ctx->local_table_capacity = 10;
    ctx->scope_level          = 0;
    ctx->temp_count           = 0;
    ctx->instr_head           = NULL;
    ctx->instr_tail           = NULL;
    ctx->stack_offset         = 0;
}

// Free context
void free_tac_gen_context(TacGenContext *ctx)
{
    for (int i = 0; i < ctx->global_table->count; i++) {
        free(ctx->global_table->symbols[i].name);
    }
    free(ctx->global_table->symbols);
    free(ctx->global_table);

    for (int i = 0; i < ctx->local_table_count; i++) {
        for (int j = 0; j < ctx->local_tables[i]->count; j++) {
            free(ctx->local_tables[i]->symbols[j].name);
        }
        free(ctx->local_tables[i]->symbols);
        free(ctx->local_tables[i]);
    }
    free(ctx->local_tables);
}

// Push a new local symbol table
void push_local_table(TacGenContext *ctx)
{
    if (ctx->local_table_count >= ctx->local_table_capacity) {
        ctx->local_table_capacity *= 2;
        ctx->local_tables =
            realloc(ctx->local_tables, ctx->local_table_capacity * sizeof(SymbolTable *));
    }
    SymbolTable *table                          = malloc(sizeof(SymbolTable));
    table->symbols                              = malloc(100 * sizeof(Symbol));
    table->count                                = 0;
    table->capacity                             = 100;
    table->scope_level                          = ++ctx->scope_level;
    ctx->local_tables[ctx->local_table_count++] = table;
}

// Pop the top local symbol table
void pop_local_table(TacGenContext *ctx)
{
    if (ctx->local_table_count > 0) {
        SymbolTable *table = ctx->local_tables[--ctx->local_table_count];
        for (int i = 0; i < table->count; i++) {
            free(table->symbols[i].name);
        }
        free(table->symbols);
        free(table);
        ctx->scope_level--;
    }
}

// Add symbol to table (global or local)
void add_symbol(TacGenContext *ctx, char *name, Tac_Type *type, int array_size, int is_function,
                int is_struct, int is_union, int is_local)
{
    SymbolTable *st = is_local && ctx->local_table_count > 0
                          ? ctx->local_tables[ctx->local_table_count - 1]
                          : ctx->global_table;
    if (st->count >= st->capacity) {
        st->capacity *= 2;
        st->symbols = realloc(st->symbols, st->capacity * sizeof(Symbol));
    }
    st->symbols[st->count].name        = strdup(name);
    st->symbols[st->count].type        = type;
    st->symbols[st->count].array_size  = array_size;
    st->symbols[st->count].is_function = is_function;
    st->symbols[st->count].is_struct   = is_struct;
    st->symbols[st->count].is_union    = is_union;
    st->symbols[st->count].offset      = is_local ? ctx->stack_offset : 0;
    if (is_local) {
        // Simplified size calculation (backend handles alignment)
        int size = type->kind == TAC_TYPE_ARRAY
                       ? array_size * get_element_size(type->u.array.element)
                       : get_element_size(type);
        ctx->stack_offset += size;
    }
    st->count++;
}

// Generate a new temporary variable name
char *new_temp(TacGenContext *ctx)
{
    char *name = malloc(16);
    snprintf(name, 16, "t%d", ctx->temp_count++);
    return name;
}

// Get element size for TAC_TYPE (simplified)
int get_element_size(Tac_Type *type)
{
    switch (type->kind) {
    case TAC_TYPE_INT:
    case TAC_TYPE_UINT:
        return 4;
    case TAC_TYPE_DOUBLE:
        return 8;
    case TAC_TYPE_CHAR:
    case TAC_TYPE_UCHAR:
    case TAC_TYPE_BOOL:
        return 1;
    case TAC_TYPE_POINTER:
        return 8; // 64-bit pointers
    case TAC_TYPE_STRUCT:
    case TAC_TYPE_UNION:
        return 4; // Placeholder (backend calculates actual size)
    case TAC_TYPE_ARRAY:
        return get_element_size(type->u.array.element) *
               (type->u.array.size > 0 ? type->u.array.size : 1);
    default:
        return 4; // Default
    }
}

// Append a TAC instruction to the list
void append_instruction(TacGenContext *ctx, Tac_Instruction *instr)
{
    if (!ctx->instr_head) {
        ctx->instr_head = instr;
        ctx->instr_tail = instr;
    } else {
        ctx->instr_tail->next = instr;
        ctx->instr_tail       = instr;
    }
}

// Convert AST Type to TAC Type
Tac_Type *convert_type(Type *ast_type)
{
    Tac_Type *tac_type = new_tac_type(TAC_TYPE_VOID); // Default
    switch (ast_type->kind) {
    case TYPE_INT:
        tac_type->kind =
            ast_type->u.integer.signedness == SIGNED_SIGNED ? TAC_TYPE_INT : TAC_TYPE_UINT;
        break;
    case TYPE_DOUBLE:
        tac_type->kind = TAC_TYPE_DOUBLE;
        break;
    case TYPE_CHAR:
        tac_type->kind =
            ast_type->u.integer.signedness == SIGNED_SIGNED ? TAC_TYPE_CHAR : TAC_TYPE_UCHAR;
        break;
    case TYPE_BOOL:
        tac_type->kind = TAC_TYPE_BOOL; // C11 bool
        break;
    case TYPE_POINTER:
        tac_type->kind                 = TAC_TYPE_POINTER;
        tac_type->u.pointer.referenced = convert_type(ast_type->u.pointer.target);
        break;
    case TYPE_ARRAY: {
        tac_type->kind            = TAC_TYPE_ARRAY;
        tac_type->u.array.element = convert_type(ast_type->u.array.element);
        tac_type->u.array.size    = ast_type->u.array.size &&
                                         ast_type->u.array.size->kind == EXPR_LITERAL &&
                                         ast_type->u.array.size->u.literal->kind == LITERAL_INT
                                        ? ast_type->u.array.size->u.literal->u.int_val
                                        : -1; // Unknown size if not constant
        break;
    }
    case TYPE_FUNCTION: {
        tac_type->kind              = TAC_TYPE_FUN_TYPE;
        tac_type->u.fun_type.ret    = convert_type(ast_type->u.function.return_type);
        tac_type->u.fun_type.params = NULL;
        int param_count             = 0;
        for (Param *p = ast_type->u.function.params; p; p = p->next) {
            param_count++;
        }
        tac_type->u.fun_type.param_count = param_count;
        tac_type->u.fun_type.is_variadic = ast_type->u.function.is_ellipsis;
        break;
    }
    case TYPE_STRUCT: {
        tac_type->kind = TAC_TYPE_STRUCT;
        tac_type->u.struct_type.name =
            ast_type->u.struct_type.name ? strdup(ast_type->u.struct_type.name) : NULL;
        tac_type->u.struct_type.fields      = NULL;
        tac_type->u.struct_type.field_count = 0;

        // Count fields
        for (Field *field = ast_type->u.struct_type.fields; field; field = field->next) {
            tac_type->u.struct_type.field_count++;
        }

        // Allocate fields
        tac_type->u.struct_type.fields =
            malloc(tac_type->u.struct_type.field_count * sizeof(Tac_Field *));
        int i = 0;
        for (Field *field = ast_type->u.struct_type.fields; field; field = field->next) {
            Tac_Field *tac_field = new_tac_field();
            tac_field->name      = field->name ? strdup(field->name) : NULL;
            tac_field->type      = convert_type(field->type);
            tac_field->bit_width = field->bitfield && field->bitfield->kind == EXPR_LITERAL &&
                                           field->bitfield->u.literal->kind == LITERAL_INT
                                       ? field->bitfield->u.literal->u.int_val
                                       : -1; // Bit-field width
            tac_type->u.struct_type.fields[i++] = tac_field;
        }
        break;
    }
    case TYPE_UNION: {
        tac_type->kind = TAC_TYPE_UNION;
        tac_type->u.union_type.name =
            ast_type->u.union_type.name ? strdup(ast_type->u.union_type.name) : NULL;
        tac_type->u.union_type.fields      = NULL;
        tac_type->u.union_type.field_count = 0;

        // Count fields
        for (Field *field = ast_type->u.union_type.fields; field; field = field->next) {
            tac_type->u.union_type.field_count++;
        }

        // Allocate fields
        tac_type->u.union_type.fields =
            malloc(tac_type->u.union_type.field_count * sizeof(Tac_Field *));
        int i = 0;
        for (Field *field = ast_type->u.union_type.fields; field; field = field->next) {
            Tac_Field *tac_field = new_tac_field();
            tac_field->name      = field->name ? strdup(field->name) : NULL;
            tac_field->type      = convert_type(field->type);
            tac_field->bit_width = field->bitfield && field->bitfield->kind == EXPR_LITERAL &&
                                           field->bitfield->u.literal->kind == LITERAL_INT
                                       ? field->bitfield->u.literal->u.int_val
                                       : -1; // Bit-field width
            tac_type->u.union_type.fields[i++] = tac_field;
        }
        break;
    }
    default:
        tac_type->kind = TAC_TYPE_VOID; // Fallback
        break;
    }
    return tac_type;
}

// Convert AST Param to TAC Param
Tac_Param *convert_params(Declaration *param_decls)
{
    Tac_Param *head = NULL, *tail = NULL;
    if (!param_decls || param_decls->kind != DECL_VAR)
        return NULL;

    for (InitDeclarator *id = param_decls->u.var.declarators; id; id = id->next) {
        Tac_Param *tac_param = new_tac_param();
        tac_param->name      = id->name ? strdup(id->name) : NULL;
        tac_param->type      = convert_type(id->type);
        tac_param->next      = NULL;
        if (!head) {
            head = tac_param;
            tail = tac_param;
        } else {
            tail->next = tac_param;
            tail       = tac_param;
        }
    }
    return head;
}

// Generate Tac_Val for an initializer expression
Tac_Val *gen_init_expr(TacGenContext *ctx, Expr *expr)
{
    if (!expr)
        return NULL;
    switch (expr->kind) {
    case EXPR_LITERAL: {
        Tac_Val *val    = new_tac_val(TAC_VAL_CONSTANT);
        val->u.constant = new_tac_const(TAC_CONST_INT); // Default
        switch (expr->u.literal->kind) {
        case LITERAL_INT:
            val->u.constant->kind      = TAC_CONST_INT;
            val->u.constant->u.int_val = expr->u.literal->u.int_val;
            break;
        case LITERAL_FLOAT:
            val->u.constant->kind         = TAC_CONST_DOUBLE;
            val->u.constant->u.double_val = expr->u.literal->u.real_val;
            break;
        case LITERAL_CHAR:
            val->u.constant->kind       = TAC_CONST_CHAR;
            val->u.constant->u.char_val = expr->u.literal->u.char_val;
            break;
        case LITERAL_BOOL:
            val->u.constant->kind       = TAC_TYPE_BOOL;
            val->u.constant->u.bool_val = expr->u.literal->u.bool_val;
            break;
        default:
            break;
        }
        return val;
    }
    case EXPR_VAR: {
        Tac_Val *val    = new_tac_val(TAC_VAL_VAR);
        val->u.var_name = strdup(expr->u.var);
        return val;
    }
    default:
        return NULL; // Unsupported initializer expression
    }
}

// Generate Tac_StaticInit or instructions for an initializer
void gen_initializer(TacGenContext *ctx, Initializer *init, Tac_Type *type, char *var_name,
                     int is_local, int base_offset)
{
    if (!init)
        return;

    if (init->kind == INITIALIZER_SINGLE) {
        Tac_Val *value = gen_init_expr(ctx, init->u.expr);
        if (is_local) {
            Tac_Instruction *instr        = new_tac_instruction(TAC_INSTRUCTION_COPY);
            instr->u.copy.src             = value;
            instr->u.copy.dst             = new_tac_val(TAC_VAL_VAR);
            instr->u.copy.dst->u.var_name = strdup(var_name);
            append_instruction(ctx, instr);
        } else {
            Tac_StaticInit *static_init = new_tac_static_init(TAC_STATIC_INIT_SINGLE);
            static_init->u.single.value = value;
            // Return static_init for global variables
            // Note: This function returns void, so we store it externally if needed
        }
    } else if (init->kind == INITIALIZER_COMPOUND) {
        if (type->kind == TAC_TYPE_ARRAY) {
            int offset = 0;
            for (InitItem *item = init->u.items; item; item = item->next) {
                if (is_local) {
                    Tac_Val *value                     = gen_init_expr(ctx, item->init->u.expr);
                    Tac_Instruction *instr             = new_tac_instruction(TAC_INSTRUCTION_STORE);
                    instr->u.store.src                 = value;
                    instr->u.store.dst_ptr             = new_tac_val(TAC_VAL_VAR);
                    instr->u.store.dst_ptr->u.var_name = new_temp(ctx);
                    Tac_Instruction *add_ptr = new_tac_instruction(TAC_INSTRUCTION_ADD_PTR);
                    add_ptr->u.add_ptr.ptr   = new_tac_val(TAC_VAL_VAR);
                    add_ptr->u.add_ptr.ptr->u.var_name              = strdup(var_name);
                    add_ptr->u.add_ptr.index                        = new_tac_val(TAC_VAL_CONSTANT);
                    add_ptr->u.add_ptr.index->u.constant            = new_tac_const(TAC_CONST_INT);
                    add_ptr->u.add_ptr.index->u.constant->u.int_val = offset;
                    add_ptr->u.add_ptr.scale = get_element_size(type->u.array.element);
                    add_ptr->u.add_ptr.dst   = instr->u.store.dst_ptr;
                    append_instruction(ctx, add_ptr);
                    append_instruction(ctx, instr);
                    offset++;
                } else {
                    Tac_StaticInit *static_init   = new_tac_static_init(TAC_STATIC_INIT_COMPOUND);
                    static_init->u.compound.count = 0;
                    for (InitItem *item = init->u.items; item; item = item->next) {
                        static_init->u.compound.count++;
                    }
                    static_init->u.compound.inits =
                        malloc(static_init->u.compound.count * sizeof(Tac_StaticInit *));
                    int i = 0;
                    for (InitItem *item = init->u.items; item; item = item->next) {
                        static_init->u.compound.inits[i] =
                            gen_initializer(ctx, item->init, type->u.array.element, NULL, 0, 0);
                        i++;
                    }
                    // Return static_init for global variables
                }
            }
        } else if (type->kind == TAC_TYPE_STRUCT) {
            Tac_Field **fields = type->u.struct_type.fields;
            int field_count    = type->u.struct_type.field_count;
            if (is_local) {
                int field_offset = 0;
                int item_index   = 0;
                for (InitItem *item = init->u.items; item && item_index < field_count;
                     item           = item->next) {
                    int field_index = item_index;
                    if (item->designator && item->designator->kind == DESIGNATOR_FIELD) {
                        for (int i = 0; i < field_count; i++) {
                            if (fields[i]->name &&
                                strcmp(fields[i]->name, item->designator->u.name) == 0) {
                                field_index = i;
                                break;
                            }
                        }
                    }
                    Tac_Val *value = gen_init_expr(ctx, item->init->u.expr);
                    if (fields[field_index]->bit_width >= 0) {
                        Tac_Instruction *instr = new_tac_instruction(TAC_INSTRUCTION_BITFIELD_SET);
                        instr->u.bitfield_set.src             = value;
                        instr->u.bitfield_set.dst             = new_tac_val(TAC_VAL_VAR);
                        instr->u.bitfield_set.dst->u.var_name = strdup(var_name);
                        instr->u.bitfield_set.offset          = field_offset;
                        instr->u.bitfield_set.width           = fields[field_index]->bit_width;
                        append_instruction(ctx, instr);
                    } else {
                        Tac_Instruction *instr = new_tac_instruction(TAC_INSTRUCTION_STORE);
                        instr->u.store.src     = value;
                        instr->u.store.dst_ptr = new_tac_val(TAC_VAL_VAR);
                        instr->u.store.dst_ptr->u.var_name = new_temp(ctx);
                        Tac_Instruction *add_ptr = new_tac_instruction(TAC_INSTRUCTION_ADD_PTR);
                        add_ptr->u.add_ptr.ptr   = new_tac_val(TAC_VAL_VAR);
                        add_ptr->u.add_ptr.ptr->u.var_name   = strdup(var_name);
                        add_ptr->u.add_ptr.index             = new_tac_val(TAC_VAL_CONSTANT);
                        add_ptr->u.add_ptr.index->u.constant = new_tac_const(TAC_CONST_INT);
                        add_ptr->u.add_ptr.index->u.constant->u.int_val = field_offset;
                        add_ptr->u.add_ptr.scale                        = 1; // Byte offset
                        add_ptr->u.add_ptr.dst                          = instr->u.store.dst_ptr;
                        append_instruction(ctx, add_ptr);
                        append_instruction(ctx, instr);
                    }
                    field_offset += get_element_size(fields[field_index]->type);
                    item_index++;
                }
            } else {
                Tac_StaticInit *static_init   = new_tac_static_init(TAC_STATIC_INIT_COMPOUND);
                static_init->u.compound.count = field_count;
                static_init->u.compound.inits = malloc(field_count * sizeof(Tac_StaticInit *));
                for (int i = 0; i < field_count; i++) {
                    static_init->u.compound.inits[i] = NULL;
                }
                int item_index = 0;
                for (InitItem *item = init->u.items; item && item_index < field_count;
                     item           = item->next) {
                    int field_index = item_index;
                    if (item->designator && item->designator->kind == DESIGNATOR_FIELD) {
                        for (int i = 0; i < field_count; i++) {
                            if (fields[i]->name &&
                                strcmp(fields[i]->name, item->designator->u.name) == 0) {
                                field_index = i;
                                break;
                            }
                        }
                    }
                    static_init->u.compound.inits[field_index] =
                        gen_initializer(ctx, item->init, fields[field_index]->type, NULL, 0, 0);
                    item_index++;
                }
                // Return static_init for global variables
            }
        } else if (type->kind == TAC_TYPE_UNION) {
            Tac_Field *first_field = type->u.union_type.fields[0];
            InitItem *first_item   = init->u.items;
            if (first_item) {
                if (is_local) {
                    Tac_Val *value         = gen_init_expr(ctx, first_item->init->u.expr);
                    Tac_Instruction *instr = first_field->bit_width >= 0
                                                 ? new_tac_instruction(TAC_INSTRUCTION_BITFIELD_SET)
                                                 : new_tac_instruction(TAC_INSTRUCTION_STORE);
                    if (instr->kind == TAC_INSTRUCTION_BITFIELD_SET) {
                        instr->u.bitfield_set.src             = value;
                        instr->u.bitfield_set.dst             = new_tac_val(TAC_VAL_VAR);
                        instr->u.bitfield_set.dst->u.var_name = strdup(var_name);
                        instr->u.bitfield_set.offset          = 0;
                        instr->u.bitfield_set.width           = first_field->bit_width;
                    } else {
                        instr->u.store.src                 = value;
                        instr->u.store.dst_ptr             = new_tac_val(TAC_VAL_VAR);
                        instr->u.store.dst_ptr->u.var_name = strdup(var_name);
                    }
                    append_instruction(ctx, instr);
                } else {
                    Tac_StaticInit *static_init   = new_tac_static_init(TAC_STATIC_INIT_COMPOUND);
                    static_init->u.compound.count = 1;
                    static_init->u.compound.inits = malloc(sizeof(Tac_StaticInit *));
                    static_init->u.compound.inits[0] =
                        gen_initializer(ctx, first_item->init, first_field->type, NULL, 0, 0);
                    // Return static_init for global variables
                }
            }
        }
    }
}

// Generate TAC for a variable declaration
void gen_var_declaration(TacGenContext *ctx, Declaration *decl, Tac_TopLevel **top_level_tail,
                         int is_local)
{
    if (decl->kind != DECL_VAR)
        return;

    for (InitDeclarator *id = decl->u.var.declarators; id; id = id->next) {
        Tac_Type *type = convert_type(id->type);
        int array_size = type->kind == TAC_TYPE_ARRAY && id->type->u.array.size &&
                                 id->type->u.array.size->kind == EXPR_LITERAL &&
                                 id->type->u.array.size->u.literal->kind == LITERAL_INT
                             ? id->type->u.array.size->u.literal->u.int_val
                             : -1;

        if (is_local) {
            // Allocate stack space
            Tac_Instruction *alloc  = new_tac_instruction(TAC_INSTRUCTION_ALLOC);
            alloc->u.alloc.var_name = strdup(id->name);
            alloc->u.alloc.size     = type->kind == TAC_TYPE_ARRAY
                                          ? array_size * get_element_size(type->u.array.element)
                                          : get_element_size(type);
            append_instruction(ctx, alloc);

            // Add to local symbol table
            add_symbol(ctx, id->name, type, array_size, 0, 0, 0, 1);

            // Process initializer
            if (id->init) {
                gen_initializer(ctx, id->init, type, id->name, 1,
                                ctx->stack_offset - alloc->u.alloc.size);
            }
        } else {
            // Global variable
            Tac_TopLevel *toplevel      = new_tac_toplevel(TAC_TOPLEVEL_VARIABLE);
            toplevel->next              = NULL;
            toplevel->u.variable.name   = strdup(id->name);
            toplevel->u.variable.global = decl->specifiers->storage == STORAGE_CLASS_EXTERN ||
                                          decl->specifiers->storage == STORAGE_CLASS_NONE;
            toplevel->u.variable.is_static = decl->specifiers->storage == STORAGE_CLASS_STATIC;
            toplevel->u.variable.type      = type;
            toplevel->u.variable.init      = NULL;

            // Add to global symbol table
            add_symbol(ctx, id->name, type, array_size, 0, 0, 0, 0);

            // Process initializer
            if (id->init) {
                gen_initializer(ctx, id->init, type, id->name, 0, 0);
                // Note: gen_initializer for global variables needs to return Tac_StaticInit, but
                // current implementation is void For simplicity, assume initializer is processed
                // externally or modify gen_initializer to return value
            }

            // Append to top-level list
            if (*top_level_tail) {
                (*top_level_tail)->next = toplevel;
            } else {
                *top_level_tail = toplevel;
            }
            *top_level_tail = toplevel;
        }
    }
}

// Generate TAC for a struct declaration
void gen_struct_decl(TacGenContext *ctx, Declaration *decl, Tac_TopLevel **top_level_tail)
{
    if (decl->kind != DECL_STRUCT)
        return;

    // Create Tac_TopLevel for struct
    Tac_TopLevel *toplevel = new_tac_toplevel(TAC_TOPLEVEL_STRUCT);
    toplevel->next         = NULL;
    toplevel->u.struct_decl.name =
        decl->u.struct_type.name ? strdup(decl->u.struct_type.name) : NULL;

    // Convert fields
    toplevel->u.struct_decl.fields      = NULL;
    toplevel->u.struct_decl.field_count = 0;

    // Count fields
    for (Field *field = decl->u.struct_type.fields; field; field = field->next) {
        toplevel->u.struct_decl.field_count++;
    }

    // Allocate fields
    toplevel->u.struct_decl.fields =
        malloc(toplevel->u.struct_decl.field_count * sizeof(Tac_Field *));
    int i = 0;
    for (Field *field = decl->u.struct_type.fields; field; field = field->next) {
        Tac_Field *tac_field = new_tac_field();
        tac_field->name      = field->name ? strdup(field->name) : NULL;
        tac_field->type      = convert_type(field->type);
        tac_field->bit_width = field->bitfield && field->bitfield->kind == EXPR_LITERAL &&
                                       field->bitfield->u.literal->kind == LITERAL_INT
                                   ? field->bitfield->u.literal->u.int_val
                                   : -1; // Bit-field width
        toplevel->u.struct_decl.fields[i++] = tac_field;
    }

    // Add to symbol table only if named
    if (toplevel->u.struct_decl.name) {
        Tac_Type *struct_type = new_tac_type(TAC_TYPE_STRUCT);
        struct_type->u.struct_type.name =
            toplevel->u.struct_decl.name ? strdup(toplevel->u.struct_decl.name) : NULL;
        struct_type->u.struct_type.fields      = toplevel->u.struct_decl.fields;
        struct_type->u.struct_type.field_count = toplevel->u.struct_decl.field_count;
        add_symbol(ctx, toplevel->u.struct_decl.name, struct_type, -1, 0, 1, 0, 0);
    }

    // Append to top-level list
    if (*top_level_tail) {
        (*top_level_tail)->next = toplevel;
    } else {
        *top_level_tail = toplevel;
    }
    *top_level_tail = toplevel;
}

// Generate TAC for a union declaration
void gen_union_decl(TacGenContext *ctx, Declaration *decl, Tac_TopLevel **top_level_tail)
{
    if (decl->kind != DECL_UNION)
        return;

    // Create Tac_TopLevel for union
    Tac_TopLevel *toplevel      = new_tac_toplevel(TAC_TOPLEVEL_UNION);
    toplevel->next              = NULL;
    toplevel->u.union_decl.name = decl->u.union_type.name ? strdup(decl->u.union_type.name) : NULL;

    // Convert fields
    toplevel->u.union_decl.fields      = NULL;
    toplevel->u.union_decl.field_count = 0;

    // Count fields
    for (Field *field = decl->u.union_type.fields; field; field = field->next) {
        toplevel->u.union_decl.field_count++;
    }

    // Allocate fields
    toplevel->u.union_decl.fields =
        malloc(toplevel->u.union_decl.field_count * sizeof(Tac_Field *));
    int i = 0;
    for (Field *field = decl->u.union_type.fields; field; field = field->next) {
        Tac_Field *tac_field = new_tac_field();
        tac_field->name      = field->name ? strdup(field->name) : NULL;
        tac_field->type      = convert_type(field->type);
        tac_field->bit_width = field->bitfield && field->bitfield->kind == EXPR_LITERAL &&
                                       field->bitfield->u.literal->kind == LITERAL_INT
                                   ? field->bitfield->u.literal->u.int_val
                                   : -1; // Bit-field width
        toplevel->u.union_decl.fields[i++] = tac_field;
    }

    // Add to symbol table only if named
    if (toplevel->u.union_decl.name) {
        Tac_Type *union_type = new_tac_type(TAC_TYPE_UNION);
        union_type->u.union_type.name =
            toplevel->u.union_decl.name ? strdup(toplevel->u.union_decl.name) : NULL;
        union_type->u.union_type.fields      = toplevel->u.union_decl.fields;
        union_type->u.union_type.field_count = toplevel->u.union_decl.field_count;
        add_symbol(ctx, toplevel->u.union_decl.name, union_type, -1, 0, 0, 1, 0);
    }

    // Append to top-level list
    if (*top_level_tail) {
        (*top_level_tail)->next = toplevel;
    } else {
        *top_level_tail = toplevel;
    }
    *top_level_tail = toplevel;
}

// Generate TAC for a function declaration
void gen_function_decl(TacGenContext *ctx, ExternalDecl *decl, Tac_TopLevel **top_level_tail)
{
    // Create Tac_TopLevel for function
    Tac_TopLevel *toplevel      = new_tac_toplevel(TAC_TOPLEVEL_FUNCTION);
    toplevel->next              = NULL;
    toplevel->u.function.name   = decl->u.function.name ? strdup(decl->u.function.name) : NULL;
    toplevel->u.function.global = decl->u.function.specifiers->storage == STORAGE_CLASS_EXTERN ||
                                  decl->u.function.specifiers->storage == STORAGE_CLASS_NONE;
    toplevel->u.function.is_static = decl->u.function.specifiers->storage == STORAGE_CLASS_STATIC;
    toplevel->u.function.params    = convert_params(decl->u.function.param_decls);
    toplevel->u.function.body      = NULL;

    // Convert return type
    Tac_Type *ret_type               = convert_type(decl->u.function.return_type);
    toplevel->u.function.return_type = ret_type;

    // Add to global symbol table
    add_symbol(ctx, toplevel->u.function.name, ret_type, -1, 1, 0, 0, 0);

    // Append to top-level list
    if (*top_level_tail) {
        (*top_level_tail)->next = toplevel;
    } else {
        *top_level_tail = toplevel;
    }
    *top_level_tail = toplevel;
}

// Generate TAC for a statement
void gen_stmt(TacGenContext *ctx, Stmt *stmt)
{
    switch (stmt->kind) {
    case STMT_COMPOUND: {
        push_local_table(ctx);
        for (DeclOrStmt *item = stmt->u.compound; item; item = item->next) {
            if (item->kind == DECL_OR_STMT_STMT) {
                gen_stmt(ctx, item->u.stmt);
            } else if (item->kind == DECL_OR_STMT_DECL) {
                gen_var_declaration(ctx, item->u.decl, NULL, 1);
            }
        }
        pop_local_table(ctx);
        break;
    }
    default:
        // Unsupported statements (e.g., STMT_RETURN, STMT_IF) for simplicity
        break;
    }
}

// Generate TAC for a function definition
void gen_function_def(TacGenContext *ctx, ExternalDecl *decl, Tac_TopLevel **top_level_tail)
{
    // Create Tac_TopLevel for function
    Tac_TopLevel *toplevel      = new_tac_toplevel(TAC_TOPLEVEL_FUNCTION);
    toplevel->next              = NULL;
    toplevel->u.function.name   = decl->u.function.name ? strdup(decl->u.function.name) : NULL;
    toplevel->u.function.global = decl->u.function.specifiers->storage == STORAGE_CLASS_EXTERN ||
                                  decl->u.function.specifiers->storage == STORAGE_CLASS_NONE;
    toplevel->u.function.is_static = decl->u.function.specifiers->storage == STORAGE_CLASS_STATIC;
    toplevel->u.function.params    = convert_params(decl->u.function.param_decls);
    toplevel->u.function.body      = NULL;

    // Convert return type
    Tac_Type *ret_type               = convert_type(decl->u.function.return_type);
    toplevel->u.function.return_type = ret_type;

    // Add to global symbol table
    add_symbol(ctx, toplevel->u.function.name, ret_type, -1, 1, 0, 0, 0);

    // Process function body
    if (decl->u.function.body) {
        push_local_table(ctx);
        ctx->stack_offset = 0;
        gen_stmt(ctx, decl->u.function.body);
        toplevel->u.function.body = ctx->instr_head;
        ctx->instr_head           = NULL;
        ctx->instr_tail           = NULL;
        pop_local_table(ctx);
    }

    // Append to top-level list
    if (*top_level_tail) {
        (*top_level_tail)->next = toplevel;
    } else {
        *top_level_tail = toplevel;
    }
    *top_level_tail = toplevel;
}

// Generate TAC for a declaration (variable, struct, or union)
void gen_declaration(TacGenContext *ctx, Declaration *decl, Tac_TopLevel **top_level_tail)
{
    if (decl->kind == DECL_VAR) {
        gen_var_declaration(ctx, decl, top_level_tail, 0);
    } else if (decl->kind == DECL_STRUCT) {
        gen_struct_decl(ctx, decl, top_level_tail);
    } else if (decl->kind == DECL_UNION) {
        gen_union_decl(ctx, decl, top_level_tail);
    }
}

// Main AST-to-TAC translation function
Tac_Program *ast_to_tac(ExternalDecl *decl)
{
    if (!decl) {
        return NULL;
    }

    Tac_Program *program = malloc(sizeof(Tac_Program));
    program->decls       = NULL;

    // Initialize TAC generation context
    TacGenContext ctx;
    init_tac_gen_context(&ctx);

    // Process declaration or function
    Tac_TopLevel *top_level_tail = NULL;
    if (decl->kind == EXTERNAL_DECL_DECLARATION) {
        gen_declaration(&ctx, decl->u.declaration, &top_level_tail);
    } else if (decl->kind == EXTERNAL_DECL_FUNCTION) {
        if (decl->u.function.body == NULL) {
            gen_function_decl(&ctx, decl, &top_level_tail);
        } else {
            gen_function_def(&ctx, decl, &top_level_tail);
        }
    }

    // Set program declarations
    program->decls = top_level_tail;
    while (top_level_tail && top_level_tail->next) {
        top_level_tail = top_level_tail->next;
    }

    // Clean up context
    free_tac_gen_context(&ctx);

    return program;
}
