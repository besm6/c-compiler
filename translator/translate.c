//
// AST to TAC lowering: shared helpers, type conversion, and top-level entry points.
//

#include "translate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "string_map.h"
#include "structtab.h"
#include "xalloc.h"

// Enable debug output
int translator_debug;

//
// Low-level TAC-building helpers
//

void tac_append(TacCtx *ctx, Tac_Instruction *instr)
{
    if (!ctx->head) {
        ctx->head = ctx->tail = instr;
    } else {
        ctx->tail->next = instr;
        ctx->tail       = instr;
    }
    instr->next = NULL;
}

char *new_temp(TacCtx *ctx)
{
    return xstruniq("%", &ctx->temp_id);
}

// Record an automatic local variable name on the function being lowered. The
// optimizer reads this list to distinguish private locals (whose dead stores
// may be removed) from observable globals (whose stores must be preserved).
void tac_record_local(TacCtx *ctx, const char *name)
{
    Tac_Param *p = tac_new_param();
    p->name      = xstrdup(name);
    p->next      = NULL;
    if (!ctx->locals)
        ctx->locals = ctx->locals_tail = p;
    else {
        ctx->locals_tail->next = p;
        ctx->locals_tail       = p;
    }
}

// Record a local char/void array name so a later value use (decay) can be lowered to a
// fat-pointer GET_ADDRESS.  Local symbols are purged from the symbol table before lowering,
// so the translator tracks byte arrays itself (globals/strings stay queryable via symtab).
void tac_record_byte_array_local(TacCtx *ctx, const char *name)
{
    Tac_Param *p = tac_new_param();
    p->name      = xstrdup(name);
    p->next      = ctx->byte_array_locals;
    ctx->byte_array_locals = p;
}

bool tac_is_byte_array_local(const TacCtx *ctx, const char *name)
{
    for (const Tac_Param *p = ctx->byte_array_locals; p; p = p->next)
        if (strcmp(p->name, name) == 0)
            return true;
    return false;
}

Tac_Val *val_int(int v)
{
    Tac_Val *tv    = tac_new_val(TAC_VAL_CONSTANT);
    Tac_Const *c   = tac_new_const(TAC_CONST_INT);
    c->u.int_val   = v;
    tv->u.constant = c;
    return tv;
}

Tac_Val *val_long(long v)
{
    Tac_Val *tv    = tac_new_val(TAC_VAL_CONSTANT);
    Tac_Const *c   = tac_new_const(TAC_CONST_LONG);
    c->u.long_val  = v;
    tv->u.constant = c;
    return tv;
}

Tac_Val *val_long_long(long long v)
{
    Tac_Val *tv        = tac_new_val(TAC_VAL_CONSTANT);
    Tac_Const *c       = tac_new_const(TAC_CONST_LONG_LONG);
    c->u.long_long_val = v;
    tv->u.constant     = c;
    return tv;
}

Tac_Val *val_uint(unsigned int v)
{
    Tac_Val *tv    = tac_new_val(TAC_VAL_CONSTANT);
    Tac_Const *c   = tac_new_const(TAC_CONST_UINT);
    c->u.uint_val  = v;
    tv->u.constant = c;
    return tv;
}

Tac_Val *val_ulong(unsigned long v)
{
    Tac_Val *tv    = tac_new_val(TAC_VAL_CONSTANT);
    Tac_Const *c   = tac_new_const(TAC_CONST_ULONG);
    c->u.ulong_val = v;
    tv->u.constant = c;
    return tv;
}

Tac_Val *val_ulong_long(unsigned long long v)
{
    Tac_Val *tv         = tac_new_val(TAC_VAL_CONSTANT);
    Tac_Const *c        = tac_new_const(TAC_CONST_ULONG_LONG);
    c->u.ulong_long_val = v;
    tv->u.constant      = c;
    return tv;
}

Tac_Val *val_float(float v)
{
    Tac_Val *tv    = tac_new_val(TAC_VAL_CONSTANT);
    Tac_Const *c   = tac_new_const(TAC_CONST_FLOAT);
    c->u.float_val = v;
    tv->u.constant = c;
    return tv;
}

Tac_Val *val_double(double v)
{
    Tac_Val *tv     = tac_new_val(TAC_VAL_CONSTANT);
    Tac_Const *c    = tac_new_const(TAC_CONST_DOUBLE);
    c->u.double_val = v;
    tv->u.constant  = c;
    return tv;
}

Tac_Val *val_long_double(long double v)
{
    Tac_Val *tv          = tac_new_val(TAC_VAL_CONSTANT);
    Tac_Const *c         = tac_new_const(TAC_CONST_LONG_DOUBLE);
    c->u.long_double_val = v;
    tv->u.constant       = c;
    return tv;
}

Tac_Val *val_var(const char *name)
{
    Tac_Val *tv    = tac_new_val(TAC_VAL_VAR);
    tv->u.var_name = xstrdup(name);
    return tv;
}

Tac_Val *new_var_val(TacCtx *ctx)
{
    char *d       = new_temp(ctx);
    Tac_Val *v    = tac_new_val(TAC_VAL_VAR);
    v->u.var_name = d;
    return v;
}

// A "fat pointer" on byte-addressed targets (BESM-6: char*/void*) carries a byte
// offset and a marker bit, so it has a different bit layout from a plain word
// pointer (int*, etc.).  True when t is a pointer whose pointee is a character type
// or void.  Used to choose the pointer-representation conversion in emit_cast.
static bool is_fat_pointer(const Type *t)
{
    if (t->kind != TYPE_POINTER)
        return false;
    const Type *target = t->u.pointer.target;
    return is_character(target) || target->kind == TYPE_VOID;
}

Tac_Val *emit_cast(TacCtx *ctx, Tac_Val *src, const Type *from, const Type *to)
{
    bool from_int = is_integer(from);
    bool to_int   = is_integer(to);
    bool from_ptr = is_pointer(from);
    bool to_ptr   = is_pointer(to);
    Tac_Val *dst  = new_var_val(ctx);

    if (from_ptr || to_ptr) {
        size_t from_size = get_size(from);
        size_t to_size   = get_size(to);
        if (from_ptr && to_ptr) {
            bool from_fat = is_fat_pointer(from);
            bool to_fat   = is_fat_pointer(to);
            if (!from_fat && to_fat) {
                // word pointer → char*/void*: set the fat marker and byte offset.
                Tac_Instruction *in     = tac_new_instruction(TAC_INSTRUCTION_PTR_TO_CHAR_PTR);
                in->u.ptr_to_char_ptr.src = src;
                in->u.ptr_to_char_ptr.dst = dst;
                tac_append(ctx, in);
            } else if (from_fat && !to_fat) {
                // char*/void* → word pointer: clear the fat marker and offset.
                Tac_Instruction *in       = tac_new_instruction(TAC_INSTRUCTION_CHAR_PTR_TO_PTR);
                in->u.char_ptr_to_ptr.src = src;
                in->u.char_ptr_to_ptr.dst = dst;
                tac_append(ctx, in);
            } else {
                // word↔word or fat↔fat (incl. char*↔void*): identical representation.
                Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_COPY);
                in->u.copy.src      = src;
                in->u.copy.dst      = dst;
                tac_append(ctx, in);
            }
        } else if (to_ptr) {
            // integer → pointer
            if (from_size < to_size) {
                if (is_signed(from)) {
                    Tac_Instruction *in   = tac_new_instruction(TAC_INSTRUCTION_SIGN_EXTEND);
                    in->u.sign_extend.src = src;
                    in->u.sign_extend.dst = dst;
                    tac_append(ctx, in);
                } else {
                    Tac_Instruction *in   = tac_new_instruction(TAC_INSTRUCTION_ZERO_EXTEND);
                    in->u.zero_extend.src = src;
                    in->u.zero_extend.dst = dst;
                    tac_append(ctx, in);
                }
            } else {
                Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_COPY);
                in->u.copy.src      = src;
                in->u.copy.dst      = dst;
                tac_append(ctx, in);
            }
        } else {
            // pointer → integer
            if (from_size > to_size) {
                Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_TRUNCATE);
                in->u.truncate.src  = src;
                in->u.truncate.dst  = dst;
                tac_append(ctx, in);
            } else {
                Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_COPY);
                in->u.copy.src      = src;
                in->u.copy.dst      = dst;
                tac_append(ctx, in);
            }
        }
        return val_var(dst->u.var_name);
    }

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
            Tac_Instruction *in   = tac_new_instruction(TAC_INSTRUCTION_ZERO_EXTEND);
            in->u.zero_extend.src = src;
            in->u.zero_extend.dst = dst;
            tac_append(ctx, in);
        }
    } else if (!from_int && to_int) {
        // float/double/long double → integer
        bool from_float       = (from->kind == TYPE_FLOAT);
        bool from_long_double = (from->kind == TYPE_LONG_DOUBLE);
        if (is_signed(to)) {
            Tac_InstructionKind op  = from_float         ? TAC_INSTRUCTION_FLOAT_TO_INT
                                      : from_long_double ? TAC_INSTRUCTION_LONG_DOUBLE_TO_INT
                                                         : TAC_INSTRUCTION_DOUBLE_TO_INT;
            Tac_Instruction *in     = tac_new_instruction(op);
            in->u.double_to_int.src = src;
            in->u.double_to_int.dst = dst;
            tac_append(ctx, in);
        } else {
            Tac_InstructionKind op   = from_float         ? TAC_INSTRUCTION_FLOAT_TO_UINT
                                       : from_long_double ? TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT
                                                          : TAC_INSTRUCTION_DOUBLE_TO_UINT;
            Tac_Instruction *in      = tac_new_instruction(op);
            in->u.double_to_uint.src = src;
            in->u.double_to_uint.dst = dst;
            tac_append(ctx, in);
        }
    } else if (from_int && !to_int) {
        // integer → float/double/long double
        bool to_float       = (to->kind == TYPE_FLOAT);
        bool to_long_double = (to->kind == TYPE_LONG_DOUBLE);
        if (is_signed(from)) {
            Tac_InstructionKind op  = to_float         ? TAC_INSTRUCTION_INT_TO_FLOAT
                                      : to_long_double ? TAC_INSTRUCTION_INT_TO_LONG_DOUBLE
                                                       : TAC_INSTRUCTION_INT_TO_DOUBLE;
            Tac_Instruction *in     = tac_new_instruction(op);
            in->u.int_to_double.src = src;
            in->u.int_to_double.dst = dst;
            tac_append(ctx, in);
        } else {
            Tac_InstructionKind op   = to_float         ? TAC_INSTRUCTION_UINT_TO_FLOAT
                                       : to_long_double ? TAC_INSTRUCTION_UINT_TO_LONG_DOUBLE
                                                        : TAC_INSTRUCTION_UINT_TO_DOUBLE;
            Tac_Instruction *in      = tac_new_instruction(op);
            in->u.uint_to_double.src = src;
            in->u.uint_to_double.dst = dst;
            tac_append(ctx, in);
        }
    } else {
        // float ↔ double ↔ long double, or same-type copy
        if (from->kind == TYPE_FLOAT && to->kind == TYPE_DOUBLE) {
            Tac_Instruction *in       = tac_new_instruction(TAC_INSTRUCTION_FLOAT_TO_DOUBLE);
            in->u.float_to_double.src = src;
            in->u.float_to_double.dst = dst;
            tac_append(ctx, in);
        } else if (from->kind == TYPE_DOUBLE && to->kind == TYPE_FLOAT) {
            Tac_Instruction *in       = tac_new_instruction(TAC_INSTRUCTION_DOUBLE_TO_FLOAT);
            in->u.double_to_float.src = src;
            in->u.double_to_float.dst = dst;
            tac_append(ctx, in);
        } else if (from->kind == TYPE_LONG_DOUBLE && to->kind == TYPE_DOUBLE) {
            Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_LONG_DOUBLE_TO_DOUBLE);
            in->u.long_double_to_double.src = src;
            in->u.long_double_to_double.dst = dst;
            tac_append(ctx, in);
        } else if (from->kind == TYPE_DOUBLE && to->kind == TYPE_LONG_DOUBLE) {
            Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_DOUBLE_TO_LONG_DOUBLE);
            in->u.double_to_long_double.src = src;
            in->u.double_to_long_double.dst = dst;
            tac_append(ctx, in);
        } else if (from->kind == TYPE_LONG_DOUBLE && to->kind == TYPE_FLOAT) {
            Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_LONG_DOUBLE_TO_FLOAT);
            in->u.long_double_to_float.src = src;
            in->u.long_double_to_float.dst = dst;
            tac_append(ctx, in);
        } else if (from->kind == TYPE_FLOAT && to->kind == TYPE_LONG_DOUBLE) {
            Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_FLOAT_TO_LONG_DOUBLE);
            in->u.float_to_long_double.src = src;
            in->u.float_to_long_double.dst = dst;
            tac_append(ctx, in);
        } else {
            Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_COPY);
            in->u.copy.src      = src;
            in->u.copy.dst      = dst;
            tac_append(ctx, in);
        }
    }
    return val_var(dst->u.var_name);
}

void emit_jump(TacCtx *ctx, const char *target)
{
    Tac_Instruction *j = tac_new_instruction(TAC_INSTRUCTION_JUMP);
    j->u.jump.target   = xstrdup(target);
    tac_append(ctx, j);
}

void emit_label(TacCtx *ctx, const char *name)
{
    Tac_Instruction *l = tac_new_instruction(TAC_INSTRUCTION_LABEL);
    l->u.label.name    = xstrdup(name);
    tac_append(ctx, l);
}

//
// Type conversion: AST Type → TAC Type
//

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

Tac_Type *ast_type_to_tac_type(const Type *t)
{
    switch (t->kind) {
    case TYPE_VOID:
        return tac_new_type(TAC_TYPE_VOID);
    case TYPE_CHAR:
        return tac_new_type(TAC_TYPE_CHAR);
    case TYPE_SCHAR:
        return tac_new_type(TAC_TYPE_SCHAR);
    case TYPE_UCHAR:
        return tac_new_type(TAC_TYPE_UCHAR);
    case TYPE_SHORT:
        return tac_new_type(TAC_TYPE_SHORT);
    case TYPE_USHORT:
        return tac_new_type(TAC_TYPE_USHORT);
    case TYPE_INT:
        return tac_new_type(TAC_TYPE_INT);
    case TYPE_UINT:
        return tac_new_type(TAC_TYPE_UINT);
    case TYPE_LONG:
        return tac_new_type(TAC_TYPE_LONG);
    case TYPE_ULONG:
        return tac_new_type(TAC_TYPE_ULONG);
    case TYPE_LONG_LONG:
        return tac_new_type(TAC_TYPE_LONG_LONG);
    case TYPE_ULONG_LONG:
        return tac_new_type(TAC_TYPE_ULONG_LONG);
    case TYPE_FLOAT:
        return tac_new_type(TAC_TYPE_FLOAT);
    case TYPE_DOUBLE:
        return tac_new_type(TAC_TYPE_DOUBLE);
    case TYPE_LONG_DOUBLE:
        return tac_new_type(TAC_TYPE_LONG_DOUBLE);
    case TYPE_ENUM:
        return tac_new_type(TAC_TYPE_INT);
    case TYPE_POINTER: {
        Tac_Type *tp              = tac_new_type(TAC_TYPE_POINTER);
        tp->u.pointer.target_type = ast_type_to_tac_type(t->u.pointer.target);
        return tp;
    }
    case TYPE_ARRAY: {
        Tac_Type *ta          = tac_new_type(TAC_TYPE_ARRAY);
        ta->u.array.elem_type = ast_type_to_tac_type(t->u.array.element);
        if (t->u.array.size) {
            ta->u.array.size = (int)(get_size(t) / get_size(t->u.array.element));
        } else {
            ta->u.array.size = 0; // incomplete array type (e.g. extern int arr[])
        }
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
        Tac_Type *ts         = tac_new_type(TAC_TYPE_STRUCTURE);
        ts->u.structure.tag  = t->u.struct_t.name ? xstrdup(t->u.struct_t.name) : NULL;
        ts->u.structure.size = (int)get_size(t);
        return ts;
    }
    default:
        fatal_error("ast_type_to_tac_type: unsupported type kind %d", (int)t->kind);
    }
}

//
// Top-level translation
//

static Tac_TopLevel *translate_fn(const ExternalDecl *ast)
{
    const char *name  = ast->u.function.name;
    const Symbol *sym = symtab_get(name);

    Tac_TopLevel *tl        = tac_new_toplevel(TAC_TOPLEVEL_FUNCTION);
    tl->u.function.name     = xstrdup(name);
    tl->u.function.global   = sym->u.func.global;
    tl->u.function.params   = params_from_type(ast->u.function.type);
    tl->u.function.variadic = ast->u.function.type && ast->u.function.type->kind == TYPE_FUNCTION &&
                              ast->u.function.type->u.function.variadic;

    if (ast->u.function.body) {
        TacCtx ctx = { NULL, NULL, 0, NULL, NULL, NULL, NULL };
        gen_stmt(&ctx, ast->u.function.body);
        tl->u.function.body   = ctx.head;
        tl->u.function.locals = ctx.locals;
        tac_free_param(ctx.byte_array_locals);

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
        Symbol *sym = symtab_get(id->name);
        Tac_TopLevel *tl;

        if (id->type->kind == TYPE_FUNCTION) {
            continue; // prototype — no TAC needed
        } else if (sym->u.static_var.init_kind == INIT_NONE) {
            continue; // extern-only declaration — no storage to allocate
        } else {
            tl                              = tac_new_toplevel(TAC_TOPLEVEL_STATIC_VARIABLE);
            tl->u.static_variable.name      = xstrdup(id->name);
            tl->u.static_variable.global    = sym->u.static_var.global;
            tl->u.static_variable.type      = ast_type_to_tac_type(sym->type);
            tl->u.static_variable.init_list = sym->u.static_var.init_list;
            sym->u.static_var.init_list     = NULL; // transfer ownership to TAC
        }
        *tail = tl;
        tail  = &tl->next;
    }

    // Scan each static variable's init list for POINTER / FAT_POINTER entries that
    // reference SYM_CONST symbols (string literals). Emit a TAC_TOPLEVEL_STATIC_CONSTANT
    // for each and prepend it before the variable that uses it.  (A char* initializer is a
    // fat pointer, so the string it points at arrives as FAT_POINTER, not POINTER.)
    Tac_TopLevel *constants_head = NULL;
    Tac_TopLevel **ctail         = &constants_head;
    for (const Tac_TopLevel *cur = head; cur; cur = cur->next) {
        for (const Tac_StaticInit *init = cur->u.static_variable.init_list; init;
             init                       = init->next) {
            if (init->kind != TAC_STATIC_INIT_POINTER &&
                init->kind != TAC_STATIC_INIT_FAT_POINTER)
                continue;
            const char *sname = init->u.pointer.name;
            Symbol *sym       = symtab_get(sname);
            if (!sym || sym->kind != SYM_CONST || !sym->u.const_init)
                continue;
            Tac_TopLevel *sc           = tac_new_toplevel(TAC_TOPLEVEL_STATIC_CONSTANT);
            sc->u.static_constant.name = xstrdup(sname);
            sc->u.static_constant.type = ast_type_to_tac_type(sym->type);
            sc->u.static_constant.init = sym->u.const_init;
            sym->u.const_init          = NULL; // transfer ownership to TAC
            *ctail                     = sc;
            ctail                      = &sc->next;
        }
    }
    if (constants_head) {
        *ctail = head;
        return constants_head;
    }
    return head;
}

static Tac_TopLevel *translate_external_decl(const ExternalDecl *ast)
{
    if (!ast)
        return NULL;
    switch (ast->kind) {
    case EXTERNAL_DECL_FUNCTION:
        return translate_fn(ast);
    case EXTERNAL_DECL_DECLARATION:
        return translate_decl(ast->u.declaration);
    }
    return NULL;
}

// Return a freshly allocated copy of `name` with a leading '%' prepended.
static char *percent_name(const char *name)
{
    size_t n  = strlen(name);
    char *out = xalloc(n + 2, __func__, __FILE__, __LINE__);
    out[0]    = '%';
    memcpy(out + 1, name, n + 1);
    return out;
}

// If `v` (or any value in its ->next chain) names a frame-resident variable
// (member of `autos`), rewrite its name with a leading '%' in place.
static void percent_vals(Tac_Val *v, const StringMap *autos)
{
    intptr_t dummy;
    for (; v; v = v->next) {
        if (v->kind != TAC_VAL_VAR)
            continue;
        if (!map_get(autos, v->u.var_name, &dummy))
            continue;
        char *renamed = percent_name(v->u.var_name);
        xfree(v->u.var_name);
        v->u.var_name = renamed;
    }
}

// Same, for a bare char* name field (copy_to_offset.dst / copy_from_offset.src).
static void percent_name_field(char **name, const StringMap *autos)
{
    intptr_t dummy;
    if (!*name || !map_get(autos, *name, &dummy))
        return;
    char *renamed = percent_name(*name);
    xfree(*name);
    *name = renamed;
}

// Visit every operand of one instruction and prefix frame-resident names with '%'.
// Mirrors backend/besm6/frame.c collect_instr's operand coverage.
static void percent_instr(Tac_Instruction *in, const StringMap *autos)
{
    switch (in->kind) {
    case TAC_INSTRUCTION_RETURN:
        percent_vals(in->u.return_.src, autos);
        break;
    case TAC_INSTRUCTION_SIGN_EXTEND:
        percent_vals(in->u.sign_extend.src, autos);
        percent_vals(in->u.sign_extend.dst, autos);
        break;
    case TAC_INSTRUCTION_TRUNCATE:
        percent_vals(in->u.truncate.src, autos);
        percent_vals(in->u.truncate.dst, autos);
        break;
    case TAC_INSTRUCTION_ZERO_EXTEND:
        percent_vals(in->u.zero_extend.src, autos);
        percent_vals(in->u.zero_extend.dst, autos);
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_INT:
        percent_vals(in->u.double_to_int.src, autos);
        percent_vals(in->u.double_to_int.dst, autos);
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_UINT:
        percent_vals(in->u.double_to_uint.src, autos);
        percent_vals(in->u.double_to_uint.dst, autos);
        break;
    case TAC_INSTRUCTION_INT_TO_DOUBLE:
        percent_vals(in->u.int_to_double.src, autos);
        percent_vals(in->u.int_to_double.dst, autos);
        break;
    case TAC_INSTRUCTION_UINT_TO_DOUBLE:
        percent_vals(in->u.uint_to_double.src, autos);
        percent_vals(in->u.uint_to_double.dst, autos);
        break;
    case TAC_INSTRUCTION_FLOAT_TO_DOUBLE:
        percent_vals(in->u.float_to_double.src, autos);
        percent_vals(in->u.float_to_double.dst, autos);
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_FLOAT:
        percent_vals(in->u.double_to_float.src, autos);
        percent_vals(in->u.double_to_float.dst, autos);
        break;
    case TAC_INSTRUCTION_INT_TO_FLOAT:
        percent_vals(in->u.int_to_float.src, autos);
        percent_vals(in->u.int_to_float.dst, autos);
        break;
    case TAC_INSTRUCTION_UINT_TO_FLOAT:
        percent_vals(in->u.uint_to_float.src, autos);
        percent_vals(in->u.uint_to_float.dst, autos);
        break;
    case TAC_INSTRUCTION_FLOAT_TO_INT:
        percent_vals(in->u.float_to_int.src, autos);
        percent_vals(in->u.float_to_int.dst, autos);
        break;
    case TAC_INSTRUCTION_FLOAT_TO_UINT:
        percent_vals(in->u.float_to_uint.src, autos);
        percent_vals(in->u.float_to_uint.dst, autos);
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_INT:
        percent_vals(in->u.long_double_to_int.src, autos);
        percent_vals(in->u.long_double_to_int.dst, autos);
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_UINT:
        percent_vals(in->u.long_double_to_uint.src, autos);
        percent_vals(in->u.long_double_to_uint.dst, autos);
        break;
    case TAC_INSTRUCTION_INT_TO_LONG_DOUBLE:
        percent_vals(in->u.int_to_long_double.src, autos);
        percent_vals(in->u.int_to_long_double.dst, autos);
        break;
    case TAC_INSTRUCTION_UINT_TO_LONG_DOUBLE:
        percent_vals(in->u.uint_to_long_double.src, autos);
        percent_vals(in->u.uint_to_long_double.dst, autos);
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_DOUBLE:
        percent_vals(in->u.long_double_to_double.src, autos);
        percent_vals(in->u.long_double_to_double.dst, autos);
        break;
    case TAC_INSTRUCTION_DOUBLE_TO_LONG_DOUBLE:
        percent_vals(in->u.double_to_long_double.src, autos);
        percent_vals(in->u.double_to_long_double.dst, autos);
        break;
    case TAC_INSTRUCTION_LONG_DOUBLE_TO_FLOAT:
        percent_vals(in->u.long_double_to_float.src, autos);
        percent_vals(in->u.long_double_to_float.dst, autos);
        break;
    case TAC_INSTRUCTION_FLOAT_TO_LONG_DOUBLE:
        percent_vals(in->u.float_to_long_double.src, autos);
        percent_vals(in->u.float_to_long_double.dst, autos);
        break;
    case TAC_INSTRUCTION_PTR_TO_CHAR_PTR:
        percent_vals(in->u.ptr_to_char_ptr.src, autos);
        percent_vals(in->u.ptr_to_char_ptr.dst, autos);
        break;
    case TAC_INSTRUCTION_CHAR_PTR_TO_PTR:
        percent_vals(in->u.char_ptr_to_ptr.src, autos);
        percent_vals(in->u.char_ptr_to_ptr.dst, autos);
        break;
    case TAC_INSTRUCTION_UNARY:
        percent_vals(in->u.unary.src, autos);
        percent_vals(in->u.unary.dst, autos);
        break;
    case TAC_INSTRUCTION_BINARY:
        percent_vals(in->u.binary.src1, autos);
        percent_vals(in->u.binary.src2, autos);
        percent_vals(in->u.binary.dst, autos);
        break;
    case TAC_INSTRUCTION_COPY:
        percent_vals(in->u.copy.src, autos);
        percent_vals(in->u.copy.dst, autos);
        break;
    case TAC_INSTRUCTION_GET_ADDRESS:
    case TAC_INSTRUCTION_GET_ADDRESS_BYTE:
    case TAC_INSTRUCTION_GET_ADDRESS_DECAY:
        percent_vals(in->u.get_address.src, autos);
        percent_vals(in->u.get_address.dst, autos);
        break;
    case TAC_INSTRUCTION_LOAD:
    case TAC_INSTRUCTION_LOAD_BYTE:
        percent_vals(in->u.load.src_ptr, autos);
        percent_vals(in->u.load.dst, autos);
        break;
    case TAC_INSTRUCTION_STORE:
    case TAC_INSTRUCTION_STORE_BYTE:
        percent_vals(in->u.store.src, autos);
        percent_vals(in->u.store.dst_ptr, autos);
        break;
    case TAC_INSTRUCTION_ADD_PTR:
        percent_vals(in->u.add_ptr.ptr, autos);
        percent_vals(in->u.add_ptr.index, autos);
        percent_vals(in->u.add_ptr.dst, autos);
        break;
    case TAC_INSTRUCTION_PTR_DIFF:
        percent_vals(in->u.ptr_diff.ptr_a, autos);
        percent_vals(in->u.ptr_diff.ptr_b, autos);
        percent_vals(in->u.ptr_diff.dst, autos);
        break;
    case TAC_INSTRUCTION_COPY_TO_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET:
        percent_vals(in->u.copy_to_offset.src, autos);
        percent_name_field(&in->u.copy_to_offset.dst, autos);
        break;
    case TAC_INSTRUCTION_COPY_FROM_OFFSET:
    case TAC_INSTRUCTION_COPY_BYTE_FROM_OFFSET:
        percent_name_field(&in->u.copy_from_offset.src, autos);
        percent_vals(in->u.copy_from_offset.dst, autos);
        break;
    case TAC_INSTRUCTION_JUMP:
    case TAC_INSTRUCTION_LABEL:
        break;
    case TAC_INSTRUCTION_JUMP_IF_ZERO:
        percent_vals(in->u.jump_if_zero.condition, autos);
        break;
    case TAC_INSTRUCTION_JUMP_IF_NOT_ZERO:
        percent_vals(in->u.jump_if_not_zero.condition, autos);
        break;
    case TAC_INSTRUCTION_FUN_CALL:
        percent_vals(in->u.fun_call.args, autos);
        percent_vals(in->u.fun_call.dst, autos);
        break;
    case TAC_INSTRUCTION_ALLOCATE_LOCAL:
        percent_name_field(&in->u.allocate_local.name, autos);
        break;
    }
}

// Rewrite parameter and automatic-local names with a leading '%', so the backend
// can tell frame-resident names from module-level globals by name alone. Globals
// and compiler temporaries (already '%'-prefixed by new_temp) are left untouched.
// Run before optimize_function so the optimizer's private-name analysis, the
// stored params/locals lists, and the body all agree on the percent-prefixed spelling.
static void percent_locals_in_function(const Tac_TopLevel *fn)
{
    StringMap autos;
    map_init(&autos);
    for (const Tac_Param *p = fn->u.function.params; p; p = p->next)
        if (p->name)
            map_insert(&autos, p->name, 1, 0);
    for (const Tac_Param *p = fn->u.function.locals; p; p = p->next)
        if (p->name)
            map_insert(&autos, p->name, 1, 0);

    // Rewrite the body first (matching the still-raw names), then the lists.
    for (Tac_Instruction *in = fn->u.function.body; in; in = in->next)
        percent_instr(in, &autos);

    for (Tac_Param *p = fn->u.function.params; p; p = p->next)
        if (p->name) {
            char *d = percent_name(p->name);
            xfree(p->name);
            p->name = d;
        }
    for (Tac_Param *p = fn->u.function.locals; p; p = p->next)
        if (p->name) {
            char *d = percent_name(p->name);
            xfree(p->name);
            p->name = d;
        }

    map_destroy(&autos);
}

//
// Convert the AST to TAC.
//
Tac_TopLevel *translate(const ExternalDecl *ast, OptFlags flags)
{
    Tac_TopLevel *tac = translate_external_decl(ast);
    for (Tac_TopLevel *t = tac; t; t = t->next) {
        // Each function is optimized against its own toplevel, which carries the
        // params + automatic locals needed to tell private locals from globals.
        if (t->kind == TAC_TOPLEVEL_FUNCTION) {
            percent_locals_in_function(t);
            t->u.function.body = optimize_function(t->u.function.body, flags, t);
        }
    }
    return tac;
}
