//
// Expression lowering: AST Expr → TAC instructions.
//

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic.h"
#include "structtab.h"
#include "translate.h"
#include "typecheck.h"
#include "xalloc.h"

// The declared type of the member named by a FIELD_ACCESS/PTR_ACCESS node, looked
// up in structtab.  After typecheck an array member used as a value has been
// decayed to a pointer (so e->type no longer says "array"); the backend recovers
// the member's true type here to decide whether to load it or decay it to its
// address.  Returns NULL if the tag is not in scope (e.g. a purged block-scope
// struct), in which case the caller falls back to a plain load.
static const Type *field_member_type(const Expr *e)
{
    const Expr *base;
    const char *field;
    if (e->kind == EXPR_FIELD_ACCESS) {
        base  = e->u.field_access.expr;
        field = e->u.field_access.field;
    } else {
        base  = e->u.ptr_access.expr;
        field = e->u.ptr_access.field;
    }
    const Type *st = unalias(base->type);
    if (e->kind == EXPR_PTR_ACCESS && st->kind == TYPE_POINTER)
        st = unalias(st->u.pointer.target);
    if (st->kind != TYPE_STRUCT && st->kind != TYPE_UNION)
        return NULL;
    const StructDef *def = structtab_find_opt(st->u.struct_t.name);
    if (!def)
        return NULL;
    for (const FieldDef *m = def->members; m; m = m->next)
        if (strcmp(m->name, field) == 0)
            return m->type;
    return NULL;
}

// True when a struct member is addressed by byte (a char scalar or a character
// array), so its address is a fat pointer built with ADD_PTR scale 1.  Every other
// member (a word scalar, pointer, struct/union, or word-element array) is addressed
// by word, so its member offset is added as a plain word offset — keeping the
// pointer a plain word address that later array indexing / loads can use.
static bool member_is_byte_addressed(const Type *mt)
{
    if (!mt)
        return false;
    mt = unalias(mt);
    while (mt->kind == TYPE_ARRAY)
        mt = unalias(mt->u.array.element);
    return is_character(mt);
}

// Fill in an ADD_PTR's index/scale for a struct member at `byte_offset` whose
// declared type is `mt` (NULL if the tag is out of scope at lowering time).  A
// word-addressed member at a word-aligned offset is added as a plain word offset
// (scale = word) so the result stays a plain word pointer that later subscripts
// and loads can chain off; a char member (or an unknown member) keeps the byte
// (scale 1, fat-pointer) form it has always used.
static void emit_member_offset(Tac_Instruction *ap, int byte_offset, const Type *mt)
{
    int w = target_word_bytes();
    if (mt && !member_is_byte_addressed(mt) && byte_offset % w == 0) {
        ap->u.add_ptr.index = val_int(byte_offset / w);
        ap->u.add_ptr.scale = w;
    } else {
        ap->u.add_ptr.index = val_int(byte_offset);
        ap->u.add_ptr.scale = 1;
    }
}

static bool is_unsigned_type(const Type *t)
{
    t = unalias(t);
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
// A pointer to a char-innermost array (e.g. char(*)[4], produced by decaying a
// multi-dimensional char array) is also a fat byte pointer over the array's contiguous
// byte storage, so look through array element types to the innermost scalar.
static bool is_byte_pointer(const Type *t)
{
    t = unalias(t);
    if (t->kind != TYPE_POINTER)
        return false;
    const Type *target = unalias(t->u.pointer.target);
    while (target->kind == TYPE_ARRAY)
        target = unalias(target->u.array.element);
    return is_character(target) || target->kind == TYPE_VOID;
}

static bool is_floating_type(const Type *t)
{
    return is_arithmetic(t) && !is_integer(t);
}

// For a word (non-byte) pointer whose pointee is larger than one machine word —
// a pointer-to-array or pointer-to-struct — return the element size in bytes
// (the ADD_PTR scale).  Returns 0 when plain one-word arithmetic suffices (a
// single-word pointee, or not such a pointer): the machine is word-addressed, so
// a one-word element already advances by exactly one word.
static int wide_ptr_scale(const Type *t)
{
    t = unalias(t);
    if (t->kind != TYPE_POINTER || is_byte_pointer(t))
        return 0;
    int sz = (int)get_size(t->u.pointer.target);
    return sz > target_word_bytes() ? sz : 0;
}

// Byte stride of a fat (byte) pointer — the size of its pointee: 1 for char*/void*, or the
// row size for a pointer to a char-innermost array (char(*)[N], from decaying a
// multi-dimensional char array).  BESM-6 has no sub-word word-scale, so fat-pointer
// arithmetic runs at scale 1 with the index pre-multiplied by this stride.
static int fat_ptr_byte_scale(const Type *ptr_type)
{
    const Type *target = unalias(unalias(ptr_type)->u.pointer.target);
    return target->kind == TYPE_VOID ? 1 : (int)get_size(target);
}

// Multiply a fat-pointer index by its byte stride (a no-op for stride 1).  A constant index
// folds away in the optimizer.
static Tac_Val *scale_byte_index(TacCtx *ctx, Tac_Val *idx, int scale)
{
    if (scale == 1)
        return idx;
    Tac_Val *scaled      = new_var_val(ctx);
    Tac_Instruction *mul = tac_new_instruction(TAC_INSTRUCTION_BINARY);
    mul->u.binary.op     = TAC_BINARY_MULTIPLY;
    mul->u.binary.src1   = idx;
    mul->u.binary.src2   = val_int(scale);
    mul->u.binary.dst    = scaled;
    tac_append(ctx, mul);
    return val_var(scaled->u.var_name);
}

// Emit dst = val / divisor (signed) for a positive constant divisor; converts a raw
// pointer difference into a C element count.  A divisor that is a power of two folds to
// a shift in the backend; otherwise it calls b/div.
static Tac_Val *gen_div_const(TacCtx *ctx, Tac_Val *val, int divisor)
{
    Tac_Val *vd          = new_var_val(ctx);
    Tac_Instruction *div = tac_new_instruction(TAC_INSTRUCTION_BINARY);
    div->u.binary.op     = TAC_BINARY_DIVIDE;
    div->u.binary.src1   = val;
    div->u.binary.src2   = val_int(divisor);
    div->u.binary.dst    = vd;
    tac_append(ctx, div);
    return val_var(vd->u.var_name);
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
        // Signed ~ flips the value bits but must preserve the INT-format exponent
        // (the result is still a canonical signed integer, so ~1 == -2).  Unsigned
        // ~ is the exact 48-bit complement of the plain-integer word.
        return is_unsigned_type(operand_type) ? TAC_UNARY_COMPLEMENT_UNSIGNED
                                              : TAC_UNARY_COMPLEMENT;
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
        // After typecheck exactly one operand is a (decayed) pointer; the other is
        // the integer index.  Scale by the size of the pointee: for a multi-
        // dimensional array the pointee is itself the row array, so this is the
        // row size, not the decayed element-pointer size.
        Expr *lexp          = e->u.subscript.left;
        Expr *rexp          = e->u.subscript.right;
        const Expr *ptr_exp = is_pointer(lexp->type) ? lexp : rexp;
        int scale           = (int)get_size(unalias(ptr_exp->type)->u.pointer.target);
        Tac_Val *lval       = gen_expr(ctx, lexp);
        Tac_Val *rval       = gen_expr(ctx, rexp);
        Tac_Val *idx        = ptr_exp == lexp ? rval : lval;
        // Indexing a row of a multi-dimensional char array: the base is a fat byte pointer
        // over contiguous byte storage, so advance it by index*rowsize BYTES at scale 1
        // rather than by a whole-word scale (BESM-6 has no sub-word word-scale).
        if (is_byte_pointer(ptr_exp->type)) {
            idx   = scale_byte_index(ctx, idx, scale);
            scale = 1;
        }
        Tac_Val *dst        = new_var_val(ctx);
        Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_ADD_PTR);
        in->u.add_ptr.ptr   = ptr_exp == lexp ? lval : rval;
        in->u.add_ptr.index = idx;
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
        emit_member_offset(ap, offset, field_member_type(e));
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
        emit_member_offset(ap, offset, field_member_type(e));
        ap->u.add_ptr.dst   = dst;
        tac_append(ctx, ap);
        return val_var(dst->u.var_name);
    }
    case EXPR_COMPOUND: {
        char *T              = new_temp(ctx);
        const Type *lit_type = unalias(e->u.compound_literal.type);
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
    Tac_Val *left  = gen_cond_val(ctx, l);
    char *false_l  = new_temp(ctx);
    char *end_l    = new_temp(ctx);
    char *dst_name = new_temp(ctx);

    Tac_Instruction *jz          = tac_new_instruction(TAC_INSTRUCTION_JUMP_IF_ZERO);
    jz->u.jump_if_zero.condition = left;
    jz->u.jump_if_zero.target    = false_l; // instruction takes ownership
    tac_append(ctx, jz);

    Tac_Val *right       = gen_cond_val(ctx, r);
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
    Tac_Val *left  = gen_cond_val(ctx, l);
    char *true_l   = new_temp(ctx);
    char *end_l    = new_temp(ctx);
    char *dst_name = new_temp(ctx);

    Tac_Instruction *jnz              = tac_new_instruction(TAC_INSTRUCTION_JUMP_IF_NOT_ZERO);
    jnz->u.jump_if_not_zero.condition = left;
    jnz->u.jump_if_not_zero.target    = true_l; // instruction takes ownership
    tac_append(ctx, jnz);

    Tac_Val *right       = gen_cond_val(ctx, r);
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
    // Logical NOT of a char*/void* is a null test; gen_cond_val reduces a fat pointer to
    // its word address so a marker-tagged null still reads as zero (no effect otherwise).
    Tac_Val *src = op == UNARY_LOG_NOT ? gen_cond_val(ctx, inner) : gen_expr(ctx, inner);
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

// Emit "dst = ptr (+/-) idx" as ADD_PTR with the given byte scale.  For a char*/void*
// fat pointer the scale is 1 (the delta adjusts the 3-bit offset); for a pointer-to-array
// it is the element size.  `vptr`/`vidx` are consumed; returns the result val.
static Tac_Val *gen_ptr_add(TacCtx *ctx, Tac_Val *vptr, Tac_Val *vidx, bool subtract, int scale)
{
    if (subtract) {
        // ptr - n : negate the index (ADD_PTR / b/padd take a signed count).
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
    ap->u.add_ptr.scale = scale;
    ap->u.add_ptr.dst   = vd;
    tac_append(ctx, ap);
    return val_var(vd->u.var_name);
}

// Peel EXPR_CAST wrappers and report whether the underlying expression is a null
// pointer constant (an integer literal 0).  `p == NULL` arrives as a CAST of the
// literal 0 to the pointer type, and a coerced `p == 0` likewise, so the bare
// is_null_pointer_constant check must see through the casts.
static bool is_null_ptr_operand(const Expr *e)
{
    while (e->kind == EXPR_CAST)
        e = e->u.cast.expr;
    return is_null_pointer_constant(e);
}

// Reduce a char*/void* fat pointer to its bare word address (marker bit 48 and the
// 3-bit byte offset cleared) for null-testing.  Reuses CHAR_PTR_TO_PTR, which the
// BESM-6 backend already lowers to an AAX address mask (and which is an identity copy
// on byte-addressed targets).  A fat pointer is null iff this address word is 0,
// regardless of its marker/offset, so every "== NULL" / "if(p)" test runs the pointer
// through this first.
static Tac_Val *gen_ptr_addr_word(TacCtx *ctx, Tac_Val *fat)
{
    Tac_Val *vd               = new_var_val(ctx);
    Tac_Instruction *in       = tac_new_instruction(TAC_INSTRUCTION_CHAR_PTR_TO_PTR);
    in->u.char_ptr_to_ptr.src = fat;
    in->u.char_ptr_to_ptr.dst = vd;
    tac_append(ctx, in);
    return val_var(vd->u.var_name);
}

// Evaluate a controlling expression (an if/while/for/do condition, or a &&/||/?:
// operand) to a value suitable for a zero test.  A char*/void* fat pointer is first
// reduced to its word address so a marker-tagged null still tests as zero.
Tac_Val *gen_cond_val(TacCtx *ctx, Expr *cond)
{
    Tac_Val *v = gen_expr(ctx, cond);
    if (is_byte_pointer(cond->type))
        v = gen_ptr_addr_word(ctx, v);
    return v;
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
            // char* - char* : the difference is a ptrdiff_t (long) element count.  Decode
            // both fat pointers to absolute byte positions and subtract (the runtime
            // helper b/pdiff), then divide by the pointee byte size to get the element
            // count.  sizeof(char) == 1, so plain char*/void* needs no divide; a pointer
            // to a char-innermost array (char(*)[N]) divides by the row size N.
            Tac_Val *vl         = gen_expr(ctx, l);
            Tac_Val *vr         = gen_expr(ctx, r);
            Tac_Val *vd         = new_var_val(ctx);
            Tac_Instruction *pd = tac_new_instruction(TAC_INSTRUCTION_PTR_DIFF);
            pd->u.ptr_diff.ptr_a = vl;
            pd->u.ptr_diff.ptr_b = vr;
            pd->u.ptr_diff.dst   = vd;
            tac_append(ctx, pd);
            int elem_bytes = fat_ptr_byte_scale(l->type);
            Tac_Val *diff  = val_var(vd->u.var_name);
            return elem_bytes > 1 ? gen_div_const(ctx, diff, elem_bytes) : diff;
        }
        bool ptr_left  = l_fat && is_integer(r->type);
        bool ptr_right = op == BINARY_ADD && r_fat && is_integer(l->type);
        if (ptr_left || ptr_right) {
            Tac_Val *vl   = gen_expr(ctx, l); // keep source evaluation order
            Tac_Val *vr   = gen_expr(ctx, r);
            Tac_Val *vptr = ptr_left ? vl : vr;
            Tac_Val *vidx = ptr_left ? vr : vl;
            // Scale the index to bytes by the pointee size (1 for char*/void*, the row size
            // for a pointer to a char-innermost array), then advance at scale 1.
            vidx = scale_byte_index(ctx, vidx, fat_ptr_byte_scale(ptr_left ? l->type : r->type));
            return gen_ptr_add(ctx, vptr, vidx, op == BINARY_SUB, 1);
        }
        // Word pointer minus word pointer, both pointing to a multi-word element
        // (pointer-to-array / -struct): the raw word-address difference must be divided
        // by the element word size to yield a C element count.  Detect before the ptr ±
        // int scale block below, which would otherwise misread q as a scaled index.
        if (op == BINARY_SUB && wide_ptr_scale(l->type) && wide_ptr_scale(r->type)) {
            Tac_Val *vl          = gen_expr(ctx, l);
            Tac_Val *vr          = gen_expr(ctx, r);
            Tac_Val *vd          = new_var_val(ctx);
            Tac_Instruction *sub = tac_new_instruction(TAC_INSTRUCTION_BINARY);
            sub->u.binary.op     = TAC_BINARY_SUBTRACT; // raw word-address difference
            sub->u.binary.src1   = vl;
            sub->u.binary.src2   = vr;
            sub->u.binary.dst    = vd;
            tac_append(ctx, sub);
            int elem_words = wide_ptr_scale(l->type) / target_word_bytes();
            return gen_div_const(ctx, val_var(vd->u.var_name), elem_words);
        }
        // Word pointer with a multi-word element (pointer-to-array / -struct):
        // ptr ± int must scale by the element size, not advance a single word.
        int l_scale = wide_ptr_scale(l->type);
        int r_scale = op == BINARY_ADD ? wide_ptr_scale(r->type) : 0;
        if (l_scale || r_scale) {
            Tac_Val *vl   = gen_expr(ctx, l); // keep source evaluation order
            Tac_Val *vr   = gen_expr(ctx, r);
            Tac_Val *vptr = l_scale ? vl : vr;
            Tac_Val *vidx = l_scale ? vr : vl;
            return gen_ptr_add(ctx, vptr, vidx, op == BINARY_SUB, l_scale ? l_scale : r_scale);
        }
    }

    // Relational comparison of two char*/void* fat pointers.  The fat-pointer encoding
    // carries the byte offset in the high (exponent) bits and the word address in the low
    // bits, so a raw-word ordering compare is wrong.  Decode both to absolute byte positions
    // and subtract (PTR_DIFF / b/pdiff), then compare the signed difference against 0:
    // a >= b  iff  bytepos(a) - bytepos(b) >= 0, and likewise for <, >, <=.  (==/!=
    // between two real pointers fall through to a full-word compare — identical positions
    // have identical words — but ==/!= against a null constant is handled just below.)
    if ((op == BINARY_LT || op == BINARY_GT || op == BINARY_LE || op == BINARY_GE) &&
        is_byte_pointer(l->type) && is_byte_pointer(r->type)) {
        Tac_Val *vl          = gen_expr(ctx, l);
        Tac_Val *vr          = gen_expr(ctx, r);
        Tac_Val *vdiff       = new_var_val(ctx);
        Tac_Instruction *pd  = tac_new_instruction(TAC_INSTRUCTION_PTR_DIFF);
        pd->u.ptr_diff.ptr_a = vl;
        pd->u.ptr_diff.ptr_b = vr;
        pd->u.ptr_diff.dst   = vdiff;
        tac_append(ctx, pd);

        Tac_BinaryOperator cmp;
        switch (op) {
        case BINARY_LT:
            cmp = TAC_BINARY_LESS_THAN;
            break;
        case BINARY_GT:
            cmp = TAC_BINARY_GREATER_THAN;
            break;
        case BINARY_LE:
            cmp = TAC_BINARY_LESS_OR_EQUAL;
            break;
        default: // BINARY_GE
            cmp = TAC_BINARY_GREATER_OR_EQUAL;
            break;
        }
        Tac_Val *vd         = new_var_val(ctx);
        Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_BINARY);
        in->u.binary.op     = cmp;
        in->u.binary.src1   = val_var(vdiff->u.var_name);
        in->u.binary.src2   = val_int(0);
        in->u.binary.dst    = vd;
        tac_append(ctx, in);
        return val_var(vd->u.var_name);
    }

    // Equality of a char*/void* fat pointer against a null constant (p == NULL, p != 0).
    // A null fat pointer may carry a marker/offset (e.g. a null word pointer cast to
    // char*), so a full-word compare would wrongly read it as non-null.  Reduce the
    // pointer to its word address and compare that against 0; a fat pointer is null iff
    // its address part is 0.  (Two real pointers fall through to the full-word compare,
    // which correctly distinguishes distinct byte positions.)
    if ((op == BINARY_EQ || op == BINARY_NE) && (is_byte_pointer(l->type) || is_byte_pointer(r->type)) &&
        (is_null_ptr_operand(l) || is_null_ptr_operand(r))) {
        Tac_Val *vl       = gen_expr(ctx, l); // keep source evaluation order
        Tac_Val *vr       = gen_expr(ctx, r);
        bool r_is_null    = is_null_ptr_operand(r);
        Tac_Val *ptr_addr = gen_ptr_addr_word(ctx, r_is_null ? vl : vr);
        tac_free_val(r_is_null ? vr : vl); // unused null-constant side
        Tac_Val *vd       = new_var_val(ctx);
        Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_BINARY);
        in->u.binary.op     = op == BINARY_EQ ? TAC_BINARY_EQUAL : TAC_BINARY_NOT_EQUAL;
        in->u.binary.src1   = ptr_addr;
        in->u.binary.src2   = val_int(0);
        in->u.binary.dst    = vd;
        tac_append(ctx, in);
        return val_var(vd->u.var_name);
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
    int wscale   = wide_ptr_scale(type);
    if (is_byte_pointer(type)) {
        // Fat pointer step: ±(pointee byte size) at scale 1 — ±1 for char*/void*, ±row
        // size for a pointer to a char-innermost array (char(*)[N]).
        int bscale          = fat_ptr_byte_scale(type);
        Tac_Instruction *ap = tac_new_instruction(TAC_INSTRUCTION_ADD_PTR);
        ap->u.add_ptr.ptr   = src;
        ap->u.add_ptr.index = val_int(inc ? bscale : -bscale);
        ap->u.add_ptr.scale = 1;
        ap->u.add_ptr.dst   = dst;
        tac_append(ctx, ap);
    } else if (wscale) {
        // Pointer-to-array/-struct (word pointer): a one-element step by the element size.
        Tac_Instruction *ap = tac_new_instruction(TAC_INSTRUCTION_ADD_PTR);
        ap->u.add_ptr.ptr   = src;
        ap->u.add_ptr.index = val_int(inc ? 1 : -1);
        ap->u.add_ptr.scale = wscale;
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
        if (unalias(e->type)->kind == TYPE_POINTER) {
            const Symbol *sym = symtab_get_opt(e->u.var);
            bool is_array     = (sym && unalias(sym->type)->kind == TYPE_ARRAY) ||
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
            // A function designator used as a value decays to a pointer-to-function
            // (C11 §6.3.2.1p4).  Its symbol has function type while a function-pointer
            // *variable*'s symbol has pointer type, so the symbol kind disambiguates.
            // Materialize the function's label address explicitly — the bare name would
            // otherwise make the backend load mem[name] (the first code word).
            if (sym && unalias(sym->type)->kind == TYPE_FUNCTION) {
                Tac_Val *dst          = new_var_val(ctx);
                Tac_Instruction *in   = tac_new_instruction(TAC_INSTRUCTION_GET_ADDRESS);
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
            Tac_Val *addr = gen_lval(ctx, e);
            // Dereferencing a pointer-to-array yields a sub-array whose value is
            // its own address (array-to-pointer decay), not a scalar to load.
            const Type *opnd = unalias(e->u.unary_op.expr->type);
            if (opnd->kind == TYPE_POINTER && unalias(opnd->u.pointer.target)->kind == TYPE_ARRAY)
                return addr;
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
            if (e->u.assign.op == ASSIGN_SIMPLE &&
                (unalias(target->type)->kind == TYPE_STRUCT ||
                 unalias(target->type)->kind == TYPE_UNION)) {
                // Whole-struct assignment (a = b;): copy every word.
                gen_struct_assign(ctx, dst, 0, src->u.var_name, (int)get_size(target->type));
                tac_free_val(src);
            } else if (e->u.assign.op == ASSIGN_SIMPLE) {
                Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_COPY);
                in->is_volatile     = vol;
                in->u.copy.src      = src;
                in->u.copy.dst      = val_var(dst);
                tac_append(ctx, in);
            } else if ((is_byte_pointer(target->type) || wide_ptr_scale(target->type)) &&
                       (e->u.assign.op == ASSIGN_ADD || e->u.assign.op == ASSIGN_SUB)) {
                // char* += n (fat-pointer byte arithmetic) or pointer-to-array/-struct
                // += n (element-scaled), not a raw word add.
                int pscale   = is_byte_pointer(target->type) ? 1 : wide_ptr_scale(target->type);
                Tac_Val *res = gen_ptr_add(ctx, val_var(dst), src,
                                           e->u.assign.op == ASSIGN_SUB, pscale);
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
                if ((is_byte_pointer(target->type) || wide_ptr_scale(target->type)) &&
                    (e->u.assign.op == ASSIGN_ADD || e->u.assign.op == ASSIGN_SUB)) {
                    // char* lvalue += n (fat-pointer byte arithmetic) or pointer-to-
                    // array/-struct += n (element-scaled).
                    int pscale = is_byte_pointer(target->type) ? 1 : wide_ptr_scale(target->type);
                    result     = gen_ptr_add(ctx, val_var(loaded->u.var_name), src,
                                             e->u.assign.op == ASSIGN_SUB, pscale);
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
        Tac_Val *cond_val = gen_cond_val(ctx, e->u.cond.condition);
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
            // A multi-word struct passed by value is marshalled as N consecutive
            // machine-word arguments (true by-value): read each word out of the struct
            // slot and append it as its own call argument.  The callee reserves N
            // contiguous param slots (see params_from_type), so the words line up.
            if (type_is_byval_sret(arg->type)) {
                Tac_Val *sv = gen_expr(ctx, arg);
                int w       = target_word_bytes();
                int nwords  = ((int)get_size(arg->type) + w - 1) / w;
                for (int i = 0; i < nwords; i++) {
                    Tac_Val *t = new_var_val(ctx);
                    Tac_Instruction *ld           = tac_new_instruction(TAC_INSTRUCTION_COPY_FROM_OFFSET);
                    ld->u.copy_from_offset.src    = xstrdup(sv->u.var_name);
                    ld->u.copy_from_offset.offset = i * w;
                    ld->u.copy_from_offset.dst    = t; // owned by the COPY_FROM_OFFSET
                    tac_append(ctx, ld);

                    *args_tail = val_var(t->u.var_name);
                    args_tail  = &(*args_tail)->next;
                }
                tac_free_val(sv);
                continue;
            }
            Tac_Val *av = gen_expr(ctx, arg);
            *args_tail  = av;
            args_tail   = &av->next;
        }

        // Multi-word struct return: allocate a result slot, pass its address as a hidden
        // first argument (sret ABI), and let the call write the struct into that slot.
        // The call expression's value is then the slot itself.
        char *sret_slot = NULL;
        if (type_is_byval_sret(e->type)) {
            char *slot = new_temp(ctx);
            Tac_Instruction *al        = tac_new_instruction(TAC_INSTRUCTION_ALLOCATE_LOCAL);
            al->u.allocate_local.name  = xstrdup(slot);
            al->u.allocate_local.size  = (int)get_size(e->type);
            al->u.allocate_local.alignment = (int)get_alignment(e->type);
            tac_append(ctx, al);

            Tac_Val *addr         = new_var_val(ctx);
            Tac_Instruction *ga   = tac_new_instruction(TAC_INSTRUCTION_GET_ADDRESS);
            ga->u.get_address.src = val_var(slot);
            ga->u.get_address.dst = addr; // owned by the GET_ADDRESS instruction
            tac_append(ctx, ga);

            // Prepend a *copy* of the address as argument #1 (the operand above is owned by
            // the GET_ADDRESS; the args list owns its own values).
            Tac_Val *arg0 = val_var(addr->u.var_name);
            arg0->next    = args_head;
            args_head     = arg0;
            sret_slot     = slot; // owns `slot`; freed below
        }

        Expr *func = e->u.call.func;
        const char *fun_name;
        Tac_Val *fn_ptr = NULL;
        if (func->kind == EXPR_VAR) {
            fun_name = func->u.var;
        } else {
            // Indirect call. C11 §6.3.2.1p4: dereferencing a function pointer yields a
            // function designator that immediately decays back to the same pointer, so
            // (*fp)(...) is equivalent to fp(...).  Strip the DEREF whenever its operand
            // has function-pointer type and call through the pointer value directly,
            // rather than emitting a LOAD from it.  (The DEREF node's own type is the
            // re-decayed pointer type, so the operand's type is the reliable signal.)
            Expr *callee = func;
            if (callee->kind == EXPR_UNARY_OP && callee->u.unary_op.op == UNARY_DEREF) {
                const Type *opnd = unalias(callee->u.unary_op.expr->type);
                if (opnd && opnd->kind == TYPE_POINTER &&
                    unalias(opnd->u.pointer.target)->kind == TYPE_FUNCTION)
                    callee = callee->u.unary_op.expr;
            }
            fn_ptr   = gen_expr(ctx, callee);
            fun_name = fn_ptr->u.var_name;
        }

        // With the sret ABI the struct result lives in the slot we allocated, not in a
        // scalar destination register, so the call has no scalar `dst`.
        Tac_Val *dst            = (unalias(e->type)->kind != TYPE_VOID && !sret_slot) ? new_var_val(ctx)
                                                                            : NULL;
        // A direct call to a _Noreturn function never returns, so emit the dedicated kind:
        // the backend tail-jumps to it and drops the dead post-call path.  (Indirect calls
        // through a function pointer stay a plain FUN_CALL — the callee is not known here.)
        Tac_InstructionKind call_kind = TAC_INSTRUCTION_FUN_CALL;
        if (func->kind == EXPR_VAR) {
            const Symbol *sym = symtab_get_opt(func->u.var);
            if (sym && sym->kind == SYM_FUNC && sym->u.func.noret)
                call_kind = TAC_INSTRUCTION_FUN_CALL_NORETURN;
        }
        Tac_Instruction *in     = tac_new_instruction(call_kind);
        in->u.fun_call.fun_name = xstrdup(fun_name);
        in->u.fun_call.args     = args_head;
        in->u.fun_call.dst      = dst;
        tac_append(ctx, in);

        if (fn_ptr)
            tac_free_val(fn_ptr);

        if (sret_slot) {
            Tac_Val *result = val_var(sret_slot);
            xfree(sret_slot);
            return result;
        }
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
        Tac_Val *addr = gen_lval(ctx, e);
        // If the subscript selects a sub-array of a multi-dimensional array, its
        // value is the address of that sub-array (array-to-pointer decay), not a
        // load of a scalar.
        const Expr *ptr_exp = is_pointer(e->u.subscript.left->type) ? e->u.subscript.left
                                                                    : e->u.subscript.right;
        if (unalias(unalias(ptr_exp->type)->u.pointer.target)->kind == TYPE_ARRAY)
            return addr;
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
        // An array-typed member is not loaded: it decays to the address of its
        // first element (e.g. `s.arr` / `x.b.inner_arr`), just like a subscript
        // selecting a sub-array.  The member's array type is recovered from
        // structtab because e->type was decayed to a pointer at typecheck.
        {
            const Type *mt = unalias(field_member_type(e));
            if (mt && mt->kind == TYPE_ARRAY)
                return gen_lval(ctx, e);
        }
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
        // An array-typed member decays to the address of its first element.
        {
            const Type *mt = unalias(field_member_type(e));
            if (mt && mt->kind == TYPE_ARRAY)
                return gen_lval(ctx, e);
        }
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
        const Type *lit_type = unalias(e->u.compound_literal.type);
        if (lit_type->kind == TYPE_ARRAY || lit_type->kind == TYPE_STRUCT) {
            return gen_lval(ctx, e);
        }
        return gen_expr(ctx, e->u.compound_literal.init->init->u.expr);
    }
    default:
        fatal_error("Unsupported expression kind %d in TAC lowering", (int)e->kind);
    }
}
