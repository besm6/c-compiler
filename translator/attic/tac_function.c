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

// Simple symbol table for tracking variables
typedef struct {
    char *name;
    Tac_Type *type;
    int array_size; // For arrays, -1 for non-arrays
} Symbol;

typedef struct {
    Symbol *symbols;
    int count;
    int capacity;
} SymbolTable;

typedef struct {
    char *break_label;
    char *continue_label; // NULL for switch statements
} ControlContext;

typedef struct {
    ControlContext *contexts;
    int context_count;
    int context_capacity;
} ControlContextStack;

// Label map for goto labels
typedef struct {
    char *ast_label;
    char *tac_label;
} LabelEntry;

typedef struct {
    LabelEntry *entries;
    int count;
    int capacity;
} LabelMap;

// Context for TAC generation
typedef struct {
    Tac_Instruction *instr_head;        // Head of instruction list
    Tac_Instruction *instr_tail;        // Tail for appending
    SymbolTable *sym_table;             // Symbol table
    int temp_count;                     // Counter for temporary variables
    int label_count;                    // Counter for labels
    ControlContextStack *context_stack; // Stack for break/continue labels
    LabelMap *label_map;                // Map for goto labels
} TacGenContext;

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
        return 1;
    case TAC_TYPE_POINTER:
        return 8; // Assuming 64-bit pointers
    default:
        return 4; // Default (configurable)
    }
}

// Initialize context
void init_tac_gen_context(TacGenContext *ctx)
{
    ctx->instr_head                      = NULL;
    ctx->instr_tail                      = NULL;
    ctx->sym_table                       = malloc(sizeof(SymbolTable));
    ctx->sym_table->symbols              = malloc(100 * sizeof(Symbol)); // Arbitrary size
    ctx->sym_table->count                = 0;
    ctx->sym_table->capacity             = 100;
    ctx->temp_count                      = 0;
    ctx->label_count                     = 0;
    ctx->context_stack                   = malloc(sizeof(ControlContextStack));
    ctx->context_stack->contexts         = malloc(10 * sizeof(ControlContext));
    ctx->context_stack->context_count    = 0;
    ctx->context_stack->context_capacity = 10;
    ctx->label_map                       = malloc(sizeof(LabelMap));
    ctx->label_map->entries              = malloc(100 * sizeof(LabelEntry));
    ctx->label_map->count                = 0;
    ctx->label_map->capacity             = 100;
}

// Free context
void free_tac_gen_context(TacGenContext *ctx)
{
    for (int i = 0; i < ctx->sym_table->count; i++) {
        free(ctx->sym_table->symbols[i].name);
        // Assume Tac_Type is freed elsewhere
    }
    free(ctx->sym_table->symbols);
    free(ctx->sym_table);
    for (int i = 0; i < ctx->context_stack->context_count; i++) {
        free(ctx->context_stack->contexts[i].break_label);
        if (ctx->context_stack->contexts[i].continue_label) {
            free(ctx->context_stack->contexts[i].continue_label);
        }
    }
    free(ctx->context_stack->contexts);
    free(ctx->context_stack);
    for (int i = 0; i < ctx->label_map->count; i++) {
        free(ctx->label_map->entries[i].ast_label);
        free(ctx->label_map->entries[i].tac_label);
    }
    free(ctx->label_map->entries);
    free(ctx->label_map);
}

// Push control context
void push_control_context(TacGenContext *ctx, char *break_label, char *continue_label)
{
    ControlContextStack *cs = ctx->context_stack;
    if (cs->context_count >= cs->context_capacity) {
        cs->context_capacity *= 2;
        cs->contexts = realloc(cs->contexts, cs->context_capacity * sizeof(ControlContext));
    }
    cs->contexts[cs->context_count].break_label    = break_label;
    cs->contexts[cs->context_count].continue_label = continue_label;
    cs->context_count++;
}

// Pop control context
void pop_control_context(TacGenContext *ctx)
{
    ControlContextStack *cs = ctx->context_stack;
    if (cs->context_count > 0) {
        cs->context_count--;
    }
}

// Get current break label
char *get_break_label(TacGenContext *ctx)
{
    ControlContextStack *cs = ctx->context_stack;
    if (cs->context_count > 0) {
        return cs->contexts[cs->context_count - 1].break_label;
    }
    return NULL; // No control context
}

// Get current continue label
char *get_continue_label(TacGenContext *ctx)
{
    ControlContextStack *cs = ctx->context_stack;
    if (cs->context_count > 0) {
        return cs->contexts[cs->context_count - 1].continue_label;
    }
    return NULL; // No loop context
}

// Add label to map
void add_label(TacGenContext *ctx, char *ast_label, char *tac_label)
{
    LabelMap *lm = ctx->label_map;
    if (lm->count >= lm->capacity) {
        lm->capacity *= 2;
        lm->entries = realloc(lm->entries, lm->capacity * sizeof(LabelEntry));
    }
    lm->entries[lm->count].ast_label = strdup(ast_label);
    lm->entries[lm->count].tac_label = strdup(tac_label);
    lm->count++;
}

// Get TAC label for AST label
char *get_tac_label(TacGenContext *ctx, char *ast_label)
{
    LabelMap *lm = ctx->label_map;
    for (int i = 0; i < lm->count; i++) {
        if (strcmp(lm->entries[i].ast_label, ast_label) == 0) {
            return lm->entries[i].tac_label;
        }
    }
    return NULL; // Label not found
}

// Add symbol to table
void add_symbol(TacGenContext *ctx, char *name, Tac_Type *type, int array_size)
{
    SymbolTable *st = ctx->sym_table;
    if (st->count >= st->capacity) {
        st->capacity *= 2;
        st->symbols = realloc(st->symbols, st->capacity * sizeof(Symbol));
    }
    st->symbols[st->count].name       = strdup(name);
    st->symbols[st->count].type       = type;
    st->symbols[st->count].array_size = array_size;
    st->count++;
}

// Generate a new temporary variable name
char *new_temp(TacGenContext *ctx)
{
    char *name = malloc(16);
    snprintf(name, 16, "t%d", ctx->temp_count++);
    return name;
}

// Generate a new label name
char *new_label(TacGenContext *ctx)
{
    char *name = malloc(16);
    snprintf(name, 16, "L%d", ctx->label_count++);
    return name;
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
    case TYPE_POINTER:
        tac_type->kind                 = TAC_TYPE_POINTER;
        tac_type->u.pointer.referenced = convert_type(ast_type->u.pointer.target);
        break;
    case TYPE_FUNCTION: {
        tac_type->kind              = TAC_TYPE_FUN_TYPE;
        tac_type->u.fun_type.ret    = convert_type(ast_type->u.function.return_type);
        tac_type->u.fun_type.params = NULL;
        int param_count             = 0;
        for (Param *p = ast_type->u.function.params; p; p = p->next) {
            param_count++;
        }
        tac_type->u.fun_type.param_count = param_count;
        tac_type->u.fun_type.is_variadic =
            ast_type->u.function.is_ellipsis; // Assume is_ellipsis field
        break;
    }
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
    default:
        tac_type->kind = TAC_TYPE_VOID; // Fallback
        break;
    }
    return tac_type;
}

// Convert AST Param to TAC Param
Tac_Param *convert_params(Param *ast_params)
{
    Tac_Param *head = NULL, *tail = NULL;
    for (Param *p = ast_params; p; p = p->next) {
        Tac_Param *tac_param = new_tac_param();
        tac_param->name      = p->name ? strdup(p->name) : NULL;
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

// Generate TAC for an initializer
void gen_initializer(TacGenContext *ctx, Initializer *init, char *base_name, Tac_Type *base_type)
{
    if (!init)
        return;
    if (init->kind == INITIALIZER_SINGLE) {
        Tac_Val *dst           = new_tac_val(TAC_VAL_VAR);
        dst->u.var_name        = strdup(base_name);
        Tac_Val *src           = gen_expr(ctx, init->u.expr);
        Tac_Instruction *instr = new_tac_instruction(TAC_INSTRUCTION_COPY);
        instr->u.copy.src      = src;
        instr->u.copy.dst      = dst;
        append_instruction(ctx, instr);
    } else if (init->kind == INITIALIZER_COMPOUND && base_type->kind == TAC_TYPE_ARRAY) {
        int offset = 0;
        for (InitItem *item = init->u.items; item; item = item->next) {
            // Assume no designators for simplicity
            if (item->init->kind == INITIALIZER_SINGLE) {
                Tac_Val *src                = gen_expr(ctx, item->init->u.expr);
                Tac_Instruction *instr      = new_tac_instruction(TAC_INSTRUCTION_COPY_TO_OFFSET);
                instr->u.copy_to_offset.src = src;
                instr->u.copy_to_offset.dst = strdup(base_name);
                instr->u.copy_to_offset.offset =
                    offset * get_element_size(base_type->u.array.element);
                append_instruction(ctx, instr);
                offset++;
            }
            // Handle nested initializers or designators if needed
        }
    }
}

// Generate TAC for an expression
Tac_Val *gen_expr(TacGenContext *ctx, Expr *expr)
{
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
    case EXPR_BINARY_OP: {
        Tac_Val *left          = gen_expr(ctx, expr->u.binary_op.left);
        Tac_Val *right         = gen_expr(ctx, expr->u.binary_op.right);
        char *dst              = new_temp(ctx);
        Tac_Instruction *instr = new_tac_instruction(TAC_INSTRUCTION_BINARY);
        instr->u.binary.op =
            (Tac_BinaryOperator[]){ [BINARY_ADD]        = TAC_BINARY_ADD,
                                    [BINARY_SUB]        = TAC_BINARY_SUBTRACT,
                                    [BINARY_MUL]        = TAC_BINARY_MULTIPLY,
                                    [BINARY_DIV]        = TAC_BINARY_DIVIDE,
                                    [BINARY_MOD]        = TAC_BINARY_REMAINDER,
                                    [BINARY_EQ]         = TAC_BINARY_EQUAL,
                                    [BINARY_NE]         = TAC_BINARY_NOT_EQUAL,
                                    [BINARY_LT]         = TAC_BINARY_LESS_THAN,
                                    [BINARY_LE]         = TAC_BINARY_LESS_OR_EQUAL,
                                    [BINARY_GT]         = TAC_BINARY_GREATER_THAN,
                                    [BINARY_GE]         = TAC_BINARY_GREATER_OR_EQUAL,
                                    [BINARY_BIT_AND]    = TAC_BINARY_BITWISE_AND,
                                    [BINARY_BIT_XOR]    = TAC_BINARY_BITWISE_XOR,
                                    [BINARY_BIT_OR]     = TAC_BINARY_BITWISE_OR,
                                    [BINARY_LEFT_SHIFT] = TAC_BINARY_LEFT_SHIFT,
                                    [BINARY_RIGHT_SHIFT] =
                                        TAC_BINARY_RIGHT_SHIFT }[expr->u.binary_op.op];
        instr->u.binary.src1            = left;
        instr->u.binary.src2            = right;
        instr->u.binary.dst             = new_tac_val(TAC_VAL_VAR);
        instr->u.binary.dst->u.var_name = dst;
        append_instruction(ctx, instr);
        return instr->u.binary.dst;
    }
    case EXPR_ASSIGN: {
        Tac_Val *value  = gen_expr(ctx, expr->u.assign.value);
        Tac_Val *target = gen_expr(ctx, expr->u.assign.target);

        // Map AssignOp to Tac_BinaryOperator for compound assignments
        Tac_BinaryOperator op_map[] = { [ADD_ASSIGN]         = TAC_BINARY_ADD,
                                        [SUB_ASSIGN]         = TAC_BINARY_SUBTRACT,
                                        [MUL_ASSIGN]         = TAC_BINARY_MULTIPLY,
                                        [DIV_ASSIGN]         = TAC_BINARY_DIVIDE,
                                        [MOD_ASSIGN]         = TAC_BINARY_REMAINDER,
                                        [BIT_AND_ASSIGN]     = TAC_BINARY_BITWISE_AND,
                                        [BIT_OR_ASSIGN]      = TAC_BINARY_BITWISE_OR,
                                        [BIT_XOR_ASSIGN]     = TAC_BINARY_BITWISE_XOR,
                                        [LEFT_SHIFT_ASSIGN]  = TAC_BINARY_LEFT_SHIFT,
                                        [RIGHT_SHIFT_ASSIGN] = TAC_BINARY_RIGHT_SHIFT };

        // Check if target is an array access
        if (expr->u.assign.target->kind == EXPR_FIELD_ACCESS) {
            Expr *base = expr->u.assign.target->u.field_access.expr;
            Expr *index =
                expr->u.assign.target->u.field_access.field; // Assuming field is index expr
            Tac_Val *base_val        = gen_expr(ctx, base);
            Tac_Val *index_val       = gen_expr(ctx, index);
            char *addr               = new_temp(ctx);
            Tac_Instruction *add_ptr = new_tac_instruction(TAC_INSTRUCTION_ADD_PTR);
            add_ptr->u.add_ptr.ptr   = base_val;
            add_ptr->u.add_ptr.index = index_val;
            add_ptr->u.add_ptr.scale =
                get_element_size(convert_type(expr->u.assign.target->type->u.pointer.referenced));
            add_ptr->u.add_ptr.dst             = new_tac_val(TAC_VAL_VAR);
            add_ptr->u.add_ptr.dst->u.var_name = addr;
            append_instruction(ctx, add_ptr);

            Tac_Val *result;
            if (expr->u.assign.op == ASSIGN) {
                result = value;
            } else {
                // Load current array element value
                Tac_Val *current      = new_tac_val(TAC_VAL_VAR);
                current->u.var_name   = new_temp(ctx);
                Tac_Instruction *load = new_tac_instruction(TAC_INSTRUCTION_LOAD);
                load->u.load.src_ptr  = add_ptr->u.add_ptr.dst;
                load->u.load.dst      = current;
                append_instruction(ctx, load);

                // Perform compound operation
                char *temp                          = new_temp(ctx);
                Tac_Instruction *bin_instr          = new_tac_instruction(TAC_INSTRUCTION_BINARY);
                bin_instr->u.binary.op              = op_map[expr->u.assign.op];
                bin_instr->u.binary.src1            = current;
                bin_instr->u.binary.src2            = value;
                bin_instr->u.binary.dst             = new_tac_val(TAC_VAL_VAR);
                bin_instr->u.binary.dst->u.var_name = temp;
                append_instruction(ctx, bin_instr);
                result = bin_instr->u.binary.dst;
            }

            // Store result back to array
            Tac_Instruction *store = new_tac_instruction(TAC_INSTRUCTION_STORE);
            store->u.store.src     = result;
            store->u.store.dst_ptr = add_ptr->u.add_ptr.dst;
            append_instruction(ctx, store);
            return result;
        } else {
            Tac_Val *result;
            if (expr->u.assign.op == ASSIGN) {
                result = value;
            } else {
                // Perform compound operation
                char *temp                          = new_temp(ctx);
                Tac_Instruction *bin_instr          = new_tac_instruction(TAC_INSTRUCTION_BINARY);
                bin_instr->u.binary.op              = op_map[expr->u.assign.op];
                bin_instr->u.binary.src1            = target;
                bin_instr->u.binary.src2            = value;
                bin_instr->u.binary.dst             = new_tac_val(TAC_VAL_VAR);
                bin_instr->u.binary.dst->u.var_name = temp;
                append_instruction(ctx, bin_instr);
                result = bin_instr->u.binary.dst;
            }

            // Assign result to target
            Tac_Instruction *instr = new_tac_instruction(TAC_INSTRUCTION_COPY);
            instr->u.copy.src      = result;
            instr->u.copy.dst      = target;
            append_instruction(ctx, instr);
            return result;
        }
    }
    case EXPR_FIELD_ACCESS: { // Assuming used for array indexing
        Expr *base               = expr->u.field_access.expr;
        Expr *index              = expr->u.field_access.field; // Assuming field is index expr
        Tac_Val *base_val        = gen_expr(ctx, base);
        Tac_Val *index_val       = gen_expr(ctx, index);
        char *addr               = new_temp(ctx);
        Tac_Instruction *add_ptr = new_tac_instruction(TAC_INSTRUCTION_ADD_PTR);
        add_ptr->u.add_ptr.ptr   = base_val;
        add_ptr->u.add_ptr.index = index_val;
        add_ptr->u.add_ptr.scale = get_element_size(convert_type(expr->type->u.pointer.referenced));
        add_ptr->u.add_ptr.dst   = new_tac_val(TAC_VAL_VAR);
        add_ptr->u.add_ptr.dst->u.var_name = addr;
        append_instruction(ctx, add_ptr);
        Tac_Val *result       = new_tac_val(TAC_VAL_VAR);
        result->u.var_name    = new_temp(ctx);
        Tac_Instruction *load = new_tac_instruction(TAC_INSTRUCTION_LOAD);
        load->u.load.src_ptr  = add_ptr->u.add_ptr.dst;
        load->u.load.dst      = result;
        append_instruction(ctx, load);
        return result;
    }
    case EXPR_CALL: {
        // Evaluate function expression (usually EXPR_VAR or EXPR_FIELD_ACCESS for function
        // pointers)
        Tac_Val *func_val = gen_expr(ctx, expr->u.call.func);
        // Process arguments
        for (Arg *arg = expr->u.call.args; arg; arg = arg->next) {
            Tac_Val *arg_val             = gen_expr(ctx, arg->expr);
            Tac_Instruction *param_instr = new_tac_instruction(TAC_INSTRUCTION_PARAM);
            param_instr->u.param.src     = arg_val;
            append_instruction(ctx, param_instr);
        }
        // Generate call instruction
        Tac_Instruction *call_instr = new_tac_instruction(TAC_INSTRUCTION_CALL);
        call_instr->u.call.func     = func_val;
        Tac_Type *ret_type          = convert_type(expr->type); // Return type of the call
        // Check if function is variadic (no special handling needed in TAC, backend manages)
        if (expr->u.call.func->type->kind == TYPE_FUNCTION &&
            expr->u.call.func->type->u.function.is_ellipsis) {
            // Variadic call; same TAC structure, backend handles variable args
        }
        if (ret_type->kind != TAC_TYPE_VOID) {
            Tac_Val *result        = new_tac_val(TAC_VAL_VAR);
            result->u.var_name     = new_temp(ctx);
            call_instr->u.call.dst = result;
            append_instruction(ctx, call_instr);
            return result;
        } else {
            call_instr->u.call.dst = NULL;
            append_instruction(ctx, call_instr);
            return NULL; // Void function call
        }
    }
    default:
        return NULL; // Unsupported expression
    }
}

// Generate TAC for a statement
void gen_stmt(TacGenContext *ctx, Stmt *stmt)
{
    switch (stmt->kind) {
    case STMT_EXPR: {
        if (stmt->u.expr) {
            gen_expr(ctx, stmt->u.expr);
        }
        break;
    }
    case STMT_RETURN: {
        Tac_Instruction *instr = new_tac_instruction(TAC_INSTRUCTION_RETURN);
        if (stmt->u.expr) {
            instr->u.return_.src = gen_expr(ctx, stmt->u.expr);
        } else {
            instr->u.return_.src = NULL;
        }
        append_instruction(ctx, instr);
        break;
    }
    case STMT_IF: {
        char *else_label                    = new_label(ctx);
        char *end_label                     = new_label(ctx);
        Tac_Val *cond                       = gen_expr(ctx, stmt->u.if_stmt.condition);
        Tac_Instruction *cond_jump          = new_tac_instruction(TAC_INSTRUCTION_JUMP_IF_ZERO);
        cond_jump->u.jump_if_zero.condition = cond;
        cond_jump->u.jump_if_zero.target    = else_label;
        append_instruction(ctx, cond_jump);
        // Then branch
        gen_stmt(ctx, stmt->u.if_stmt.then_stmt);
        Tac_Instruction *goto_end = new_tac_instruction(TAC_INSTRUCTION_JUMP);
        goto_end->u.jump.target   = end_label;
        append_instruction(ctx, goto_end);
        // Else label
        Tac_Instruction *else_instr = new_tac_instruction(TAC_INSTRUCTION_LABEL);
        else_instr->u.label.name    = else_label;
        append_instruction(ctx, else_instr);
        // Else branch
        if (stmt->u.if_stmt.else_stmt) {
            gen_stmt(ctx, stmt->u.if_stmt.else_stmt);
        }
        // End label
        Tac_Instruction *end_instr = new_tac_instruction(TAC_INSTRUCTION_LABEL);
        end_instr->u.label.name    = end_label;
        append_instruction(ctx, end_instr);
        break;
    }
    case STMT_SWITCH: {
        char *end_label = new_label(ctx);
        push_control_context(ctx, end_label, NULL); // No continue label for switch
        // Evaluate controlling expression
        Tac_Val *switch_val = gen_expr(ctx, stmt->u.switch_stmt.expr);
        // Generate comparisons for cases
        DeclOrStmt *body    = stmt->u.switch_stmt.body->u.compound;
        char *default_label = NULL;
        for (DeclOrStmt *item = body; item; item = item->next) {
            if (item->kind == DECL_OR_STMT_STMT && item->u.stmt->kind == STMT_CASE) {
                Expr *case_expr               = item->u.stmt->u.case_stmt.expr;
                char *case_label              = new_label(ctx);
                Tac_Val *case_val             = gen_expr(ctx, case_expr); // Assume constant
                char *temp                    = new_temp(ctx);
                Tac_Instruction *cmp          = new_tac_instruction(TAC_INSTRUCTION_BINARY);
                cmp->u.binary.op              = TAC_BINARY_EQUAL;
                cmp->u.binary.src1            = switch_val;
                cmp->u.binary.src2            = case_val;
                cmp->u.binary.dst             = new_tac_val(TAC_VAL_VAR);
                cmp->u.binary.dst->u.var_name = temp;
                append_instruction(ctx, cmp);
                Tac_Instruction *jump = new_tac_instruction(TAC_INSTRUCTION_JUMP_IF_NOT_ZERO);
                jump->u.jump_if_not_zero.condition = cmp->u.binary.dst;
                jump->u.jump_if_not_zero.target    = case_label;
                append_instruction(ctx, jump);
                item->u.stmt->u.case_stmt.label = case_label; // Store label for later
            } else if (item->kind == DECL_OR_STMT_STMT && item->u.stmt->kind == STMT_DEFAULT) {
                default_label                       = new_label(ctx);
                item->u.stmt->u.default_stmt->label = default_label; // Store label
            }
        }
        // Jump to default or end if no match
        Tac_Instruction *jump = new_tac_instruction(TAC_INSTRUCTION_JUMP);
        jump->u.jump.target   = default_label ? default_label : end_label;
        append_instruction(ctx, jump);
        // Generate body
        for (DeclOrStmt *item = body; item; item = item->next) {
            if (item->kind == DECL_OR_STMT_STMT) {
                if (item->u.stmt->kind == STMT_CASE) {
                    Tac_Instruction *label_instr = new_tac_instruction(TAC_INSTRUCTION_LABEL);
                    label_instr->u.label.name    = item->u.stmt->u.case_stmt.label;
                    append_instruction(ctx, label_instr);
                    gen_stmt(ctx, item->u.stmt->u.case_stmt.stmt);
                } else if (item->u.stmt->kind == STMT_DEFAULT) {
                    Tac_Instruction *label_instr = new_tac_instruction(TAC_INSTRUCTION_LABEL);
                    label_instr->u.label.name    = item->u.stmt->u.default_stmt->label;
                    append_instruction(ctx, label_instr);
                    gen_stmt(ctx, item->u.stmt->u.default_stmt);
                } else {
                    gen_stmt(ctx, item->u.stmt);
                }
            } else if (item->kind == DECL_OR_STMT_DECL) {
                Declaration *decl = item->u.decl;
                if (decl->kind == DECL_VAR) {
                    for (InitDeclarator *id = decl->u.var.declarators; id; id = id->next) {
                        int array_size =
                            id->type->kind == TYPE_ARRAY && id->type->u.array.size &&
                                    id->type->u.array.size->kind == EXPR_LITERAL &&
                                    id->type->u.array.size->u.literal->kind == LITERAL_INT
                                ? id->type->u.array.size->u.literal->u.int_val
                                : -1;
                        Tac_Type *type = convert_type(id->type);
                        add_symbol(ctx, id->name, type, array_size);
                        if (id->init) {
                            gen_initializer(ctx, id->init, id->name, type);
                        }
                    }
                }
            }
        }
        // End label
        Tac_Instruction *end_instr = new_tac_instruction(TAC_INSTRUCTION_LABEL);
        end_instr->u.label.name    = end_label;
        append_instruction(ctx, end_instr);
        pop_control_context(ctx);
        break;
    }
    case STMT_WHILE: {
        char *loop_label = new_label(ctx);
        char *end_label  = new_label(ctx);
        push_control_context(ctx, end_label, loop_label);
        // Loop label
        Tac_Instruction *loop_instr = new_tac_instruction(TAC_INSTRUCTION_LABEL);
        loop_instr->u.label.name    = loop_label;
        append_instruction(ctx, loop_instr);
        // Condition
        Tac_Val *cond                       = gen_expr(ctx, stmt->u.while_stmt.condition);
        Tac_Instruction *cond_jump          = new_tac_instruction(TAC_INSTRUCTION_JUMP_IF_ZERO);
        cond_jump->u.jump_if_zero.condition = cond;
        cond_jump->u.jump_if_zero.target    = end_label;
        append_instruction(ctx, cond_jump);
        // Body
        gen_stmt(ctx, stmt->u.while_stmt.body);
        // Jump back to loop
        Tac_Instruction *goto_loop = new_tac_instruction(TAC_INSTRUCTION_JUMP);
        goto_loop->u.jump.target   = loop_label;
        append_instruction(ctx, goto_loop);
        // End label
        Tac_Instruction *end_instr = new_tac_instruction(TAC_INSTRUCTION_LABEL);
        end_instr->u.label.name    = end_label;
        append_instruction(ctx, end_instr);
        pop_control_context(ctx);
        break;
    }
    case STMT_DO_WHILE: {
        char *loop_label = new_label(ctx);
        char *end_label  = new_label(ctx);
        push_control_context(ctx, end_label, loop_label);
        // Loop label
        Tac_Instruction *loop_instr = new_tac_instruction(TAC_INSTRUCTION_LABEL);
        loop_instr->u.label.name    = loop_label;
        append_instruction(ctx, loop_instr);
        // Body
        gen_stmt(ctx, stmt->u.do_while.body);
        // Condition
        Tac_Val *cond              = gen_expr(ctx, stmt->u.do_while.condition);
        Tac_Instruction *cond_jump = new_tac_instruction(TAC_INSTRUCTION_JUMP_IF_NOT_ZERO);
        cond_jump->u.jump_if_not_zero.condition = cond;
        cond_jump->u.jump_if_not_zero.target    = loop_label;
        append_instruction(ctx, cond_jump);
        // End label
        Tac_Instruction *end_instr = new_tac_instruction(TAC_INSTRUCTION_LABEL);
        end_instr->u.label.name    = end_label;
        append_instruction(ctx, end_instr);
        pop_control_context(ctx);
        break;
    }
    case STMT_FOR: {
        char *loop_label   = new_label(ctx);
        char *body_label   = new_label(ctx);
        char *update_label = new_label(ctx);
        char *end_label    = new_label(ctx);
        push_control_context(ctx, end_label, update_label);
        // Initializer
        if (stmt->u.for_stmt.init) {
            if (stmt->u.for_stmt.init->kind == FOR_INIT_EXPR && stmt->u.for_stmt.init->u.expr) {
                gen_expr(ctx, stmt->u.for_stmt.init->u.expr);
            } else if (stmt->u.for_stmt.init->kind == FOR_INIT_DECL) {
                Declaration *decl = stmt->u.for_stmt.init->u.decl;
                if (decl->kind == DECL_VAR) {
                    for (InitDeclarator *id = decl->u.var.declarators; id; id = id->next) {
                        int array_size =
                            id->type->kind == TYPE_ARRAY && id->type->u.array.size &&
                                    id->type->u.array.size->kind == EXPR_LITERAL &&
                                    id->type->u.array.size->u.literal->kind == LITERAL_INT
                                ? id->type->u.array.size->u.literal->u.int_val
                                : -1;
                        Tac_Type *type = convert_type(id->type);
                        add_symbol(ctx, id->name, type, array_size);
                        if (id->init) {
                            gen_initializer(ctx, id->init, id->name, type);
                        }
                    }
                }
            }
        }
        // Loop label (for condition)
        Tac_Instruction *loop_instr = new_tac_instruction(TAC_INSTRUCTION_LABEL);
        loop_instr->u.label.name    = loop_label;
        append_instruction(ctx, loop_instr);
        // Condition
        if (stmt->u.for_stmt.condition) {
            Tac_Val *cond                       = gen_expr(ctx, stmt->u.for_stmt.condition);
            Tac_Instruction *cond_jump          = new_tac_instruction(TAC_INSTRUCTION_JUMP_IF_ZERO);
            cond_jump->u.jump_if_zero.condition = cond;
            cond_jump->u.jump_if_zero.target    = end_label;
            append_instruction(ctx, cond_jump);
        }
        // Body label
        Tac_Instruction *body_instr = new_tac_instruction(TAC_INSTRUCTION_LABEL);
        body_instr->u.label.name    = body_label;
        append_instruction(ctx, body_instr);
        // Body
        gen_stmt(ctx, stmt->u.for_stmt.body);
        // Update label
        Tac_Instruction *update_instr = new_tac_instruction(TAC_INSTRUCTION_LABEL);
        update_instr->u.label.name    = update_label;
        append_instruction(ctx, update_instr);
        // Update expression
        if (stmt->u.for_stmt.update) {
            gen_expr(ctx, stmt->u.for_stmt.update);
        }
        // Jump back to loop
        Tac_Instruction *goto_loop = new_tac_instruction(TAC_INSTRUCTION_JUMP);
        goto_loop->u.jump.target   = loop_label;
        append_instruction(ctx, goto_loop);
        // End label
        Tac_Instruction *end_instr = new_tac_instruction(TAC_INSTRUCTION_LABEL);
        end_instr->u.label.name    = end_label;
        append_instruction(ctx, end_instr);
        pop_control_context(ctx);
        break;
    }
    case STMT_BREAK: {
        char *break_label = get_break_label(ctx);
        if (break_label) {
            Tac_Instruction *instr = new_tac_instruction(TAC_INSTRUCTION_JUMP);
            instr->u.jump.target   = break_label;
            append_instruction(ctx, instr);
        }
        break;
    }
    case STMT_CONTINUE: {
        char *continue_label = get_continue_label(ctx);
        if (continue_label) {
            Tac_Instruction *instr = new_tac_instruction(TAC_INSTRUCTION_JUMP);
            instr->u.jump.target   = continue_label;
            append_instruction(ctx, instr);
        }
        break;
    }
    case STMT_GOTO: {
        char *tac_label = get_tac_label(ctx, stmt->u.goto_label);
        if (tac_label) {
            Tac_Instruction *instr = new_tac_instruction(TAC_INSTRUCTION_JUMP);
            instr->u.jump.target   = tac_label;
            append_instruction(ctx, instr);
        }
        break;
    }
    case STMT_LABELED: {
        char *tac_label = new_label(ctx);
        add_label(ctx, stmt->u.labeled.label, tac_label);
        Tac_Instruction *label_instr = new_tac_instruction(TAC_INSTRUCTION_LABEL);
        label_instr->u.label.name    = tac_label;
        append_instruction(ctx, label_instr);
        gen_stmt(ctx, stmt->u.labeled.stmt);
        break;
    }
    case STMT_COMPOUND: {
        for (DeclOrStmt *item = stmt->u.compound; item; item = item->next) {
            if (item->kind == DECL_OR_STMT_STMT) {
                gen_stmt(ctx, item->u.stmt);
            } else if (item->kind == DECL_OR_STMT_DECL) {
                Declaration *decl = item->u.decl;
                if (decl->kind == DECL_VAR) {
                    for (InitDeclarator *id = decl->u.var.declarators; id; id = id->next) {
                        int array_size =
                            id->type->kind == TYPE_ARRAY && id->type->u.array.size &&
                                    id->type->u.array.size->kind == EXPR_LITERAL &&
                                    id->type->u.array.size->u.literal->kind == LITERAL_INT
                                ? id->type->u.array.size->u.literal->u.int_val
                                : -1;
                        Tac_Type *type = convert_type(id->type);
                        add_symbol(ctx, id->name, type, array_size);
                        if (id->init) {
                            gen_initializer(ctx, id->init, id->name, type);
                        }
                    }
                }
            }
        }
        break;
    }
    default:
        break; // Unsupported statement
    }
}

// Main AST-to-TAC translation function
Tac_Program *ast_to_tac(ExternalDecl *decl)
{
    if (!decl || decl->kind != EXTERNAL_DECL_FUNCTION) {
        return NULL;
    }

    Tac_Program *program = malloc(sizeof(Tac_Program));
    program->decls       = NULL;

    // Create Tac_TopLevel for function
    Tac_TopLevel *toplevel      = new_tac_toplevel(TAC_TOPLEVEL_FUNCTION);
    toplevel->next              = NULL;
    toplevel->u.function.name   = decl->u.function.name ? strdup(decl->u.function.name) : NULL;
    toplevel->u.function.global = decl->u.function.specifiers->storage == STORAGE_CLASS_EXTERN;
    toplevel->u.function.params = convert_params(
        decl->u.function.param_decls ? decl->u.function.param_decls->u.var.declarators : NULL);
    toplevel->u.function.body = NULL;

    // Initialize TAC generation context
    TacGenContext ctx;
    init_tac_gen_context(&ctx);

    // Add parameters to symbol table
    for (Tac_Param *p = toplevel->u.function.params; p; p = p->next) {
        if (p->name) {
            Tac_Type *type = new_tac_type(TAC_TYPE_INT); // Placeholder type
            add_symbol(&ctx, p->name, type, -1);
        }
    }

    // Generate TAC for function body
    if (decl->u.function.body) {
        gen_stmt(&ctx, decl->u.function.body);
    }

    // Set the instruction list
    toplevel->u.function.body = ctx.instr_head;

    // Attach to program
    program->decls = toplevel;

    // Clean up context
    free_tac_gen_context(&ctx);

    return program;
}
