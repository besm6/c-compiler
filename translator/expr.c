//
// Expression lowering: AST Expr → TAC instructions.
//

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic.h"
#include "translate.h"
#include "xalloc.h"

static bool is_unsigned_type(const Type *t)
{
    return t->kind == TYPE_UCHAR || t->kind == TYPE_UINT || t->kind == TYPE_ULONG ||
           t->kind == TYPE_ULONG_LONG;
}

// 1 when an object/pointee occupies a single byte (char/schar/uchar/bool), so a
// load/store through it is a byte access and its address is a fat pointer.  Selects the
// byte variant of LOAD/STORE/GET_ADDRESS/COPY_*_OFFSET for the BESM-6 backend.
static int byte_access_for(const Type *t)
{
    return get_size(t) == 1;
}

// True for a char*/void* — a fat pointer whose arithmetic adjusts the 3-bit byte
// offset (scale 1) rather than the word address.  Mirrors is_fat_pointer in translate.c.
static bool is_byte_pointer(const Type *t)
{
    if (t->kind != TYPE_POINTER)
        return false;
    const Type *target = t->u.pointer.target;
    return is_character(target) || target->kind == TYPE_VOID;
}

static bool is_floating_type(const Type *t)
{
    return is_arithmetic(t) && !is_integer(t);
}

static Tac_BinaryOperator map_binary_op(BinaryOp op, const Type *operand_type)
{
    bool is_unsigned = is_unsigned_type(operand_type);
    bool is_float    = is_floating_type(operand_type);
    switch (op) {
    case BINARY_ADD:
        return is_float ? TAC_BINARY_ADD_DOUBLE
               : is_unsigned ? TAC_BINARY_ADD_UNSIGNED
                             : TAC_BINARY_ADD;
    case BINARY_SUB:
        return is_float ? TAC_BINARY_SUBTRACT_DOUBLE
               : is_unsigned ? TAC_BINARY_SUBTRACT_UNSIGNED
                             : TAC_BINARY_SUBTRACT;
    case BINARY_MUL:
        return is_float ? TAC_BINARY_MULTIPLY_DOUBLE
               : is_unsigned ? TAC_BINARY_MULTIPLY_UNSIGNED
                             : TAC_BINARY_MULTIPLY;
    case BINARY_DIV:
        return is_float ? TAC_BINARY_DIVIDE_DOUBLE
               : is_unsigned ? TAC_BINARY_DIVIDE_UNSIGNED
                             : TAC_BINARY_DIVIDE;
    case BINARY_MOD:
        return is_unsigned ? TAC_BINARY_REMAINDER_UNSIGNED : TAC_BINARY_REMAINDER;
    case BINARY_LT:
        return is_float ? TAC_BINARY_LESS_THAN_DOUBLE
               : is_unsigned ? TAC_BINARY_LESS_THAN_UNSIGNED
                             : TAC_BINARY_LESS_THAN;
    case BINARY_GT:
        return is_float ? TAC_BINARY_GREATER_THAN_DOUBLE
               : is_unsigned ? TAC_BINARY_GREATER_THAN_UNSIGNED
                             : TAC_BINARY_GREATER_THAN;
    case BINARY_LE:
        return is_float ? TAC_BINARY_LESS_OR_EQUAL_DOUBLE
               : is_unsigned ? TAC_BINARY_LESS_OR_EQUAL_UNSIGNED
                             : TAC_BINARY_LESS_OR_EQUAL;
    case BINARY_GE:
        return is_float ? TAC_BINARY_GREATER_OR_EQUAL_DOUBLE
               : is_unsigned ? TAC_BINARY_GREATER_OR_EQUAL_UNSIGNED
                             : TAC_BINARY_GREATER_OR_EQUAL;
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
        return is_unsigned ? TAC_BINARY_RIGHT_SHIFT_LOGICAL : TAC_BINARY_RIGHT_SHIFT;
    default:
        fatal_error("Unsupported binary operator in TAC lowering");
    }
}

static Tac_UnaryOperator map_unary_op(UnaryOp op, const Type *operand_type)
{
    switch (op) {
    case UNARY_BIT_NOT:
        return TAC_UNARY_COMPLEMENT;
    case UNARY_NEG:
        if (is_floating_type(operand_type))
            return TAC_UNARY_NEGATE_DOUBLE;
        if (is_unsigned_type(operand_type))
            return TAC_UNARY_NEGATE_UNSIGNED;
        return TAC_UNARY_NEGATE;
    case UNARY_LOG_NOT:
        return TAC_UNARY_NOT;
    default:
        fatal_error("Unsupported unary operator in TAC lowering");
    }
}

static Tac_BinaryOperator map_assign_op(AssignOp op, const Type *operand_type)
{
    bool is_unsigned = is_unsigned_type(operand_type);
    bool is_float    = is_floating_type(operand_type);
    switch (op) {
    case ASSIGN_ADD:
        return is_float ? TAC_BINARY_ADD_DOUBLE
               : is_unsigned ? TAC_BINARY_ADD_UNSIGNED
                             : TAC_BINARY_ADD;
    case ASSIGN_SUB:
        return is_float ? TAC_BINARY_SUBTRACT_DOUBLE
               : is_unsigned ? TAC_BINARY_SUBTRACT_UNSIGNED
                             : TAC_BINARY_SUBTRACT;
    case ASSIGN_MUL:
        return is_float ? TAC_BINARY_MULTIPLY_DOUBLE
               : is_unsigned ? TAC_BINARY_MULTIPLY_UNSIGNED
                             : TAC_BINARY_MULTIPLY;
    case ASSIGN_DIV:
        return is_float ? TAC_BINARY_DIVIDE_DOUBLE
               : is_unsigned ? TAC_BINARY_DIVIDE_UNSIGNED
                             : TAC_BINARY_DIVIDE;
    case ASSIGN_MOD:
        return is_unsigned ? TAC_BINARY_REMAINDER_UNSIGNED : TAC_BINARY_REMAINDER;
    case ASSIGN_LEFT:
        return TAC_BINARY_LEFT_SHIFT;
    case ASSIGN_RIGHT:
        return is_unsigned ? TAC_BINARY_RIGHT_SHIFT_LOGICAL : TAC_BINARY_RIGHT_SHIFT;
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

static Tac_Val *gen_lval(TacCtx *ctx, Expr *e)
{
    switch (e->kind) {
    case EXPR_VAR: {
        Tac_Val *dst        = new_var_val(ctx);
        Tac_Instruction *in = tac_new_instruction(
            byte_access_for(e->type) ? TAC_INSTRUCTION_GET_ADDRESS_BYTE
                                     : TAC_INSTRUCTION_GET_ADDRESS);
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
        Expr *base = e->u.field_access.expr;
        int offset = e->u.field_access.offset;
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
    case EXPR_PTR_ACCESS: {
        Expr *ptr_expr      = e->u.ptr_access.expr;
        Tac_Val *ptr_val    = gen_expr(ctx, ptr_expr);
        int offset          = e->u.ptr_access.offset;
        Tac_Val *dst        = new_var_val(ctx);
        Tac_Instruction *ap = tac_new_instruction(TAC_INSTRUCTION_ADD_PTR);
        ap->u.add_ptr.ptr   = ptr_val;
        ap->u.add_ptr.index = val_int(offset);
        ap->u.add_ptr.scale = 1;
        ap->u.add_ptr.dst   = dst;
        tac_append(ctx, ap);
        return val_var(dst->u.var_name);
    }
    case EXPR_COMPOUND: {
        char *T              = new_temp(ctx);
        const Type *lit_type = e->u.compound_literal.type;
        if (lit_type->kind == TYPE_ARRAY || lit_type->kind == TYPE_STRUCT) {
            Initializer wrap;
            memset(&wrap, 0, sizeof wrap);
            wrap.kind    = INITIALIZER_COMPOUND;
            wrap.u.items = e->u.compound_literal.init;
            wrap.type    = (Type *)lit_type;
            gen_compound_init(ctx, T, 0, &wrap);
        } else {
            gen_compound_init(ctx, T, 0, e->u.compound_literal.init->init);
        }
        Tac_Val *ptr          = new_var_val(ctx);
        Tac_Instruction *ga   = tac_new_instruction(TAC_INSTRUCTION_GET_ADDRESS);
        ga->u.get_address.src = val_var(T);
        ga->u.get_address.dst = ptr;
        tac_append(ctx, ga);
        xfree(T);
        return val_var(ptr->u.var_name);
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
    in->u.unary.op      = map_unary_op(op, inner->type);
    in->u.unary.src     = src;
    in->u.unary.dst     = vd;
    tac_append(ctx, in);
    // Return a fresh val so callers can store it in a second instruction
    // without aliasing vd (which is already owned by this instruction).
    return val_var(vd->u.var_name);
}

// Emit "dst = ptr (+/-) idx" on a char*/void* fat pointer as ADD_PTR scale 1 (the byte
// delta adjusts the 3-bit offset).  `vptr`/`vidx` are consumed; returns the result val.
static Tac_Val *gen_byte_ptr_add(TacCtx *ctx, Tac_Val *vptr, Tac_Val *vidx, bool subtract)
{
    if (subtract) {
        // char* - n : negate the byte delta (b/padd takes a signed count).
        Tac_Val *neg        = new_var_val(ctx);
        Tac_Instruction *un = tac_new_instruction(TAC_INSTRUCTION_UNARY);
        un->u.unary.op      = TAC_UNARY_NEGATE;
        un->u.unary.src     = vidx;
        un->u.unary.dst     = neg;
        tac_append(ctx, un);
        vidx = val_var(neg->u.var_name);
    }
    Tac_Val *vd         = new_var_val(ctx);
    Tac_Instruction *ap = tac_new_instruction(TAC_INSTRUCTION_ADD_PTR);
    ap->u.add_ptr.ptr   = vptr;
    ap->u.add_ptr.index = vidx;
    ap->u.add_ptr.scale = 1;
    ap->u.add_ptr.dst   = vd;
    tac_append(ctx, ap);
    return val_var(vd->u.var_name);
}

static Tac_Val *gen_binary(TacCtx *ctx, BinaryOp op, Expr *l, Expr *r)
{
    // char*/void* arithmetic: pointer ± integer adjusts the 3-bit byte offset of a fat
    // pointer, so lower it to ADD_PTR (scale 1) rather than a raw word add/subtract.
    // (Word pointers fall through to the plain BINARY path below — correct because the
    // machine is word-addressed and a 1-word element advances by one word.)
    if (op == BINARY_ADD || op == BINARY_SUB) {
        bool l_fat = is_byte_pointer(l->type);
        bool r_fat = is_byte_pointer(r->type);
        if (op == BINARY_SUB && l_fat && r_fat) {
            // char* - char* : the difference is a ptrdiff_t (long) byte count, not a
            // pointer.  Decode both fat pointers to absolute byte positions and subtract
            // (the runtime helper b/pdiff); sizeof(char) == 1, so no scaling is needed.
            Tac_Val *vl         = gen_expr(ctx, l);
            Tac_Val *vr         = gen_expr(ctx, r);
            Tac_Val *vd         = new_var_val(ctx);
            Tac_Instruction *pd = tac_new_instruction(TAC_INSTRUCTION_PTR_DIFF);
            pd->u.ptr_diff.ptr_a = vl;
            pd->u.ptr_diff.ptr_b = vr;
            pd->u.ptr_diff.dst   = vd;
            tac_append(ctx, pd);
            return val_var(vd->u.var_name);
        }
        bool ptr_left  = l_fat && is_integer(r->type);
        bool ptr_right = op == BINARY_ADD && r_fat && is_integer(l->type);
        if (ptr_left || ptr_right) {
            Tac_Val *vl   = gen_expr(ctx, l); // keep source evaluation order
            Tac_Val *vr   = gen_expr(ctx, r);
            Tac_Val *vptr = ptr_left ? vl : vr;
            Tac_Val *vidx = ptr_left ? vr : vl;
            return gen_byte_ptr_add(ctx, vptr, vidx, op == BINARY_SUB);
        }
    }

    Tac_Val *vl = gen_expr(ctx, l);
    Tac_Val *vr = gen_expr(ctx, r);
    Tac_Val *vd = new_var_val(ctx);

    Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_BINARY);
    in->u.binary.op     = map_binary_op(op, l->type);
    in->u.binary.src1   = vl;
    in->u.binary.src2   = vr;
    in->u.binary.dst    = vd;
    tac_append(ctx, in);
    // Return a fresh val so callers can store it in a second instruction
    // without aliasing vd (which is already owned by this instruction).
    return val_var(vd->u.var_name);
}

// Emit "dst = src (+/-) 1" for the inc/dec operators.  For a char*/void* the step adjusts
// the 3-bit byte offset of the fat pointer (ADD_PTR scale 1, byte index +1/-1); for any
// other scalar it is a plain BINARY add/subtract by 1 (word-addressed for word pointers).
// Returns the dst Val owned by the emitted instruction; callers re-wrap with val_var.
static Tac_Val *gen_step(TacCtx *ctx, const Type *type, Tac_Val *src, bool inc)
{
    Tac_Val *dst = new_var_val(ctx);
    if (is_byte_pointer(type)) {
        Tac_Instruction *ap = tac_new_instruction(TAC_INSTRUCTION_ADD_PTR);
        ap->u.add_ptr.ptr   = src;
        ap->u.add_ptr.index = val_int(inc ? 1 : -1);
        ap->u.add_ptr.scale = 1;
        ap->u.add_ptr.dst   = dst;
        tac_append(ctx, ap);
    } else {
        Tac_Instruction *bin = tac_new_instruction(TAC_INSTRUCTION_BINARY);
        bin->u.binary.op     = inc ? TAC_BINARY_ADD : TAC_BINARY_SUBTRACT;
        bin->u.binary.src1   = src;
        bin->u.binary.src2   = val_int(1);
        bin->u.binary.dst    = dst;
        tac_append(ctx, bin);
    }
    return dst;
}

Tac_Val *gen_expr(TacCtx *ctx, Expr *e)
{
    if (!e) {
        fatal_error("NULL expression in TAC lowering");
    }
    assert(e);
    switch (e->kind) {
    case EXPR_LITERAL:
        switch (e->u.literal->kind) {
        case LITERAL_INT:
            return val_int(e->u.literal->u.int_val);
        case LITERAL_LONG:
            return val_long(e->u.literal->u.long_val);
        case LITERAL_LONG_LONG:
            return val_long_long(e->u.literal->u.long_long_val);
        case LITERAL_UINT:
            return val_uint(e->u.literal->u.uint_val);
        case LITERAL_ULONG:
            return val_ulong(e->u.literal->u.ulong_val);
        case LITERAL_ULONG_LONG:
            return val_ulong_long(e->u.literal->u.ulong_long_val);
        case LITERAL_FLOAT:
            return val_float((float)e->u.literal->u.real_val);
        case LITERAL_DOUBLE:
            return val_double(e->u.literal->u.real_val);
        case LITERAL_LONG_DOUBLE:
            return val_long_double(e->u.literal->u.long_double_val);
        case LITERAL_CHAR:
            return val_int(e->u.literal->u.char_val);
        case LITERAL_STRING: {
            char *decoded_str = decode_c_string_literal(e->u.literal->u.string_val);
            const char *sname = symtab_add_string(decoded_str);
            xfree(decoded_str);
            Symbol *sym = symtab_get(sname);

            Tac_TopLevel *sc           = tac_new_toplevel(TAC_TOPLEVEL_STATIC_CONSTANT);
            sc->u.static_constant.name = xstrdup(sname);
            sc->u.static_constant.type = ast_type_to_tac_type(sym->type);
            sc->u.static_constant.init = sym->u.const_init;
            sym->u.const_init          = NULL; // transfer ownership to TAC node

            sc->next              = ctx->static_constants;
            ctx->static_constants = sc;

            Tac_Val *dst = new_var_val(ctx);
            // A string literal decays to a char* at its first byte (byte#0 = MSB):
            // a fat pointer at offset_enc 5.
            Tac_Instruction *in   = tac_new_instruction(TAC_INSTRUCTION_GET_ADDRESS_DECAY);
            in->u.get_address.src = val_var(sname);
            in->u.get_address.dst = dst;
            tac_append(ctx, in);

            xfree((char *)sname);
            return val_var(dst->u.var_name);
        }
        default:
            fatal_error("Unsupported literal in TAC lowering");
        }
    case EXPR_VAR:
        // An array used as a value decays to a pointer to its first element.  Materialize
        // the address explicitly so the backend never has to disambiguate an array name
        // (whose value is its label address) from a pointer name (whose value is stored):
        // the bare array name would otherwise load the array's first word, not its address.
        // After typecheck a decayed array has pointer type, while its symbol is an array.
        // A char/void array decays to a fat pointer at its first byte (byte#0 = MSB,
        // offset_enc 5) via GET_ADDRESS_DECAY; any other array uses a plain GET_ADDRESS.
        if (e->type->kind == TYPE_POINTER) {
            const Symbol *sym = symtab_get_opt(e->u.var);
            bool is_array     = (sym && sym->type->kind == TYPE_ARRAY) ||
                            tac_is_array_local(ctx, e->u.var);
            if (is_array) {
                Tac_Val *dst          = new_var_val(ctx);
                Tac_Instruction *in   = tac_new_instruction(
                    is_byte_pointer(e->type) ? TAC_INSTRUCTION_GET_ADDRESS_DECAY
                                             : TAC_INSTRUCTION_GET_ADDRESS);
                in->u.get_address.src = val_var(e->u.var);
                in->u.get_address.dst = dst;
                tac_append(ctx, in);
                return val_var(dst->u.var_name);
            }
        }
        // A read of a volatile scalar variable must re-read memory on every use.
        // Materialize it into a volatile COPY so the optimizer cannot fold or
        // propagate the value away. Aggregates are read via field access instead.
        if (type_is_volatile(e->type) && is_scalar(e->type)) {
            Tac_Val *dst        = new_var_val(ctx);
            Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_COPY);
            in->is_volatile     = true;
            in->u.copy.src      = val_var(e->u.var);
            in->u.copy.dst      = dst;
            tac_append(ctx, in);
            return val_var(dst->u.var_name);
        }
        return val_var(e->u.var);
    case EXPR_UNARY_OP:
        if (e->u.unary_op.op == UNARY_PLUS)
            return gen_expr(ctx, e->u.unary_op.expr);
        if (e->u.unary_op.op == UNARY_ADDRESS)
            return gen_lval(ctx, e->u.unary_op.expr);
        if (e->u.unary_op.op == UNARY_DEREF) {
            Tac_Val *addr       = gen_lval(ctx, e);
            Tac_Val *dst        = new_var_val(ctx);
            Tac_Instruction *in = tac_new_instruction(
                byte_access_for(e->type) ? TAC_INSTRUCTION_LOAD_BYTE : TAC_INSTRUCTION_LOAD);
            in->is_volatile    = type_is_volatile(e->type);
            in->u.load.src_ptr = addr;
            in->u.load.dst     = dst;
            tac_append(ctx, in);
            return val_var(dst->u.var_name);
        }
        if (e->u.unary_op.op == UNARY_PRE_INC || e->u.unary_op.op == UNARY_PRE_DEC) {
            Expr *inner = e->u.unary_op.expr;
            bool inc    = (e->u.unary_op.op == UNARY_PRE_INC);
            if (inner->kind == EXPR_VAR) {
                const char *var     = inner->u.var;
                const Tac_Val *vd         = gen_step(ctx, inner->type, val_var(var), inc);
                Tac_Instruction *cp = tac_new_instruction(TAC_INSTRUCTION_COPY);
                cp->is_volatile     = type_is_volatile(inner->type);
                cp->u.copy.src      = val_var(vd->u.var_name);
                cp->u.copy.dst      = val_var(var);
                tac_append(ctx, cp);
                return val_var(vd->u.var_name);
            } else {
                bool vol            = type_is_volatile(inner->type);
                Tac_Val *addr_raw   = gen_lval(ctx, inner);
                Tac_Val *loaded     = new_var_val(ctx);
                Tac_Instruction *ld = tac_new_instruction(
                    byte_access_for(inner->type) ? TAC_INSTRUCTION_LOAD_BYTE
                                                 : TAC_INSTRUCTION_LOAD);
                ld->is_volatile    = vol;
                ld->u.load.src_ptr = addr_raw;
                ld->u.load.dst     = loaded;
                tac_append(ctx, ld);
                const Tac_Val *result = gen_step(ctx, inner->type, val_var(loaded->u.var_name), inc);
                Tac_Instruction *st = tac_new_instruction(
                    byte_access_for(inner->type) ? TAC_INSTRUCTION_STORE_BYTE
                                                 : TAC_INSTRUCTION_STORE);
                st->is_volatile     = vol;
                st->u.store.src     = val_var(result->u.var_name);
                st->u.store.dst_ptr = val_var(addr_raw->u.var_name);
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
        Expr *target = e->u.assign.target;
        Tac_Val *src = gen_expr(ctx, e->u.assign.value);
        if (target->kind == EXPR_VAR) {
            const char *dst = target->u.var;
            bool vol        = type_is_volatile(target->type);
            if (e->u.assign.op == ASSIGN_SIMPLE) {
                Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_COPY);
                in->is_volatile     = vol;
                in->u.copy.src      = src;
                in->u.copy.dst      = val_var(dst);
                tac_append(ctx, in);
            } else if (is_byte_pointer(target->type) &&
                       (e->u.assign.op == ASSIGN_ADD || e->u.assign.op == ASSIGN_SUB)) {
                // char* += n / -= n : fat-pointer byte arithmetic, not a raw word add.
                Tac_Val *res = gen_byte_ptr_add(ctx, val_var(dst), src,
                                                e->u.assign.op == ASSIGN_SUB);
                Tac_Instruction *cp = tac_new_instruction(TAC_INSTRUCTION_COPY);
                cp->is_volatile     = vol;
                cp->u.copy.src      = res;
                cp->u.copy.dst      = val_var(dst);
                tac_append(ctx, cp);
                return val_var(dst);
            } else {
                Tac_Val *vd          = new_var_val(ctx);
                Tac_Instruction *bin = tac_new_instruction(TAC_INSTRUCTION_BINARY);
                bin->u.binary.op   = map_assign_op(e->u.assign.op, target->type);
                bin->u.binary.src1 = val_var(dst);
                bin->u.binary.src2 = src;
                bin->u.binary.dst  = vd;
                tac_append(ctx, bin);
                Tac_Instruction *cp = tac_new_instruction(TAC_INSTRUCTION_COPY);
                cp->is_volatile     = vol;
                cp->u.copy.src      = val_var(vd->u.var_name);
                cp->u.copy.dst      = val_var(dst);
                tac_append(ctx, cp);
            }
            return val_var(dst);
        } else if (target->kind == EXPR_FIELD_ACCESS &&
                   target->u.field_access.expr->kind == EXPR_VAR &&
                   e->u.assign.op == ASSIGN_SIMPLE) {
            const char *var_name        = target->u.field_access.expr->u.var;
            int offset          = target->u.field_access.offset;
            Tac_Instruction *in = tac_new_instruction(
                byte_access_for(target->type) ? TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET
                                              : TAC_INSTRUCTION_COPY_TO_OFFSET);
            in->is_volatile             = type_is_volatile(target->type) ||
                                          type_is_volatile(target->u.field_access.expr->type);
            in->u.copy_to_offset.src    = src;
            in->u.copy_to_offset.dst    = xstrdup(var_name);
            in->u.copy_to_offset.offset = offset;
            tac_append(ctx, in);
            return val_var(var_name);
        } else {
            bool vol          = type_is_volatile(target->type);
            Tac_Val *addr_raw = gen_lval(ctx, target);
            if (e->u.assign.op == ASSIGN_SIMPLE) {
                Tac_Instruction *st = tac_new_instruction(
                    byte_access_for(target->type) ? TAC_INSTRUCTION_STORE_BYTE
                                                  : TAC_INSTRUCTION_STORE);
                st->is_volatile     = vol;
                st->u.store.src     = src;
                st->u.store.dst_ptr = addr_raw;
                tac_append(ctx, st);
                return new_var_val(ctx);
            } else {
                Tac_Val *loaded     = new_var_val(ctx);
                Tac_Instruction *ld = tac_new_instruction(
                    byte_access_for(target->type) ? TAC_INSTRUCTION_LOAD_BYTE
                                                  : TAC_INSTRUCTION_LOAD);
                ld->is_volatile    = vol;
                ld->u.load.src_ptr = addr_raw;
                ld->u.load.dst     = loaded;
                tac_append(ctx, ld);
                Tac_Val *result;
                if (is_byte_pointer(target->type) &&
                    (e->u.assign.op == ASSIGN_ADD || e->u.assign.op == ASSIGN_SUB)) {
                    // char* lvalue += n / -= n : fat-pointer byte arithmetic.
                    result = gen_byte_ptr_add(ctx, val_var(loaded->u.var_name), src,
                                              e->u.assign.op == ASSIGN_SUB);
                } else {
                    Tac_Val *vd          = new_var_val(ctx);
                    Tac_Instruction *bin = tac_new_instruction(TAC_INSTRUCTION_BINARY);
                    bin->u.binary.op   = map_assign_op(e->u.assign.op, target->type);
                    bin->u.binary.src1 = val_var(loaded->u.var_name);
                    bin->u.binary.src2 = src;
                    bin->u.binary.dst  = vd;
                    tac_append(ctx, bin);
                    result = val_var(vd->u.var_name);
                }
                Tac_Instruction *st = tac_new_instruction(
                    byte_access_for(target->type) ? TAC_INSTRUCTION_STORE_BYTE
                                                  : TAC_INSTRUCTION_STORE);
                st->is_volatile     = vol;
                st->u.store.src     = result;
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
        Tac_Val *args_head  = NULL;
        Tac_Val **args_tail = &args_head;
        for (Expr *arg = e->u.call.args; arg; arg = arg->next) {
            Tac_Val *av = gen_expr(ctx, arg);
            *args_tail  = av;
            args_tail   = &av->next;
        }

        Expr *func = e->u.call.func;
        const char *fun_name;
        Tac_Val *fn_ptr = NULL;
        if (func->kind == EXPR_VAR) {
            fun_name = func->u.var;
        } else {
            // Indirect call. C11 §6.3.2.1p4: a function designator (*fp)
            // decays to a function pointer; strip the DEREF so we use the
            // pointer variable directly rather than emitting a LOAD from it.
            Expr *callee = func;
            if (callee->kind == EXPR_UNARY_OP && callee->u.unary_op.op == UNARY_DEREF &&
                callee->type->kind == TYPE_FUNCTION)
                callee = callee->u.unary_op.expr;
            fn_ptr   = gen_expr(ctx, callee);
            fun_name = fn_ptr->u.var_name;
        }

        Tac_Val *dst            = (e->type->kind != TYPE_VOID) ? new_var_val(ctx) : NULL;
        Tac_Instruction *in     = tac_new_instruction(TAC_INSTRUCTION_FUN_CALL);
        in->u.fun_call.fun_name = xstrdup(fun_name);
        in->u.fun_call.args     = args_head;
        in->u.fun_call.dst      = dst;
        tac_append(ctx, in);

        if (fn_ptr)
            tac_free_val(fn_ptr);

        return dst ? val_var(dst->u.var_name) : val_int(0);
    }
    case EXPR_POST_INC:
    case EXPR_POST_DEC: {
        Expr *inner = (e->kind == EXPR_POST_INC) ? e->u.post_inc : e->u.post_dec;
        bool inc    = (e->kind == EXPR_POST_INC);
        if (inner->kind == EXPR_VAR) {
            bool vol             = type_is_volatile(inner->type);
            const char *var      = inner->u.var;
            Tac_Val *old         = new_var_val(ctx);
            Tac_Instruction *cp1 = tac_new_instruction(TAC_INSTRUCTION_COPY);
            cp1->is_volatile     = vol;
            cp1->u.copy.src      = val_var(var);
            cp1->u.copy.dst      = old;
            tac_append(ctx, cp1);
            const Tac_Val *vd          = gen_step(ctx, inner->type, val_var(var), inc);
            Tac_Instruction *cp2 = tac_new_instruction(TAC_INSTRUCTION_COPY);
            cp2->is_volatile     = vol;
            cp2->u.copy.src      = val_var(vd->u.var_name);
            cp2->u.copy.dst      = val_var(var);
            tac_append(ctx, cp2);
            return val_var(old->u.var_name);
        } else {
            bool vol            = type_is_volatile(inner->type);
            Tac_Val *addr_raw   = gen_lval(ctx, inner);
            Tac_Val *old        = new_var_val(ctx);
            Tac_Instruction *ld = tac_new_instruction(
                byte_access_for(inner->type) ? TAC_INSTRUCTION_LOAD_BYTE : TAC_INSTRUCTION_LOAD);
            ld->is_volatile    = vol;
            ld->u.load.src_ptr = addr_raw;
            ld->u.load.dst     = old;
            tac_append(ctx, ld);
            const Tac_Val *result = gen_step(ctx, inner->type, val_var(old->u.var_name), inc);
            Tac_Instruction *st = tac_new_instruction(
                byte_access_for(inner->type) ? TAC_INSTRUCTION_STORE_BYTE : TAC_INSTRUCTION_STORE);
            st->is_volatile     = vol;
            st->u.store.src     = val_var(result->u.var_name);
            st->u.store.dst_ptr = val_var(addr_raw->u.var_name);
            tac_append(ctx, st);
            return val_var(old->u.var_name);
        }
    }
    case EXPR_SUBSCRIPT: {
        Tac_Val *addr       = gen_lval(ctx, e);
        Tac_Val *dst        = new_var_val(ctx);
        Tac_Instruction *in = tac_new_instruction(
            byte_access_for(e->type) ? TAC_INSTRUCTION_LOAD_BYTE : TAC_INSTRUCTION_LOAD);
        in->is_volatile    = type_is_volatile(e->type);
        in->u.load.src_ptr = addr;
        in->u.load.dst     = dst;
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
        int offset       = e->u.field_access.offset;
        if (base->kind == EXPR_VAR) {
            Tac_Val *dst        = new_var_val(ctx);
            Tac_Instruction *in = tac_new_instruction(
                byte_access_for(e->type) ? TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET
                                         : TAC_INSTRUCTION_COPY_FROM_OFFSET);
            in->is_volatile            = type_is_volatile(e->type) || type_is_volatile(base->type);
            in->u.copy_from_offset.src = xstrdup(base->u.var);
            in->u.copy_from_offset.offset = offset;
            in->u.copy_from_offset.dst    = dst;
            tac_append(ctx, in);
            return val_var(dst->u.var_name);
        } else {
            Tac_Val *addr       = gen_lval(ctx, e);
            Tac_Val *dst        = new_var_val(ctx);
            Tac_Instruction *in = tac_new_instruction(
                byte_access_for(e->type) ? TAC_INSTRUCTION_LOAD_BYTE : TAC_INSTRUCTION_LOAD);
            in->is_volatile    = type_is_volatile(e->type);
            in->u.load.src_ptr = addr;
            in->u.load.dst     = dst;
            tac_append(ctx, in);
            return val_var(dst->u.var_name);
        }
    }
    case EXPR_PTR_ACCESS: {
        Tac_Val *addr       = gen_lval(ctx, e);
        Tac_Val *dst        = new_var_val(ctx);
        Tac_Instruction *in = tac_new_instruction(
            byte_access_for(e->type) ? TAC_INSTRUCTION_LOAD_BYTE : TAC_INSTRUCTION_LOAD);
        in->is_volatile    = type_is_volatile(e->type);
        in->u.load.src_ptr = addr;
        in->u.load.dst     = dst;
        tac_append(ctx, in);
        return val_var(dst->u.var_name);
    }
    case EXPR_GENERIC: {
        GenericAssoc *match = e->u.generic.associations;
        Expr *match_expr =
            (match->kind == GENERIC_ASSOC_TYPE) ? match->u.type_assoc.expr : match->u.default_assoc;
        return gen_expr(ctx, match_expr);
    }
    case EXPR_COMPOUND: {
        const Type *lit_type = e->u.compound_literal.type;
        if (lit_type->kind == TYPE_ARRAY || lit_type->kind == TYPE_STRUCT) {
            return gen_lval(ctx, e);
        }
        return gen_expr(ctx, e->u.compound_literal.init->init->u.expr);
    }
    default:
        fatal_error("Unsupported expression kind %d in TAC lowering", (int)e->kind);
    }
}
