//
// AST to TAC lowering (initial subset).
//

#include "translate_gen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "translator.h"
#include "xalloc.h"

typedef struct {
    Tac_Instruction *head;
    Tac_Instruction *tail;
    int              temp_id;
} TacCtx;

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
    Tac_Val *tv         = new_tac_val(TAC_VAL_CONSTANT);
    Tac_Const *c        = new_tac_const(TAC_CONST_INT);
    c->u.int_val        = v;
    tv->u.constant      = c;
    return tv;
}

static Tac_Val *val_double(double v)
{
    Tac_Val *tv    = new_tac_val(TAC_VAL_CONSTANT);
    Tac_Const *c   = new_tac_const(TAC_CONST_DOUBLE);
    c->u.double_val = v;
    tv->u.constant = c;
    return tv;
}

static Tac_Val *val_var(const char *name)
{
    Tac_Val *tv    = new_tac_val(TAC_VAL_VAR);
    tv->u.var_name = xstrdup(name);
    return tv;
}

static Tac_Val *new_var_val(TacCtx *ctx)
{
    char    *d = new_temp(ctx);
    Tac_Val *v = new_tac_val(TAC_VAL_VAR);
    v->u.var_name = d;
    return v;
}

static Tac_Val *gen_expr(TacCtx *ctx, Expr *e);

static Tac_Val *gen_unary(TacCtx *ctx, UnaryOp op, Expr *inner)
{
    Tac_Val *src = gen_expr(ctx, inner);
    Tac_Val *vd  = new_var_val(ctx);

    Tac_Instruction *in = new_tac_instruction(TAC_INSTRUCTION_UNARY);
    in->u.unary.op      = map_unary_op(op);
    in->u.unary.src     = src;
    in->u.unary.dst     = vd;
    tac_append(ctx, in);
    return vd;
}

static Tac_Val *gen_binary(TacCtx *ctx, BinaryOp op, Expr *l, Expr *r)
{
    Tac_Val *vl = gen_expr(ctx, l);
    Tac_Val *vr = gen_expr(ctx, r);
    Tac_Val *vd = new_var_val(ctx);

    Tac_Instruction *in = new_tac_instruction(TAC_INSTRUCTION_BINARY);
    in->u.binary.op       = map_binary_op(op);
    in->u.binary.src1     = vl;
    in->u.binary.src2     = vr;
    in->u.binary.dst      = vd;
    tac_append(ctx, in);
    return vd;
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
        default:
            fatal_error("Unsupported literal in TAC lowering");
        }
    case EXPR_VAR:
        return val_var(e->u.var);
    case EXPR_UNARY_OP:
        return gen_unary(ctx, e->u.unary_op.op, e->u.unary_op.expr);
    case EXPR_BINARY_OP:
        return gen_binary(ctx, e->u.binary_op.op, e->u.binary_op.left, e->u.binary_op.right);
    default:
        fatal_error("Unsupported expression kind %d in TAC lowering", (int)e->kind);
    }
}

static void gen_stmt(TacCtx *ctx, Stmt *stmt);

static void emit_jump(TacCtx *ctx, const char *target)
{
    Tac_Instruction *j = new_tac_instruction(TAC_INSTRUCTION_JUMP);
    j->u.jump.target   = xstrdup(target);
    tac_append(ctx, j);
}

static void emit_label(TacCtx *ctx, const char *name)
{
    Tac_Instruction *l = new_tac_instruction(TAC_INSTRUCTION_LABEL);
    l->u.label.name    = xstrdup(name);
    tac_append(ctx, l);
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
                fatal_error("Local declarations not yet lowered to TAC");
            }
            gen_stmt(ctx, ds->u.stmt);
        }
        break;
    }
    case STMT_EXPR:
        if (stmt->u.expr) {
            gen_expr(ctx, stmt->u.expr);
        }
        break;
    case STMT_RETURN: {
        Tac_Instruction *in = new_tac_instruction(TAC_INSTRUCTION_RETURN);
        in->u.return_.src     = stmt->u.expr ? gen_expr(ctx, stmt->u.expr) : NULL;
        tac_append(ctx, in);
        break;
    }
    case STMT_IF: {
        Tac_Val *cond   = gen_expr(ctx, stmt->u.if_stmt.condition);
        char          *else_l = new_temp(ctx);
        const char    *end_l  = new_temp(ctx);

        Tac_Instruction *jz = new_tac_instruction(TAC_INSTRUCTION_JUMP_IF_ZERO);
        jz->u.jump_if_zero.condition = cond;
        jz->u.jump_if_zero.target    = else_l;
        tac_append(ctx, jz);
        gen_stmt(ctx, stmt->u.if_stmt.then_stmt);
        emit_jump(ctx, end_l);
        emit_label(ctx, else_l);
        if (stmt->u.if_stmt.else_stmt) {
            gen_stmt(ctx, stmt->u.if_stmt.else_stmt);
        }
        emit_label(ctx, end_l);
        break;
    }
    case STMT_WHILE: {
        const char *cl = stmt->loop_continue_label;
        const char *bl = stmt->loop_end_label;
        if (!cl || !bl) {
            fatal_error("while: missing loop labels (label_loops not run?)");
        }
        emit_label(ctx, cl);
        Tac_Val *cond = gen_expr(ctx, stmt->u.while_stmt.condition);
        Tac_Instruction *jz = new_tac_instruction(TAC_INSTRUCTION_JUMP_IF_ZERO);
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
        Tac_Val *cond = gen_expr(ctx, stmt->u.do_while.condition);
        Tac_Instruction *jnz = new_tac_instruction(TAC_INSTRUCTION_JUMP_IF_NOT_ZERO);
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
                    gen_expr(ctx, stmt->u.for_stmt.init->u.expr);
                }
            } else {
                fatal_error("for-init declaration not supported in TAC yet");
            }
        }
        const char *test_lab = new_temp(ctx);
        emit_label(ctx, test_lab);
        if (stmt->u.for_stmt.condition) {
            Tac_Val *cond = gen_expr(ctx, stmt->u.for_stmt.condition);
            Tac_Instruction *jz = new_tac_instruction(TAC_INSTRUCTION_JUMP_IF_ZERO);
            jz->u.jump_if_zero.condition = cond;
            jz->u.jump_if_zero.target    = xstrdup(bl);
            tac_append(ctx, jz);
        }
        gen_stmt(ctx, stmt->u.for_stmt.body);
        emit_label(ctx, cl);
        if (stmt->u.for_stmt.update) {
            gen_expr(ctx, stmt->u.for_stmt.update);
        }
        emit_jump(ctx, test_lab);
        emit_label(ctx, bl);
        break;
    }
    case STMT_SWITCH:
        fatal_error("switch not yet lowered to TAC");
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
        fatal_error("goto not yet lowered to TAC");
    case STMT_LABELED:
        gen_stmt(ctx, stmt->u.labeled.stmt);
        break;
    case STMT_CASE:
    case STMT_DEFAULT:
        fatal_error("case/default in switch not yet lowered to TAC");
    default:
        fatal_error("Unsupported statement kind %d in TAC lowering", (int)stmt->kind);
    }
}

static Tac_Param *params_from_type(const Type *fun_type)
{
    if (!fun_type || fun_type->kind != TYPE_FUNCTION) {
        return NULL;
    }
    Tac_Param *head = NULL;
    Tac_Param **tail = &head;
    for (const Param *p = fun_type->u.function.params; p; p = p->next) {
        Tac_Param *tp = new_tac_param();
        tp->name      = p->name ? xstrdup(p->name) : xstrdup("");
        *tail         = tp;
        tail          = &tp->next;
    }
    return head;
}

Tac_TopLevel *translate_external_decl(ExternalDecl *ast)
{
    if (!ast || ast->kind != EXTERNAL_DECL_FUNCTION) {
        return NULL;
    }
    if (!ast->u.function.body) {
        return NULL;
    }
    TacCtx ctx;
    ctx.head     = NULL;
    ctx.tail     = NULL;
    ctx.temp_id  = 0;

    gen_stmt(&ctx, ast->u.function.body);

    Tac_TopLevel *tl  = new_tac_toplevel(TAC_TOPLEVEL_FUNCTION);
    tl->u.function.name   = xstrdup(ast->u.function.name);
    tl->u.function.global = true;
    tl->u.function.params = params_from_type(ast->u.function.type);
    tl->u.function.body   = ctx.head;
    return tl;
}
