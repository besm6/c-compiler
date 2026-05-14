//
// AST to TAC lowering.
//

#include "translate.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic.h"
#include "symtab.h"
#include "xalloc.h"

// Enable debug output
int translator_debug;

//
// TAC generation helpers
//

typedef struct {
    Tac_Instruction *head;
    Tac_Instruction *tail;
    int temp_id;
    Tac_TopLevel *static_constants; // string-constant toplevel nodes accumulated during body lowering
} TacCtx;

typedef struct CaseEntry {
    Expr             *expr;  // case constant expression
    const char       *label; // non-owning ptr to stmt->branch_target_label
    struct CaseEntry *next;
} CaseEntry;

typedef struct {
    CaseEntry  *head;
    CaseEntry **tail;
    const char *default_label; // non-owning ptr, or NULL
} CaseList;

static void tac_append(TacCtx *ctx, Tac_Instruction *instr)
{
    if (!ctx->head) {
        ctx->head = ctx->tail = instr;
    } else {
        ctx->tail->next = instr;
        ctx->tail       = instr;
    }
    instr->next = NULL;
}

static char *new_temp(TacCtx *ctx)
{
    char buf[32];
    snprintf(buf, sizeof buf, "t.%d", ctx->temp_id++);
    return xstrdup(buf);
}

static Tac_BinaryOperator map_binary_op(BinaryOp op)
{
    switch (op) {
    case BINARY_ADD:
        return TAC_BINARY_ADD;
    case BINARY_SUB:
        return TAC_BINARY_SUBTRACT;
    case BINARY_MUL:
        return TAC_BINARY_MULTIPLY;
    case BINARY_DIV:
        return TAC_BINARY_DIVIDE;
    case BINARY_MOD:
        return TAC_BINARY_REMAINDER;
    case BINARY_LT:
        return TAC_BINARY_LESS_THAN;
    case BINARY_GT:
        return TAC_BINARY_GREATER_THAN;
    case BINARY_LE:
        return TAC_BINARY_LESS_OR_EQUAL;
    case BINARY_GE:
        return TAC_BINARY_GREATER_OR_EQUAL;
    case BINARY_EQ:
        return TAC_BINARY_EQUAL;
    case BINARY_NE:
        return TAC_BINARY_NOT_EQUAL;
    case BINARY_BIT_AND:
        return TAC_BINARY_BITWISE_AND;
    case BINARY_BIT_OR:
        return TAC_BINARY_BITWISE_OR;
    case BINARY_BIT_XOR:
        return TAC_BINARY_BITWISE_XOR;
    case BINARY_LEFT_SHIFT:
        return TAC_BINARY_LEFT_SHIFT;
    case BINARY_RIGHT_SHIFT:
        return TAC_BINARY_RIGHT_SHIFT;
    default:
        fatal_error("Unsupported binary operator in TAC lowering");
    }
}

static Tac_UnaryOperator map_unary_op(UnaryOp op)
{
    switch (op) {
    case UNARY_BIT_NOT:
        return TAC_UNARY_COMPLEMENT;
    case UNARY_NEG:
        return TAC_UNARY_NEGATE;
    case UNARY_LOG_NOT:
        return TAC_UNARY_NOT;
    default:
        fatal_error("Unsupported unary operator in TAC lowering");
    }
}

static Tac_Val *val_int(int v)
{
    Tac_Val *tv    = tac_new_val(TAC_VAL_CONSTANT);
    Tac_Const *c   = tac_new_const(TAC_CONST_INT);
    c->u.int_val   = v;
    tv->u.constant = c;
    return tv;
}

static Tac_Val *val_double(double v)
{
    Tac_Val *tv     = tac_new_val(TAC_VAL_CONSTANT);
    Tac_Const *c    = tac_new_const(TAC_CONST_DOUBLE);
    c->u.double_val = v;
    tv->u.constant  = c;
    return tv;
}

static Tac_Val *val_var(const char *name)
{
    Tac_Val *tv    = tac_new_val(TAC_VAL_VAR);
    tv->u.var_name = xstrdup(name);
    return tv;
}

static Tac_BinaryOperator map_assign_op(AssignOp op)
{
    switch (op) {
    case ASSIGN_ADD:
        return TAC_BINARY_ADD;
    case ASSIGN_SUB:
        return TAC_BINARY_SUBTRACT;
    case ASSIGN_MUL:
        return TAC_BINARY_MULTIPLY;
    case ASSIGN_DIV:
        return TAC_BINARY_DIVIDE;
    case ASSIGN_MOD:
        return TAC_BINARY_REMAINDER;
    case ASSIGN_LEFT:
        return TAC_BINARY_LEFT_SHIFT;
    case ASSIGN_RIGHT:
        return TAC_BINARY_RIGHT_SHIFT;
    case ASSIGN_AND:
        return TAC_BINARY_BITWISE_AND;
    case ASSIGN_XOR:
        return TAC_BINARY_BITWISE_XOR;
    case ASSIGN_OR:
        return TAC_BINARY_BITWISE_OR;
    default:
        fatal_error("Unsupported compound assignment operator in TAC lowering");
    }
}

static const char *lvalue_name(const Expr *e)
{
    if (e->kind == EXPR_VAR)
        return e->u.var;
    fatal_error("lvalue not yet supported: expression kind %d", (int)e->kind);
}

static Tac_Val *new_var_val(TacCtx *ctx)
{
    char *d       = new_temp(ctx);
    Tac_Val *v    = tac_new_val(TAC_VAL_VAR);
    v->u.var_name = d;
    return v;
}

static Tac_Val *emit_cast(TacCtx *ctx, Tac_Val *src, const Type *from, const Type *to)
{
    bool from_int = is_integer(from);
    bool to_int   = is_integer(to);
    Tac_Val *dst  = new_var_val(ctx);

    if (from_int && to_int) {
        size_t from_size = get_size(from);
        size_t to_size   = get_size(to);
        if (to_size < from_size) {
            Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_TRUNCATE);
            in->u.truncate.src  = src;
            in->u.truncate.dst  = dst;
            tac_append(ctx, in);
        } else if (to_size == from_size) {
            Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_COPY);
            in->u.copy.src      = src;
            in->u.copy.dst      = dst;
            tac_append(ctx, in);
        } else if (is_signed(from)) {
            Tac_Instruction *in   = tac_new_instruction(TAC_INSTRUCTION_SIGN_EXTEND);
            in->u.sign_extend.src = src;
            in->u.sign_extend.dst = dst;
            tac_append(ctx, in);
        } else {
            Tac_Instruction *in    = tac_new_instruction(TAC_INSTRUCTION_ZERO_EXTEND);
            in->u.zero_extend.src  = src;
            in->u.zero_extend.dst  = dst;
            tac_append(ctx, in);
        }
    } else if (!from_int && to_int) {
        // double → integer
        if (is_signed(to)) {
            Tac_Instruction *in     = tac_new_instruction(TAC_INSTRUCTION_DOUBLE_TO_INT);
            in->u.double_to_int.src = src;
            in->u.double_to_int.dst = dst;
            tac_append(ctx, in);
        } else {
            Tac_Instruction *in      = tac_new_instruction(TAC_INSTRUCTION_DOUBLE_TO_UINT);
            in->u.double_to_uint.src = src;
            in->u.double_to_uint.dst = dst;
            tac_append(ctx, in);
        }
    } else if (from_int && !to_int) {
        // integer → double
        if (is_signed(from)) {
            Tac_Instruction *in     = tac_new_instruction(TAC_INSTRUCTION_INT_TO_DOUBLE);
            in->u.int_to_double.src = src;
            in->u.int_to_double.dst = dst;
            tac_append(ctx, in);
        } else {
            Tac_Instruction *in      = tac_new_instruction(TAC_INSTRUCTION_UINT_TO_DOUBLE);
            in->u.uint_to_double.src = src;
            in->u.uint_to_double.dst = dst;
            tac_append(ctx, in);
        }
    } else {
        // double → double
        Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_COPY);
        in->u.copy.src      = src;
        in->u.copy.dst      = dst;
        tac_append(ctx, in);
    }
    return val_var(dst->u.var_name);
}

static void emit_jump(TacCtx *ctx, const char *target)
{
    Tac_Instruction *j = tac_new_instruction(TAC_INSTRUCTION_JUMP);
    j->u.jump.target   = xstrdup(target);
    tac_append(ctx, j);
}

static void emit_label(TacCtx *ctx, const char *name)
{
    Tac_Instruction *l = tac_new_instruction(TAC_INSTRUCTION_LABEL);
    l->u.label.name    = xstrdup(name);
    tac_append(ctx, l);
}

static Tac_Val *gen_expr(TacCtx *ctx, Expr *e);
static Tac_Type *ast_type_to_tac_type(const Type *t);

static Tac_Val *gen_logical_and(TacCtx *ctx, Expr *l, Expr *r)
{
    Tac_Val *left  = gen_expr(ctx, l);
    char *false_l  = new_temp(ctx);
    char *end_l    = new_temp(ctx);
    char *dst_name = new_temp(ctx);

    Tac_Instruction *jz          = tac_new_instruction(TAC_INSTRUCTION_JUMP_IF_ZERO);
    jz->u.jump_if_zero.condition = left;
    jz->u.jump_if_zero.target    = false_l; // instruction takes ownership
    tac_append(ctx, jz);

    Tac_Val *right       = gen_expr(ctx, r);
    Tac_Instruction *bin = tac_new_instruction(TAC_INSTRUCTION_BINARY);
    bin->u.binary.op     = TAC_BINARY_NOT_EQUAL;
    bin->u.binary.src1   = right;
    bin->u.binary.src2   = val_int(0);
    bin->u.binary.dst    = val_var(dst_name);
    tac_append(ctx, bin);
    emit_jump(ctx, end_l);

    emit_label(ctx, false_l); // false_l still valid; owned by jz
    Tac_Instruction *cp = tac_new_instruction(TAC_INSTRUCTION_COPY);
    cp->u.copy.src      = val_int(0);
    cp->u.copy.dst      = val_var(dst_name);
    tac_append(ctx, cp);

    emit_label(ctx, end_l);
    xfree(end_l);
    Tac_Val *result = val_var(dst_name);
    xfree(dst_name);
    return result;
}

static Tac_Val *gen_logical_or(TacCtx *ctx, Expr *l, Expr *r)
{
    Tac_Val *left  = gen_expr(ctx, l);
    char *true_l   = new_temp(ctx);
    char *end_l    = new_temp(ctx);
    char *dst_name = new_temp(ctx);

    Tac_Instruction *jnz              = tac_new_instruction(TAC_INSTRUCTION_JUMP_IF_NOT_ZERO);
    jnz->u.jump_if_not_zero.condition = left;
    jnz->u.jump_if_not_zero.target    = true_l; // instruction takes ownership
    tac_append(ctx, jnz);

    Tac_Val *right       = gen_expr(ctx, r);
    Tac_Instruction *bin = tac_new_instruction(TAC_INSTRUCTION_BINARY);
    bin->u.binary.op     = TAC_BINARY_NOT_EQUAL;
    bin->u.binary.src1   = right;
    bin->u.binary.src2   = val_int(0);
    bin->u.binary.dst    = val_var(dst_name);
    tac_append(ctx, bin);
    emit_jump(ctx, end_l);

    emit_label(ctx, true_l); // true_l still valid; owned by jnz
    Tac_Instruction *cp = tac_new_instruction(TAC_INSTRUCTION_COPY);
    cp->u.copy.src      = val_int(1);
    cp->u.copy.dst      = val_var(dst_name);
    tac_append(ctx, cp);

    emit_label(ctx, end_l);
    xfree(end_l);
    Tac_Val *result = val_var(dst_name);
    xfree(dst_name);
    return result;
}

static Tac_Val *gen_unary(TacCtx *ctx, UnaryOp op, Expr *inner)
{
    Tac_Val *src = gen_expr(ctx, inner);
    Tac_Val *vd  = new_var_val(ctx);

    Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_UNARY);
    in->u.unary.op      = map_unary_op(op);
    in->u.unary.src     = src;
    in->u.unary.dst     = vd;
    tac_append(ctx, in);
    // Return a fresh val so callers can store it in a second instruction
    // without aliasing vd (which is already owned by this instruction).
    return val_var(vd->u.var_name);
}

static Tac_Val *gen_binary(TacCtx *ctx, BinaryOp op, Expr *l, Expr *r)
{
    Tac_Val *vl = gen_expr(ctx, l);
    Tac_Val *vr = gen_expr(ctx, r);
    Tac_Val *vd = new_var_val(ctx);

    Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_BINARY);
    in->u.binary.op     = map_binary_op(op);
    in->u.binary.src1   = vl;
    in->u.binary.src2   = vr;
    in->u.binary.dst    = vd;
    tac_append(ctx, in);
    // Return a fresh val so callers can store it in a second instruction
    // without aliasing vd (which is already owned by this instruction).
    return val_var(vd->u.var_name);
}

static Tac_Val *gen_expr(TacCtx *ctx, Expr *e)
{
    if (!e) {
        fatal_error("NULL expression in TAC lowering");
    }
    switch (e->kind) {
    case EXPR_LITERAL:
        switch (e->u.literal->kind) {
        case LITERAL_INT:
            return val_int(e->u.literal->u.int_val);
        case LITERAL_FLOAT:
            return val_double(e->u.literal->u.real_val);
        case LITERAL_CHAR:
            return val_int(e->u.literal->u.char_val);
        case LITERAL_STRING: {
            const char *sname = symtab_add_string(e->u.literal->u.string_val);
            Symbol *sym       = symtab_get(sname);

            Tac_TopLevel *sc          = tac_new_toplevel(TAC_TOPLEVEL_STATIC_CONSTANT);
            sc->u.static_constant.name = xstrdup(sname);
            sc->u.static_constant.type = ast_type_to_tac_type(sym->type);
            sc->u.static_constant.init = sym->u.const_init;
            sym->u.const_init          = NULL; // transfer ownership to TAC node

            sc->next              = ctx->static_constants;
            ctx->static_constants = sc;

            Tac_Val *dst           = new_var_val(ctx);
            Tac_Instruction *in    = tac_new_instruction(TAC_INSTRUCTION_GET_ADDRESS);
            in->u.get_address.src  = val_var(sname);
            in->u.get_address.dst  = dst;
            tac_append(ctx, in);

            xfree((char *)sname);
            return val_var(dst->u.var_name);
        }
        default:
            fatal_error("Unsupported literal in TAC lowering");
        }
    case EXPR_VAR:
        return val_var(e->u.var);
    case EXPR_UNARY_OP:
        if (e->u.unary_op.op == UNARY_PLUS)
            return gen_expr(ctx, e->u.unary_op.expr);
        if (e->u.unary_op.op == UNARY_PRE_INC || e->u.unary_op.op == UNARY_PRE_DEC) {
            const char *var      = lvalue_name(e->u.unary_op.expr);
            Tac_Val *vd          = new_var_val(ctx);
            Tac_Instruction *bin = tac_new_instruction(TAC_INSTRUCTION_BINARY);
            bin->u.binary.op     = (e->u.unary_op.op == UNARY_PRE_INC) ? TAC_BINARY_ADD : TAC_BINARY_SUBTRACT;
            bin->u.binary.src1   = val_var(var);
            bin->u.binary.src2   = val_int(1);
            bin->u.binary.dst    = vd;
            tac_append(ctx, bin);
            Tac_Instruction *cp = tac_new_instruction(TAC_INSTRUCTION_COPY);
            cp->u.copy.src      = val_var(vd->u.var_name);
            cp->u.copy.dst      = val_var(var);
            tac_append(ctx, cp);
            return val_var(vd->u.var_name);
        }
        return gen_unary(ctx, e->u.unary_op.op, e->u.unary_op.expr);
    case EXPR_BINARY_OP:
        if (e->u.binary_op.op == BINARY_LOG_AND)
            return gen_logical_and(ctx, e->u.binary_op.left, e->u.binary_op.right);
        if (e->u.binary_op.op == BINARY_LOG_OR)
            return gen_logical_or(ctx, e->u.binary_op.left, e->u.binary_op.right);
        return gen_binary(ctx, e->u.binary_op.op, e->u.binary_op.left, e->u.binary_op.right);
    case EXPR_ASSIGN: {
        const char *dst = lvalue_name(e->u.assign.target);
        Tac_Val *src    = gen_expr(ctx, e->u.assign.value);
        if (e->u.assign.op == ASSIGN_SIMPLE) {
            Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_COPY);
            in->u.copy.src      = src;
            in->u.copy.dst      = val_var(dst);
            tac_append(ctx, in);
        } else {
            Tac_Val *vd          = new_var_val(ctx);
            Tac_Instruction *bin = tac_new_instruction(TAC_INSTRUCTION_BINARY);
            bin->u.binary.op     = map_assign_op(e->u.assign.op);
            bin->u.binary.src1   = val_var(dst);
            bin->u.binary.src2   = src;
            bin->u.binary.dst    = vd;
            tac_append(ctx, bin);
            Tac_Instruction *cp = tac_new_instruction(TAC_INSTRUCTION_COPY);
            cp->u.copy.src      = val_var(vd->u.var_name);
            cp->u.copy.dst      = val_var(dst);
            tac_append(ctx, cp);
        }
        return val_var(dst);
    }
    case EXPR_COND: {
        Tac_Val *cond_val = gen_expr(ctx, e->u.cond.condition);
        char *else_l      = new_temp(ctx);
        char *end_l       = new_temp(ctx);
        char *dst_name    = new_temp(ctx);

        Tac_Instruction *jz          = tac_new_instruction(TAC_INSTRUCTION_JUMP_IF_ZERO);
        jz->u.jump_if_zero.condition = cond_val;
        jz->u.jump_if_zero.target    = else_l; // instruction takes ownership
        tac_append(ctx, jz);

        Tac_Val *then_val        = gen_expr(ctx, e->u.cond.then_expr);
        Tac_Instruction *cp_then = tac_new_instruction(TAC_INSTRUCTION_COPY);
        cp_then->u.copy.src      = then_val;
        cp_then->u.copy.dst      = val_var(dst_name);
        tac_append(ctx, cp_then);
        emit_jump(ctx, end_l);

        emit_label(ctx, else_l);
        Tac_Val *else_val        = gen_expr(ctx, e->u.cond.else_expr);
        Tac_Instruction *cp_else = tac_new_instruction(TAC_INSTRUCTION_COPY);
        cp_else->u.copy.src      = else_val;
        cp_else->u.copy.dst      = val_var(dst_name);
        tac_append(ctx, cp_else);

        emit_label(ctx, end_l);
        xfree(end_l);
        Tac_Val *result = val_var(dst_name);
        xfree(dst_name);
        return result;
    }
    case EXPR_CAST: {
        Tac_Val *inner = gen_expr(ctx, e->u.cast.expr);
        return emit_cast(ctx, inner, e->u.cast.expr->type, e->u.cast.type);
    }
    case EXPR_CALL: {
        Tac_Val *args_head = NULL;
        Tac_Val **args_tail = &args_head;
        for (Expr *arg = e->u.call.args; arg; arg = arg->next) {
            Tac_Val *av = gen_expr(ctx, arg);
            *args_tail  = av;
            args_tail   = &av->next;
        }

        Expr *func = e->u.call.func;
        if (func->kind != EXPR_VAR)
            fatal_error("Indirect call through non-variable expression not yet supported");
        const char *fun_name = func->u.var;

        Tac_Val *dst = (e->type->kind != TYPE_VOID) ? new_var_val(ctx) : NULL;

        Tac_Instruction *in     = tac_new_instruction(TAC_INSTRUCTION_FUN_CALL);
        in->u.fun_call.fun_name = xstrdup(fun_name);
        in->u.fun_call.args     = args_head;
        in->u.fun_call.dst      = dst;
        tac_append(ctx, in);

        return dst ? val_var(dst->u.var_name) : val_int(0);
    }
    case EXPR_POST_INC:
    case EXPR_POST_DEC: {
        const char *var      = lvalue_name(e->kind == EXPR_POST_INC ? e->u.post_inc : e->u.post_dec);
        Tac_Val *old         = new_var_val(ctx);
        Tac_Instruction *cp1 = tac_new_instruction(TAC_INSTRUCTION_COPY);
        cp1->u.copy.src      = val_var(var);
        cp1->u.copy.dst      = old;
        tac_append(ctx, cp1);
        Tac_Val *vd          = new_var_val(ctx);
        Tac_Instruction *bin = tac_new_instruction(TAC_INSTRUCTION_BINARY);
        bin->u.binary.op     = (e->kind == EXPR_POST_INC) ? TAC_BINARY_ADD : TAC_BINARY_SUBTRACT;
        bin->u.binary.src1   = val_var(var);
        bin->u.binary.src2   = val_int(1);
        bin->u.binary.dst    = vd;
        tac_append(ctx, bin);
        Tac_Instruction *cp2 = tac_new_instruction(TAC_INSTRUCTION_COPY);
        cp2->u.copy.src      = val_var(vd->u.var_name);
        cp2->u.copy.dst      = val_var(var);
        tac_append(ctx, cp2);
        return val_var(old->u.var_name);
    }
    default:
        fatal_error("Unsupported expression kind %d in TAC lowering", (int)e->kind);
    }
}

static void gen_stmt(TacCtx *ctx, Stmt *stmt);

static void collect_cases(TacCtx *ctx, Stmt *stmt, CaseList *list)
{
    if (!stmt)
        return;
    switch (stmt->kind) {
    case STMT_CASE: {
        stmt->branch_target_label = new_temp(ctx);
        CaseEntry *e              = xalloc(sizeof *e, __func__, __FILE__, __LINE__);
        e->expr                   = stmt->u.case_stmt.expr;
        e->label                  = stmt->branch_target_label;
        e->next                   = NULL;
        *list->tail               = e;
        list->tail                = &e->next;
        collect_cases(ctx, stmt->u.case_stmt.stmt, list);
        break;
    }
    case STMT_DEFAULT:
        stmt->branch_target_label = new_temp(ctx);
        list->default_label       = stmt->branch_target_label;
        collect_cases(ctx, stmt->u.default_stmt, list);
        break;
    case STMT_COMPOUND:
        for (DeclOrStmt *ds = stmt->u.compound; ds; ds = ds->next)
            if (ds->kind == DECL_OR_STMT_STMT)
                collect_cases(ctx, ds->u.stmt, list);
        break;
    case STMT_IF:
        collect_cases(ctx, stmt->u.if_stmt.then_stmt, list);
        collect_cases(ctx, stmt->u.if_stmt.else_stmt, list);
        break;
    case STMT_WHILE:
        collect_cases(ctx, stmt->u.while_stmt.body, list);
        break;
    case STMT_DO_WHILE:
        collect_cases(ctx, stmt->u.do_while.body, list);
        break;
    case STMT_FOR:
        collect_cases(ctx, stmt->u.for_stmt.body, list);
        break;
    case STMT_LABELED:
        collect_cases(ctx, stmt->u.labeled.stmt, list);
        break;
    default: // STMT_SWITCH and leaf statements: do not recurse
        break;
    }
}

static void gen_local_decl(TacCtx *ctx, const Declaration *decl)
{
    if (decl->kind != DECL_VAR)
        return;
    for (InitDeclarator *id = decl->u.var.declarators; id; id = id->next) {
        if (id->init && id->init->kind == INITIALIZER_SINGLE) {
            Tac_Val *src        = gen_expr(ctx, id->init->u.expr);
            Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_COPY);
            in->u.copy.src      = src;
            in->u.copy.dst      = val_var(id->name);
            tac_append(ctx, in);
        }
    }
}

static void gen_stmt(TacCtx *ctx, Stmt *stmt)
{
    if (!stmt) {
        return;
    }
    switch (stmt->kind) {
    case STMT_COMPOUND: {
        for (DeclOrStmt *ds = stmt->u.compound; ds; ds = ds->next) {
            if (ds->kind == DECL_OR_STMT_DECL) {
                gen_local_decl(ctx, ds->u.decl);
            } else {
                gen_stmt(ctx, ds->u.stmt);
            }
        }
        break;
    }
    case STMT_EXPR:
        if (stmt->u.expr) {
            tac_free_val(gen_expr(ctx, stmt->u.expr));
        }
        break;
    case STMT_RETURN: {
        Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_RETURN);
        in->u.return_.src   = stmt->u.expr ? gen_expr(ctx, stmt->u.expr) : NULL;
        tac_append(ctx, in);
        break;
    }
    case STMT_IF: {
        Tac_Val *cond = gen_expr(ctx, stmt->u.if_stmt.condition);
        char *else_l  = new_temp(ctx);
        char *end_l   = new_temp(ctx);

        Tac_Instruction *jz          = tac_new_instruction(TAC_INSTRUCTION_JUMP_IF_ZERO);
        jz->u.jump_if_zero.condition = cond;
        jz->u.jump_if_zero.target    = else_l; // instruction takes ownership
        tac_append(ctx, jz);
        gen_stmt(ctx, stmt->u.if_stmt.then_stmt);
        emit_jump(ctx, end_l);
        emit_label(ctx, else_l);
        if (stmt->u.if_stmt.else_stmt) {
            gen_stmt(ctx, stmt->u.if_stmt.else_stmt);
        }
        emit_label(ctx, end_l);
        xfree(end_l); // emit_jump and emit_label each xstrdup; free the original
        break;
    }
    case STMT_WHILE: {
        const char *cl = stmt->loop_continue_label;
        const char *bl = stmt->loop_end_label;
        if (!cl || !bl) {
            fatal_error("while: missing loop labels (label_loops not run?)");
        }
        emit_label(ctx, cl);
        Tac_Val *cond                = gen_expr(ctx, stmt->u.while_stmt.condition);
        Tac_Instruction *jz          = tac_new_instruction(TAC_INSTRUCTION_JUMP_IF_ZERO);
        jz->u.jump_if_zero.condition = cond;
        jz->u.jump_if_zero.target    = xstrdup(bl);
        tac_append(ctx, jz);
        gen_stmt(ctx, stmt->u.while_stmt.body);
        emit_jump(ctx, cl);
        emit_label(ctx, bl);
        break;
    }
    case STMT_DO_WHILE: {
        const char *cl = stmt->loop_continue_label;
        const char *bl = stmt->loop_end_label;
        if (!cl || !bl) {
            fatal_error("do-while: missing loop labels");
        }
        char *loop_top = new_temp(ctx);
        emit_label(ctx, loop_top);
        gen_stmt(ctx, stmt->u.do_while.body);
        emit_label(ctx, cl);
        Tac_Val *cond                     = gen_expr(ctx, stmt->u.do_while.condition);
        Tac_Instruction *jnz              = tac_new_instruction(TAC_INSTRUCTION_JUMP_IF_NOT_ZERO);
        jnz->u.jump_if_not_zero.condition = cond;
        jnz->u.jump_if_not_zero.target    = loop_top;
        tac_append(ctx, jnz);
        emit_label(ctx, bl);
        break;
    }
    case STMT_FOR: {
        const char *cl = stmt->loop_continue_label;
        const char *bl = stmt->loop_end_label;
        if (!cl || !bl) {
            fatal_error("for: missing loop labels");
        }
        if (stmt->u.for_stmt.init) {
            if (stmt->u.for_stmt.init->kind == FOR_INIT_EXPR) {
                if (stmt->u.for_stmt.init->u.expr) {
                    tac_free_val(gen_expr(ctx, stmt->u.for_stmt.init->u.expr));
                }
            } else {
                gen_local_decl(ctx, stmt->u.for_stmt.init->u.decl);
            }
        }
        char *test_lab = new_temp(ctx);
        emit_label(ctx, test_lab);
        if (stmt->u.for_stmt.condition) {
            Tac_Val *cond                = gen_expr(ctx, stmt->u.for_stmt.condition);
            Tac_Instruction *jz          = tac_new_instruction(TAC_INSTRUCTION_JUMP_IF_ZERO);
            jz->u.jump_if_zero.condition = cond;
            jz->u.jump_if_zero.target    = xstrdup(bl);
            tac_append(ctx, jz);
        }
        gen_stmt(ctx, stmt->u.for_stmt.body);
        emit_label(ctx, cl);
        if (stmt->u.for_stmt.update) {
            tac_free_val(gen_expr(ctx, stmt->u.for_stmt.update));
        }
        emit_jump(ctx, test_lab);
        xfree(test_lab); // emit_label and emit_jump each xstrdup; free the original
        emit_label(ctx, bl);
        break;
    }
    case STMT_SWITCH: {
        if (!stmt->loop_end_label)
            fatal_error("switch: missing end label (label_loops not run?)");

        CaseList cases  = {NULL, NULL, NULL};
        cases.tail      = &cases.head;
        collect_cases(ctx, stmt->u.switch_stmt.body, &cases);

        Tac_Val *ctrl_raw      = gen_expr(ctx, stmt->u.switch_stmt.expr);
        Tac_Val *ctrl_dst      = new_var_val(ctx);
        const char *ctrl_name  = ctrl_dst->u.var_name; // save before ownership transfer
        Tac_Instruction *cp    = tac_new_instruction(TAC_INSTRUCTION_COPY);
        cp->u.copy.src         = ctrl_raw;
        cp->u.copy.dst         = ctrl_dst;
        tac_append(ctx, cp);

        for (CaseEntry *e = cases.head; e; e = e->next) {
            Tac_Val *cval         = gen_expr(ctx, e->expr);
            Tac_Val *cmp_dst      = new_var_val(ctx);
            const char *cmp_name  = cmp_dst->u.var_name;
            Tac_Instruction *bin  = tac_new_instruction(TAC_INSTRUCTION_BINARY);
            bin->u.binary.op      = TAC_BINARY_EQUAL;
            bin->u.binary.src1    = val_var(ctrl_name);
            bin->u.binary.src2    = cval;
            bin->u.binary.dst     = cmp_dst;
            tac_append(ctx, bin);
            Tac_Instruction *jnz              = tac_new_instruction(TAC_INSTRUCTION_JUMP_IF_NOT_ZERO);
            jnz->u.jump_if_not_zero.condition = val_var(cmp_name);
            jnz->u.jump_if_not_zero.target    = xstrdup(e->label);
            tac_append(ctx, jnz);
        }

        emit_jump(ctx, cases.default_label ? cases.default_label : stmt->loop_end_label);
        gen_stmt(ctx, stmt->u.switch_stmt.body);
        emit_label(ctx, stmt->loop_end_label);

        for (CaseEntry *e = cases.head; e;) {
            CaseEntry *nx = e->next;
            xfree(e);
            e = nx;
        }
        break;
    }
    case STMT_BREAK: {
        if (!stmt->branch_target_label) {
            fatal_error("break without target label");
        }
        emit_jump(ctx, stmt->branch_target_label);
        break;
    }
    case STMT_CONTINUE: {
        if (!stmt->branch_target_label) {
            fatal_error("continue without target label");
        }
        emit_jump(ctx, stmt->branch_target_label);
        break;
    }
    case STMT_GOTO:
        emit_jump(ctx, stmt->u.goto_label);
        break;
    case STMT_LABELED:
        emit_label(ctx, stmt->u.labeled.label);
        gen_stmt(ctx, stmt->u.labeled.stmt);
        break;
    case STMT_CASE:
        if (!stmt->branch_target_label)
            fatal_error("case: missing label (collect_cases not run?)");
        emit_label(ctx, stmt->branch_target_label);
        gen_stmt(ctx, stmt->u.case_stmt.stmt);
        break;
    case STMT_DEFAULT:
        if (!stmt->branch_target_label)
            fatal_error("default: missing label (collect_cases not run?)");
        emit_label(ctx, stmt->branch_target_label);
        gen_stmt(ctx, stmt->u.default_stmt);
        break;
    default:
        fatal_error("Unsupported statement kind %d in TAC lowering", (int)stmt->kind);
    }
}

static Tac_Param *params_from_type(const Type *fun_type)
{
    if (!fun_type || fun_type->kind != TYPE_FUNCTION) {
        return NULL;
    }
    Tac_Param *head  = NULL;
    Tac_Param **tail = &head;
    for (const Param *p = fun_type->u.function.params; p; p = p->next) {
        if (!p->name)
            continue; // skip void sentinel and unnamed params
        Tac_Param *tp = tac_new_param();
        tp->name      = xstrdup(p->name);
        *tail         = tp;
        tail          = &tp->next;
    }
    return head;
}

static Tac_Type *ast_type_to_tac_type(const Type *t)
{
    switch (t->kind) {
    case TYPE_VOID:      return tac_new_type(TAC_TYPE_VOID);
    case TYPE_CHAR:      return tac_new_type(TAC_TYPE_CHAR);
    case TYPE_SCHAR:     return tac_new_type(TAC_TYPE_SCHAR);
    case TYPE_UCHAR:     return tac_new_type(TAC_TYPE_UCHAR);
    case TYPE_SHORT:     return tac_new_type(TAC_TYPE_SHORT);
    case TYPE_USHORT:    return tac_new_type(TAC_TYPE_USHORT);
    case TYPE_INT:       return tac_new_type(TAC_TYPE_INT);
    case TYPE_UINT:      return tac_new_type(TAC_TYPE_UINT);
    case TYPE_LONG:      return tac_new_type(TAC_TYPE_LONG);
    case TYPE_ULONG:     return tac_new_type(TAC_TYPE_ULONG);
    case TYPE_LONG_LONG: return tac_new_type(TAC_TYPE_LONG_LONG);
    case TYPE_ULONG_LONG:return tac_new_type(TAC_TYPE_ULONG_LONG);
    case TYPE_FLOAT:     return tac_new_type(TAC_TYPE_FLOAT);
    case TYPE_DOUBLE:    return tac_new_type(TAC_TYPE_DOUBLE);
    case TYPE_ENUM:      return tac_new_type(TAC_TYPE_INT);
    case TYPE_POINTER: {
        Tac_Type *tp               = tac_new_type(TAC_TYPE_POINTER);
        tp->u.pointer.target_type  = ast_type_to_tac_type(t->u.pointer.target);
        return tp;
    }
    case TYPE_ARRAY: {
        Tac_Type *ta          = tac_new_type(TAC_TYPE_ARRAY);
        ta->u.array.elem_type = ast_type_to_tac_type(t->u.array.element);
        ta->u.array.size      = (int)(get_size(t) / get_size(t->u.array.element));
        return ta;
    }
    case TYPE_FUNCTION: {
        Tac_Type *tf          = tac_new_type(TAC_TYPE_FUN_TYPE);
        Tac_Type **param_tail = &tf->u.fun_type.param_types;
        for (const Param *p = t->u.function.params; p; p = p->next) {
            if (!p->name && p->type->kind == TYPE_VOID)
                continue; // skip void sentinel
            Tac_Type *pt = ast_type_to_tac_type(p->type);
            *param_tail  = pt;
            param_tail   = &pt->next;
        }
        tf->u.fun_type.ret_type = ast_type_to_tac_type(t->u.function.return_type);
        return tf;
    }
    case TYPE_STRUCT:
    case TYPE_UNION: {
        Tac_Type *ts    = tac_new_type(TAC_TYPE_STRUCTURE);
        ts->u.structure.tag = t->u.struct_t.name ? xstrdup(t->u.struct_t.name) : NULL;
        return ts;
    }
    default:
        fatal_error("ast_type_to_tac_type: unsupported type kind %d", (int)t->kind);
    }
}

static Tac_TopLevel *translate_fn(ExternalDecl *ast)
{
    const char *name  = ast->u.function.name;
    const Symbol *sym = symtab_get(name);

    Tac_TopLevel *tl      = tac_new_toplevel(TAC_TOPLEVEL_FUNCTION);
    tl->u.function.name   = xstrdup(name);
    tl->u.function.global = sym->u.func.global;
    tl->u.function.params = params_from_type(ast->u.function.type);

    if (ast->u.function.body) {
        TacCtx ctx = { NULL, NULL, 0, NULL };
        gen_stmt(&ctx, ast->u.function.body);
        tl->u.function.body = ctx.head;

        if (ctx.static_constants) {
            Tac_TopLevel *last = ctx.static_constants;
            while (last->next)
                last = last->next;
            last->next = tl;
            return ctx.static_constants;
        }
    }
    return tl;
}

static Tac_TopLevel *translate_decl(const Declaration *decl)
{
    if (decl->kind != DECL_VAR)
        return NULL;
    if (decl->u.var.specifiers && decl->u.var.specifiers->storage == STORAGE_CLASS_TYPEDEF)
        return NULL;

    Tac_TopLevel *head  = NULL;
    Tac_TopLevel **tail = &head;
    for (const InitDeclarator *id = decl->u.var.declarators; id; id = id->next) {
        Symbol *sym      = symtab_get(id->name);
        Tac_TopLevel *tl;

        if (id->type->kind == TYPE_FUNCTION) {
            tl = tac_new_toplevel(TAC_TOPLEVEL_FUNCTION);
            tl->u.function.name   = xstrdup(id->name);
            tl->u.function.global = sym->u.func.global;
            tl->u.function.params = params_from_type(id->type);
            // body stays NULL — this is a prototype
        } else {
            tl = tac_new_toplevel(TAC_TOPLEVEL_STATIC_VARIABLE);
            tl->u.static_variable.name      = xstrdup(id->name);
            tl->u.static_variable.global    = sym->u.static_var.global;
            tl->u.static_variable.type      = ast_type_to_tac_type(sym->type);
            tl->u.static_variable.init_list = sym->u.static_var.init_list;
            sym->u.static_var.init_list     = NULL; // transfer ownership to TAC
        }
        *tail = tl;
        tail  = &tl->next;
    }
    return head;
}

static Tac_TopLevel *translate_external_decl(ExternalDecl *ast)
{
    if (!ast)
        return NULL;
    switch (ast->kind) {
    case EXTERNAL_DECL_FUNCTION:    return translate_fn(ast);
    case EXTERNAL_DECL_DECLARATION: return translate_decl(ast->u.declaration);
    }
    return NULL;
}

//
// Convert the AST to TAC.
//
Tac_TopLevel *translate(ExternalDecl *ast) // cppcheck-suppress constParameterPointer
{
    return translate_external_decl(ast);
}
