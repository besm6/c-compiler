//
// Expression lowering: AST Expr → TAC instructions.
//

#include "translate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "structtab.h"
#include "xalloc.h"

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


static int find_field_offset(const Type *struct_type, const char *field_name)
{
    const StructDef *def = structtab_find(struct_type->u.struct_t.name);
    for (const FieldDef *m = def->members; m; m = m->next) {
        if (strcmp(m->name, field_name) == 0)
            return m->offset;
    }
    fatal_error("Field %s not found in struct %s", field_name, struct_type->u.struct_t.name);
}

static Tac_Val *gen_lval(TacCtx *ctx, Expr *e)
{
    switch (e->kind) {
    case EXPR_VAR: {
        Tac_Val *dst          = new_var_val(ctx);
        Tac_Instruction *in   = tac_new_instruction(TAC_INSTRUCTION_GET_ADDRESS);
        in->u.get_address.src = val_var(e->u.var);
        in->u.get_address.dst = dst;
        tac_append(ctx, in);
        return val_var(dst->u.var_name);
    }
    case EXPR_UNARY_OP:
        if (e->u.unary_op.op == UNARY_DEREF)
            return gen_expr(ctx, e->u.unary_op.expr);
        fatal_error("lvalue not yet supported in gen_lval: expression kind %d", (int)e->kind);
    case EXPR_SUBSCRIPT: {
        Tac_Val *ptr_val    = gen_expr(ctx, e->u.subscript.left);
        Tac_Val *idx_val    = gen_expr(ctx, e->u.subscript.right);
        int scale           = (int)get_size(e->type);
        Tac_Val *dst        = new_var_val(ctx);
        Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_ADD_PTR);
        in->u.add_ptr.ptr   = ptr_val;
        in->u.add_ptr.index = idx_val;
        in->u.add_ptr.scale = scale;
        in->u.add_ptr.dst   = dst;
        tac_append(ctx, in);
        return val_var(dst->u.var_name);
    }
    case EXPR_FIELD_ACCESS: {
        Expr *base    = e->u.field_access.expr;
        int offset    = find_field_offset(base->type, e->u.field_access.field);
        Tac_Val *base_addr;
        if (base->kind == EXPR_VAR) {
            Tac_Val *tmp          = new_var_val(ctx);
            Tac_Instruction *ga   = tac_new_instruction(TAC_INSTRUCTION_GET_ADDRESS);
            ga->u.get_address.src = val_var(base->u.var);
            ga->u.get_address.dst = tmp;
            tac_append(ctx, ga);
            base_addr = val_var(tmp->u.var_name);
        } else {
            base_addr = gen_lval(ctx, base);
        }
        Tac_Val *dst        = new_var_val(ctx);
        Tac_Instruction *ap = tac_new_instruction(TAC_INSTRUCTION_ADD_PTR);
        ap->u.add_ptr.ptr   = base_addr;
        ap->u.add_ptr.index = val_int(offset);
        ap->u.add_ptr.scale = 1;
        ap->u.add_ptr.dst   = dst;
        tac_append(ctx, ap);
        return val_var(dst->u.var_name);
    }
    default:
        fatal_error("lvalue not yet supported in gen_lval: expression kind %d", (int)e->kind);
    }
}

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

Tac_Val *gen_expr(TacCtx *ctx, Expr *e)
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
        if (e->u.unary_op.op == UNARY_ADDRESS)
            return gen_lval(ctx, e->u.unary_op.expr);
        if (e->u.unary_op.op == UNARY_DEREF) {
            Tac_Val *addr       = gen_lval(ctx, e);
            Tac_Val *dst        = new_var_val(ctx);
            Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_LOAD);
            in->u.load.src_ptr  = addr;
            in->u.load.dst      = dst;
            tac_append(ctx, in);
            return val_var(dst->u.var_name);
        }
        if (e->u.unary_op.op == UNARY_PRE_INC || e->u.unary_op.op == UNARY_PRE_DEC) {
            Expr *inner = e->u.unary_op.expr;
            Tac_BinaryOperator inc_op =
                (e->u.unary_op.op == UNARY_PRE_INC) ? TAC_BINARY_ADD : TAC_BINARY_SUBTRACT;
            if (inner->kind == EXPR_VAR) {
                const char *var      = inner->u.var;
                Tac_Val *vd          = new_var_val(ctx);
                Tac_Instruction *bin = tac_new_instruction(TAC_INSTRUCTION_BINARY);
                bin->u.binary.op     = inc_op;
                bin->u.binary.src1   = val_var(var);
                bin->u.binary.src2   = val_int(1);
                bin->u.binary.dst    = vd;
                tac_append(ctx, bin);
                Tac_Instruction *cp = tac_new_instruction(TAC_INSTRUCTION_COPY);
                cp->u.copy.src      = val_var(vd->u.var_name);
                cp->u.copy.dst      = val_var(var);
                tac_append(ctx, cp);
                return val_var(vd->u.var_name);
            } else {
                Tac_Val *addr_raw        = gen_lval(ctx, inner);
                Tac_Val *loaded          = new_var_val(ctx);
                Tac_Instruction *ld      = tac_new_instruction(TAC_INSTRUCTION_LOAD);
                ld->u.load.src_ptr       = addr_raw;
                ld->u.load.dst           = loaded;
                tac_append(ctx, ld);
                Tac_Val *result          = new_var_val(ctx);
                Tac_Instruction *bin     = tac_new_instruction(TAC_INSTRUCTION_BINARY);
                bin->u.binary.op         = inc_op;
                bin->u.binary.src1       = val_var(loaded->u.var_name);
                bin->u.binary.src2       = val_int(1);
                bin->u.binary.dst        = result;
                tac_append(ctx, bin);
                Tac_Instruction *st      = tac_new_instruction(TAC_INSTRUCTION_STORE);
                st->u.store.src          = val_var(result->u.var_name);
                st->u.store.dst_ptr      = val_var(addr_raw->u.var_name);
                tac_append(ctx, st);
                return val_var(result->u.var_name);
            }
        }
        return gen_unary(ctx, e->u.unary_op.op, e->u.unary_op.expr);
    case EXPR_BINARY_OP:
        if (e->u.binary_op.op == BINARY_LOG_AND)
            return gen_logical_and(ctx, e->u.binary_op.left, e->u.binary_op.right);
        if (e->u.binary_op.op == BINARY_LOG_OR)
            return gen_logical_or(ctx, e->u.binary_op.left, e->u.binary_op.right);
        return gen_binary(ctx, e->u.binary_op.op, e->u.binary_op.left, e->u.binary_op.right);
    case EXPR_ASSIGN: {
        Expr    *target = e->u.assign.target;
        Tac_Val *src    = gen_expr(ctx, e->u.assign.value);
        if (target->kind == EXPR_VAR) {
            const char *dst = target->u.var;
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
        } else if (target->kind == EXPR_FIELD_ACCESS &&
                   target->u.field_access.expr->kind == EXPR_VAR &&
                   e->u.assign.op == ASSIGN_SIMPLE) {
            const char *var_name = target->u.field_access.expr->u.var;
            int offset           = find_field_offset(target->u.field_access.expr->type,
                                                     target->u.field_access.field);
            Tac_Instruction *in         = tac_new_instruction(TAC_INSTRUCTION_COPY_TO_OFFSET);
            in->u.copy_to_offset.src    = src;
            in->u.copy_to_offset.dst    = xstrdup(var_name);
            in->u.copy_to_offset.offset = offset;
            tac_append(ctx, in);
            return val_var(var_name);
        } else {
            Tac_Val *addr_raw = gen_lval(ctx, target);
            if (e->u.assign.op == ASSIGN_SIMPLE) {
                Tac_Instruction *st = tac_new_instruction(TAC_INSTRUCTION_STORE);
                st->u.store.src     = src;
                st->u.store.dst_ptr = addr_raw;
                tac_append(ctx, st);
                return new_var_val(ctx);
            } else {
                Tac_Val *loaded     = new_var_val(ctx);
                Tac_Instruction *ld = tac_new_instruction(TAC_INSTRUCTION_LOAD);
                ld->u.load.src_ptr  = addr_raw;
                ld->u.load.dst      = loaded;
                tac_append(ctx, ld);
                Tac_Val *result      = new_var_val(ctx);
                Tac_Instruction *bin = tac_new_instruction(TAC_INSTRUCTION_BINARY);
                bin->u.binary.op     = map_assign_op(e->u.assign.op);
                bin->u.binary.src1   = val_var(loaded->u.var_name);
                bin->u.binary.src2   = src;
                bin->u.binary.dst    = result;
                tac_append(ctx, bin);
                Tac_Instruction *st = tac_new_instruction(TAC_INSTRUCTION_STORE);
                st->u.store.src     = val_var(result->u.var_name);
                st->u.store.dst_ptr = val_var(addr_raw->u.var_name);
                tac_append(ctx, st);
                return val_var(result->u.var_name);
            }
        }
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
        Expr *inner = (e->kind == EXPR_POST_INC) ? e->u.post_inc : e->u.post_dec;
        Tac_BinaryOperator inc_op =
            (e->kind == EXPR_POST_INC) ? TAC_BINARY_ADD : TAC_BINARY_SUBTRACT;
        if (inner->kind == EXPR_VAR) {
            const char *var      = inner->u.var;
            Tac_Val *old         = new_var_val(ctx);
            Tac_Instruction *cp1 = tac_new_instruction(TAC_INSTRUCTION_COPY);
            cp1->u.copy.src      = val_var(var);
            cp1->u.copy.dst      = old;
            tac_append(ctx, cp1);
            Tac_Val *vd          = new_var_val(ctx);
            Tac_Instruction *bin = tac_new_instruction(TAC_INSTRUCTION_BINARY);
            bin->u.binary.op     = inc_op;
            bin->u.binary.src1   = val_var(var);
            bin->u.binary.src2   = val_int(1);
            bin->u.binary.dst    = vd;
            tac_append(ctx, bin);
            Tac_Instruction *cp2 = tac_new_instruction(TAC_INSTRUCTION_COPY);
            cp2->u.copy.src      = val_var(vd->u.var_name);
            cp2->u.copy.dst      = val_var(var);
            tac_append(ctx, cp2);
            return val_var(old->u.var_name);
        } else {
            Tac_Val *addr_raw        = gen_lval(ctx, inner);
            Tac_Val *old             = new_var_val(ctx);
            Tac_Instruction *ld      = tac_new_instruction(TAC_INSTRUCTION_LOAD);
            ld->u.load.src_ptr       = addr_raw;
            ld->u.load.dst           = old;
            tac_append(ctx, ld);
            Tac_Val *result          = new_var_val(ctx);
            Tac_Instruction *bin     = tac_new_instruction(TAC_INSTRUCTION_BINARY);
            bin->u.binary.op         = inc_op;
            bin->u.binary.src1       = val_var(old->u.var_name);
            bin->u.binary.src2       = val_int(1);
            bin->u.binary.dst        = result;
            tac_append(ctx, bin);
            Tac_Instruction *st      = tac_new_instruction(TAC_INSTRUCTION_STORE);
            st->u.store.src          = val_var(result->u.var_name);
            st->u.store.dst_ptr      = val_var(addr_raw->u.var_name);
            tac_append(ctx, st);
            return val_var(old->u.var_name);
        }
    }
    case EXPR_SUBSCRIPT: {
        Tac_Val *addr       = gen_lval(ctx, e);
        Tac_Val *dst        = new_var_val(ctx);
        Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_LOAD);
        in->u.load.src_ptr  = addr;
        in->u.load.dst      = dst;
        tac_append(ctx, in);
        return val_var(dst->u.var_name);
    }
    case EXPR_SIZEOF_EXPR:
        return val_int(get_size(e->u.sizeof_expr->type));
    case EXPR_SIZEOF_TYPE:
        return val_int(get_size(e->u.sizeof_type));
    case EXPR_ALIGNOF:
        return val_int(get_alignment(e->u.align_of));
    case EXPR_FIELD_ACCESS: {
        const Expr *base = e->u.field_access.expr;
        int offset       = find_field_offset(base->type, e->u.field_access.field);
        if (base->kind == EXPR_VAR) {
            Tac_Val *dst                   = new_var_val(ctx);
            Tac_Instruction *in            = tac_new_instruction(TAC_INSTRUCTION_COPY_FROM_OFFSET);
            in->u.copy_from_offset.src     = xstrdup(base->u.var);
            in->u.copy_from_offset.offset  = offset;
            in->u.copy_from_offset.dst     = dst;
            tac_append(ctx, in);
            return val_var(dst->u.var_name);
        } else {
            Tac_Val *addr       = gen_lval(ctx, e);
            Tac_Val *dst        = new_var_val(ctx);
            Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_LOAD);
            in->u.load.src_ptr  = addr;
            in->u.load.dst      = dst;
            tac_append(ctx, in);
            return val_var(dst->u.var_name);
        }
    }
    default:
        fatal_error("Unsupported expression kind %d in TAC lowering", (int)e->kind);
    }
}
