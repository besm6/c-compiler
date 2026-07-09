//
// Core type-checking utilities and entry points.
//
#include "typecheck.h"

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic.h"
#include "structtab.h"
#include "symtab.h"
#include "target.h"
#include "typetab.h"

// Enable debug output
int semantic_debug;

// Level of scope for nested compound operators.
int scope_level;

void scope_increment(void)
{
    scope_level++;
}

void scope_decrement(void)
{
    scope_level--;
    symtab_purge(scope_level);
    structtab_purge(scope_level);
    typetab_purge(scope_level);
}

int round_away_from_zero(int alignment, int size)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    if (size % alignment == 0) {
        return size;
    }

    if (size < 0) {
        return size - alignment - (size % alignment);
    } else {
        return size + alignment - (size % alignment);
    }
}

size_t get_array_size(const Type *t)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    t = unalias(t);
    if (t->kind != TYPE_ARRAY) {
        fatal_error("get_array_size: Array is expected");
    }
    if (!t->u.array.size) {
        return 0;
    }
    if (t->u.array.size->kind != EXPR_LITERAL) {
        fatal_error("get_array_size: Size is not a literal");
    }
    if (!t->u.array.size->u.literal) {
        fatal_error("get_array_size: No literal in size");
    }
    assert(t->u.array.size->u.literal);
    if (t->u.array.size->u.literal->kind != LITERAL_INT) {
        fatal_error("get_array_size: Non-integer size");
    }
    return t->u.array.size->u.literal->u.int_val;
}

void set_array_size(Type *t, size_t size)
{
    t->u.array.size                       = new_expression(EXPR_LITERAL);
    t->u.array.size->u.literal            = new_literal(LITERAL_INT);
    t->u.array.size->u.literal->u.int_val = size;
}

// Validate a type (recursive).
void validate_type(const Type *t)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
        print_type(stdout, t, 4);
    }
    if (!t)
        return;
    switch (t->kind) {
    case TYPE_ARRAY:
        if (!is_complete(t->u.array.element)) {
            fatal_error("Array of incomplete type");
        }
        if (unalias(t->u.array.element)->kind == TYPE_FUNCTION) {
            fatal_error("Cannot declare an array of functions");
        }
        validate_type(t->u.array.element);
        break;
    case TYPE_POINTER:
        validate_type(t->u.pointer.target);
        break;
    case TYPE_FUNCTION:
        if (t->u.function.return_type->kind == TYPE_FUNCTION) {
            fatal_error("Function cannot return a function");
        }
        if (t->u.function.return_type->kind == TYPE_ARRAY) {
            fatal_error("Function cannot return an array");
        }
        validate_type(t->u.function.return_type);
        for (const Param *p = t->u.function.params; p; p = p->next) {
            if (p->type->kind == TYPE_VOID && p->name) {
                fatal_error("Void parameter not allowed");
            }
            validate_type(p->type);
        }
        break;
    case TYPE_VOID:
    case TYPE_BOOL:
    case TYPE_CHAR:
    case TYPE_SCHAR:
    case TYPE_UCHAR:
    case TYPE_SHORT:
    case TYPE_USHORT:
    case TYPE_INT:
    case TYPE_UINT:
    case TYPE_LONG:
    case TYPE_ULONG:
    case TYPE_LONG_LONG:
    case TYPE_ULONG_LONG:
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
    case TYPE_LONG_DOUBLE:
    case TYPE_ENUM:
        break;
    case TYPE_STRUCT:
    case TYPE_UNION: {
        // A reference such as `union x foo;` must agree with the tag's declared keyword.
        check_tag_kind(t);
        // Cache the size/alignment on the AST node while the tag is still live in
        // structtab.  A block-local tag is purged on block exit, so the translator
        // (get_size/get_alignment) can no longer resolve it; the cache lets it fall
        // back without re-querying structtab.  Mirrors field_access.offset, which is
        // likewise resolved and stashed on the AST during typecheck.
        const StructDef *d = structtab_find_opt(t->u.struct_t.name);
        if (d && d->complete) {
            Type *mut                    = (Type *)t;
            mut->u.struct_t.cached_size  = (int)d->size;
            mut->u.struct_t.cached_align = (int)d->alignment;
        }
        break;
    }
    case TYPE_TYPEDEF_NAME:
        // Global typedef names survive resolve_typedef_names as references.
        if (!typetab_exists(t->u.typedef_name.name)) {
            fatal_error("Unknown typedef name '%s'", t->u.typedef_name.name);
        }
        validate_type(typetab_resolve(t->u.typedef_name.name));
        break;
    default:
        fatal_error("Unsupported type kind %d", t->kind);
    }
}

// Convert an expression to a target type.
Expr *convert_to_type(Expr *e, const Type *target_type)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    const Type *src = unalias(e->type);
    const Type *tgt = unalias(target_type);
    if (src->kind == tgt->kind &&
        (!is_pointer(src) || src->u.pointer.target->kind == tgt->u.pointer.target->kind))
        return e; // Avoid unnecessary casts

    Expr *cast        = new_expression(EXPR_CAST);
    cast->u.cast.type = clone_type(target_type, __func__, __FILE__, __LINE__);
    cast->u.cast.expr = e;
    cast->type        = clone_type(target_type, __func__, __FILE__, __LINE__);
    return cast;
}

Expr *convert_to_kind(Expr *e, TypeKind target_kind)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    if (e->type->kind == target_kind)
        return e; // Avoid unnecessary casts

    Expr *cast        = new_expression(EXPR_CAST);
    cast->u.cast.type = new_type(target_kind, __func__, __FILE__, __LINE__);
    cast->u.cast.expr = e;
    cast->type        = new_type(target_kind, __func__, __FILE__, __LINE__);
    return cast;
}

// Get common type for arithmetic operations.
const Type *get_common_type(const Type *t1, const Type *t2)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    static const Type int_type         = { .kind = TYPE_INT };
    static const Type double_type      = { .kind = TYPE_DOUBLE };
    static const Type float_type       = { .kind = TYPE_FLOAT };
    static const Type long_double_type = { .kind = TYPE_LONG_DOUBLE };
    t1                                 = unalias(t1);
    t2                                 = unalias(t2);
    if (is_character(t1))
        t1 = &int_type;
    if (is_character(t2))
        t2 = &int_type;
    if (t1->kind == TYPE_SHORT || t1->kind == TYPE_USHORT)
        t1 = &int_type;
    if (t2->kind == TYPE_SHORT || t2->kind == TYPE_USHORT)
        t2 = &int_type;
    if (t1->kind == t2->kind)
        return t1;
    if (t1->kind == TYPE_LONG_DOUBLE || t2->kind == TYPE_LONG_DOUBLE)
        return &long_double_type;
    if (t1->kind == TYPE_DOUBLE || t2->kind == TYPE_DOUBLE)
        return &double_type;
    if (t1->kind == TYPE_FLOAT || t2->kind == TYPE_FLOAT)
        return &float_type;
    if (get_size(t1) == get_size(t2))
        return is_signed(t1) ? t2 : t1;
    return get_size(t1) > get_size(t2) ? t1 : t2;
}

// Check if a constant is a zero integer.
bool is_zero_int(const Literal *c)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    switch (c->kind) {
    case LITERAL_INT:
        return c->u.int_val == 0;
    case LITERAL_LONG:
        return c->u.long_val == 0;
    case LITERAL_LONG_LONG:
        return c->u.long_long_val == 0;
    case LITERAL_UINT:
        return c->u.uint_val == 0;
    case LITERAL_ULONG:
        return c->u.ulong_val == 0;
    case LITERAL_ULONG_LONG:
        return c->u.ulong_long_val == 0;
    case LITERAL_CHAR:
        return c->u.char_val == 0;
    case LITERAL_ENUM:
        return false; // Conservative
    default:
        return false;
    }
}

// Check if an expression is a null pointer constant.
bool is_null_pointer_constant(const Expr *e)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    return e->kind == EXPR_LITERAL && is_zero_int(e->u.literal);
}

// Get common pointer type for pointer-involved binary operations.
Type *common_pointer_type(const Expr *e1, const Expr *e2)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    const Type *p1 = unalias(e1->type), *p2 = unalias(e2->type);
    if (p1->kind == p2->kind && is_pointer(p1) &&
        unalias(p1->u.pointer.target)->kind == unalias(p2->u.pointer.target)->kind) {
        // Pointers to struct/union are compatible only if the tags match.
        const Type *t1 = unalias(p1->u.pointer.target), *t2 = unalias(p2->u.pointer.target);
        if ((t1->kind == TYPE_STRUCT || t1->kind == TYPE_UNION) &&
            strcmp(t1->u.struct_t.name, t2->u.struct_t.name) != 0)
            fatal_error("Incompatible pointer types");
        return e1->type;
    }
    if (is_null_pointer_constant(e1))
        return e2->type;
    if (is_null_pointer_constant(e2))
        return e1->type;

    // void* is the common type with any other object pointer — but only when both
    // operands are actually pointers (a void* vs. a non-null integer is invalid).
    bool e1_void_ptr = p1->kind == TYPE_POINTER && unalias(p1->u.pointer.target)->kind == TYPE_VOID;
    bool e2_void_ptr = p2->kind == TYPE_POINTER && unalias(p2->u.pointer.target)->kind == TYPE_VOID;
    if (e1_void_ptr && p2->kind == TYPE_POINTER)
        return e1->type; // already void* — return it borrowed (no allocation)
    if (e2_void_ptr && p1->kind == TYPE_POINTER)
        return e2->type;
    fatal_error("Incompatible pointer types");
}

// Parser represents f(void) as a single unnamed TYPE_VOID param; treat as no params.
static const Param *params_for_compat(const Type *fn_type)
{
    const Param *params = fn_type->u.function.params;
    if (params && !params->next && unalias(params->type)->kind == TYPE_VOID && !params->name)
        return NULL;
    return params;
}

static bool compatible_params(const Param *a, const Param *b)
{
    while (a && b) {
        if (!compatible_type(a->type, b->type))
            return false;
        a = a->next;
        b = b->next;
    }
    return a == NULL && b == NULL;
}

// Return true if src may initialize or be assigned to target (target is the lhs type).
bool compatible_type(const Type *target, const Type *src)
{
    if (!target && !src)
        return true;
    if (!target || !src)
        return false;
    // Look through global typedef references (kept by resolve_typedef_names) so a
    // nested `MyInt *` compares equal to `int *`.
    target = unalias(target);
    src    = unalias(src);
    if (target->kind != src->kind)
        return false;
    switch (target->kind) {
    case TYPE_FUNCTION: {
        if (target->u.function.variadic != src->u.function.variadic)
            return false;
        if (!compatible_type(target->u.function.return_type, src->u.function.return_type))
            return false;
        const Param *tp = params_for_compat(target);
        const Param *sp = params_for_compat(src);
        if (!tp || !sp)
            return true; // old-style or f(void): no prototype to compare
        return compatible_params(tp, sp);
    }
    case TYPE_POINTER:
        return compatible_type(target->u.pointer.target, src->u.pointer.target);
    case TYPE_ARRAY: {
        if (!compatible_type(target->u.array.element, src->u.array.element))
            return false;
        // Arrays are incompatible when both dimensions are known and differ
        // (int[3] vs int[4]). An unknown (incomplete) dimension on either side
        // stays compatible — get_array_size() returns 0 for an incomplete array.
        size_t ts = get_array_size(target), ss = get_array_size(src);
        return !(ts && ss && ts != ss);
    }
    case TYPE_COMPLEX:
    case TYPE_IMAGINARY:
        return compatible_type(target->u.complex.base, src->u.complex.base);
    case TYPE_ATOMIC:
        return compatible_type(target->u.atomic.base, src->u.atomic.base);
    case TYPE_STRUCT:
    case TYPE_UNION:
        return strcmp(target->u.struct_t.name, src->u.struct_t.name) == 0;
    default:
        // Leaf types (scalars, enum, void, typedef name): the kind equality
        // checked above is sufficient. Type qualifiers (const/volatile/...) are
        // deliberately ignored: C assignment and argument compatibility operate
        // on the unqualified value type, so e.g. `char *` is compatible with
        // `const char *`. (compare_type is qualifier-strict and unsuitable here.)
        return true;
    }
}

// Convert an expression for assignment to target_type.
Expr *coerce_for_assignment(Expr *e, const Type *target_type)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    const Type *e_type = unalias(e->type);
    target_type        = unalias(target_type);
    if (e_type->kind == target_type->kind && compatible_type(target_type, e_type))
        return e;
    if (is_arithmetic(e_type) && is_arithmetic(target_type))
        return convert_to_type(e, target_type);
    if (is_null_pointer_constant(e) && is_pointer(target_type))
        return convert_to_type(e, target_type);
    if ((target_type->kind == TYPE_POINTER && target_type->u.pointer.target->kind == TYPE_VOID &&
         is_pointer(e_type)) ||
        (is_pointer(target_type) && e_type->kind == TYPE_POINTER &&
         e_type->u.pointer.target->kind == TYPE_VOID)) {
        return convert_to_type(e, target_type);
    }
    // C11 §6.7.6.3p7: array param adjusts to pointer; int* arg matches int[N] param
    if (e_type->kind == TYPE_POINTER && target_type->kind == TYPE_ARRAY &&
        e_type->u.pointer.target->kind == target_type->u.array.element->kind)
        return e;
    fatal_error("Cannot convert type for assignment");
}

//
// A folded arithmetic constant: either an integer or a real.  C's usual arithmetic
// conversions let an operator's operands disagree, and an operator's result kind need
// not match its operands' (comparing two reals yields an int; casting a real to int
// yields an int), so the folder carries the value's kind along with it rather than
// committing to one up front.  try_eval_const_int/try_eval_const_real project the result.
//
typedef struct {
    bool is_real;
    long i;   // valid when !is_real
    double d; // valid when  is_real
} ConstVal;

// The value of a folded constant as a real, whichever kind it holds.
static double const_as_real(const ConstVal *v)
{
    return v->is_real ? v->d : (double)v->i;
}

// Value width in bits of an integer type on the active target.  A signed type uses the
// target's <type>_bits (a BESM-6 signed int is 41-bit inside a 48-bit word); an unsigned
// type uses the full storage width, size*8.  Mirrors target_signed_bits/target_unsigned_bits
// in optimize/const_fold.c.  Returns 0 when no target is configured, and the caller then
// keeps the host's narrowing.
static int type_value_bits(const Type *t)
{
    if (!target_config)
        return 0;
    switch (unalias(t)->kind) {
    case TYPE_CHAR:
    case TYPE_SCHAR:
    case TYPE_UCHAR:
        return 8;
    case TYPE_SHORT:
        return target_config->short_bits;
    case TYPE_USHORT:
        return (int)target_config->short_size * 8;
    case TYPE_INT:
    case TYPE_ENUM:
        return target_config->int_bits;
    case TYPE_UINT:
        return (int)target_config->int_size * 8;
    case TYPE_LONG:
        return target_config->long_bits;
    case TYPE_ULONG:
        return (int)target_config->long_size * 8;
    case TYPE_LONG_LONG:
        return target_config->llong_bits;
    case TYPE_ULONG_LONG:
        return (int)target_config->llong_size * 8;
    default:
        return 0;
    }
}

// Convert a folded integer to an integer cast target: wrap it to that type's value width
// and signedness (C11 §6.3.1.3), so "(char)300" folds to 44 rather than staying 300.
// _Bool yields 0 or 1 (§6.3.1.2).
static long narrow_to_int_type(const Type *t, long v)
{
    if (unalias(t)->kind == TYPE_BOOL)
        return v != 0;

    int width = type_value_bits(t);
    if (width <= 0 || width >= 64)
        return v; // no target configured, or already the host's width

    uint64_t mask = ((uint64_t)1 << width) - 1;
    uint64_t bits = (uint64_t)v & mask;
    if (is_signed(t) && (bits >> (width - 1)) & 1)
        bits |= ~mask; // sign-extend from the target's sign bit
    return (long)bits;
}

// Fold a binary operator whose operands underwent the usual arithmetic conversions to a
// real type.  Arithmetic yields a real; the comparisons and the logical operators yield
// an int.  The remaining operators require integer operands and do not fold.
static bool fold_real_binop(BinaryOp op, double left, double right, ConstVal *out)
{
    out->is_real = false;
    switch (op) {
    case BINARY_MUL:
        out->is_real = true;
        out->d       = left * right;
        return true;
    case BINARY_DIV:
        // Division by zero is not a constant expression; reject rather than fold an
        // infinity into a static initializer.  Mirrors the integer guard below.
        if (right == 0.0)
            return false;
        out->is_real = true;
        out->d       = left / right;
        return true;
    case BINARY_ADD:
        out->is_real = true;
        out->d       = left + right;
        return true;
    case BINARY_SUB:
        out->is_real = true;
        out->d       = left - right;
        return true;
    case BINARY_LT:
        out->i = left < right;
        return true;
    case BINARY_GT:
        out->i = left > right;
        return true;
    case BINARY_LE:
        out->i = left <= right;
        return true;
    case BINARY_GE:
        out->i = left >= right;
        return true;
    case BINARY_EQ:
        out->i = left == right;
        return true;
    case BINARY_NE:
        out->i = left != right;
        return true;
    case BINARY_LOG_AND:
        out->i = (left != 0.0 && right != 0.0) ? 1L : 0L;
        return true;
    case BINARY_LOG_OR:
        out->i = (left != 0.0 || right != 0.0) ? 1L : 0L;
        return true;
    default:
        // BINARY_MOD, the shifts and the bitwise operators need integer operands.
        return false;
    }
}

// Evaluate a constant arithmetic expression; return false if not constant.
static bool eval_const(const Expr *e, ConstVal *out)
{
    switch (e->kind) {
    case EXPR_LITERAL:
        out->is_real = false;
        switch (e->u.literal->kind) {
        case LITERAL_INT:
            out->i = e->u.literal->u.int_val;
            return true;
        case LITERAL_LONG:
            out->i = e->u.literal->u.long_val;
            return true;
        case LITERAL_LONG_LONG:
            out->i = (long)e->u.literal->u.long_long_val;
            return true;
        case LITERAL_UINT:
            out->i = (long)e->u.literal->u.uint_val;
            return true;
        case LITERAL_ULONG:
            out->i = (long)e->u.literal->u.ulong_val;
            return true;
        case LITERAL_ULONG_LONG:
            out->i = (long)e->u.literal->u.ulong_long_val;
            return true;
        case LITERAL_CHAR:
            out->i = (unsigned char)e->u.literal->u.char_val;
            return true;
        case LITERAL_ENUM: {
            const Symbol *sym = symtab_get_opt(e->u.literal->u.enum_const);
            if (!sym)
                return false;
            out->i = sym->u.enum_val;
            return true;
        }
        case LITERAL_FLOAT:
        case LITERAL_DOUBLE:
        case LITERAL_LONG_DOUBLE:
            out->is_real = true;
            out->d       = literal_to_double(e->u.literal);
            return true;
        default:
            return false;
        }
    case EXPR_CAST: {
        ConstVal v;
        if (!eval_const(e->u.cast.expr, &v))
            return false;
        const Type *ct = unalias(e->u.cast.type);
        if (is_integer(ct)) {
            // C11 §6.3.1.4: a real converts to an integer by truncation toward zero.
            out->is_real = false;
            out->i       = narrow_to_int_type(ct, v.is_real ? (long)v.d : v.i);
            return true;
        }
        if (ct->kind == TYPE_FLOAT) {
            // A cast to float rounds to float precision, and the folded value keeps it.
            float rounded = (float)const_as_real(&v);
            out->is_real  = true;
            out->d        = rounded;
            return true;
        }
        if (ct->kind == TYPE_DOUBLE || ct->kind == TYPE_LONG_DOUBLE) {
            out->is_real = true;
            out->d       = const_as_real(&v);
            return true;
        }
        // A cast to a non-arithmetic type passes an integer operand through unchanged:
        // build_static_init folds an address constant such as "(char *)0x4000" this way
        // and then re-inspects the cast node itself.
        if (v.is_real)
            return false;
        *out = v;
        return true;
    }
    case EXPR_UNARY_OP: {
        ConstVal v;
        if (!eval_const(e->u.unary_op.expr, &v))
            return false;
        switch (e->u.unary_op.op) {
        case UNARY_NEG:
            *out = v;
            if (v.is_real)
                out->d = -v.d;
            else
                out->i = -v.i;
            return true;
        case UNARY_PLUS:
            *out = v;
            return true;
        case UNARY_BIT_NOT:
            if (v.is_real)
                return false; // ~ requires an integer operand
            out->is_real = false;
            out->i       = ~v.i;
            return true;
        case UNARY_LOG_NOT:
            // ! yields an int whatever the operand's type is (C11 §6.5.3.3p5).
            out->is_real = false;
            out->i       = (const_as_real(&v) == 0.0) ? 1L : 0L;
            return true;
        default:
            return false;
        }
    }
    case EXPR_BINARY_OP: {
        ConstVal l, r;
        if (!eval_const(e->u.binary_op.left, &l) || !eval_const(e->u.binary_op.right, &r))
            return false;
        if (l.is_real || r.is_real)
            return fold_real_binop(e->u.binary_op.op, const_as_real(&l), const_as_real(&r), out);

        long left    = l.i;
        long right   = r.i;
        out->is_real = false;
        switch (e->u.binary_op.op) {
        case BINARY_MUL:
            out->i = left * right;
            return true;
        case BINARY_DIV:
        case BINARY_MOD:
            if (right == 0)
                return false;
            out->i = (e->u.binary_op.op == BINARY_DIV) ? left / right : left % right;
            return true;
        case BINARY_ADD:
            out->i = left + right;
            return true;
        case BINARY_SUB:
            out->i = left - right;
            return true;
        case BINARY_LEFT_SHIFT:
            if (right < 0 || (unsigned long)right >= sizeof(long) * 8)
                return false;
            out->i = (long)((unsigned long)left << (unsigned long)right);
            return true;
        case BINARY_RIGHT_SHIFT:
            if (right < 0 || (unsigned long)right >= sizeof(long) * 8)
                return false;
            out->i = left >> right;
            return true;
        case BINARY_LT:
            out->i = left < right;
            return true;
        case BINARY_GT:
            out->i = left > right;
            return true;
        case BINARY_LE:
            out->i = left <= right;
            return true;
        case BINARY_GE:
            out->i = left >= right;
            return true;
        case BINARY_EQ:
            out->i = left == right;
            return true;
        case BINARY_NE:
            out->i = left != right;
            return true;
        case BINARY_BIT_AND:
            out->i = left & right;
            return true;
        case BINARY_BIT_XOR:
            out->i = left ^ right;
            return true;
        case BINARY_BIT_OR:
            out->i = left | right;
            return true;
        case BINARY_LOG_AND:
            out->i = (left != 0 && right != 0) ? 1L : 0L;
            return true;
        case BINARY_LOG_OR:
            out->i = (left != 0 || right != 0) ? 1L : 0L;
            return true;
        default:
            return false;
        }
    }
    case EXPR_SIZEOF_TYPE:
        out->is_real = false;
        out->i       = (long)get_size(e->u.sizeof_type);
        return true;
    case EXPR_SIZEOF_EXPR:
        if (e->u.sizeof_expr->type) {
            out->is_real = false;
            out->i       = (long)get_size(e->u.sizeof_expr->type);
            return true;
        }
        return false;
    case EXPR_ALIGNOF:
        out->is_real = false;
        out->i       = (long)get_alignment(e->u.align_of);
        return true;
    default:
        return false;
    }
}

// Evaluate a constant integer expression; return false if not constant.  A folded real
// value is not an integer constant expression (C11 §6.6p6) — "int a[1.5];" stays an
// error; only an explicit cast, "(int)1.5", makes it integral.
bool try_eval_const_int(const Expr *e, long *out)
{
    ConstVal v;
    if (!eval_const(e, &v) || v.is_real)
        return false;
    *out = v.i;
    return true;
}

// Evaluate a constant arithmetic expression as a real; an integral result converts.
bool try_eval_const_real(const Expr *e, double *out)
{
    ConstVal v;
    if (!eval_const(e, &v))
        return false;
    *out = const_as_real(&v);
    return true;
}

// Type-check a global declaration and label its loops.  Loop labels draw from the
// caller-owned unit-wide counter *seq (shared with the translator's temporaries).
void typecheck_decl(ExternalDecl *d, int *seq)
{
    typecheck_global_decl(d);
    label_loops(d, seq);
    resolve_labels(d);
}

// Type-check an entire program.
void typecheck_program(const Program *p)
{
    int seq = 0;
    for (ExternalDecl *d = p->decls; d; d = d->next) {
        typecheck_decl(d, &seq);
    }
}
