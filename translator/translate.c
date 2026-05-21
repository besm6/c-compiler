//
// AST to TAC lowering: shared helpers, type conversion, and top-level entry points.
//

#include "translate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    return xstruniq("t.", &ctx->temp_id);
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
    Tac_Val *tv             = tac_new_val(TAC_VAL_CONSTANT);
    Tac_Const *c            = tac_new_const(TAC_CONST_ULONG_LONG);
    c->u.ulong_long_val     = v;
    tv->u.constant          = c;
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
    Tac_Val *tv            = tac_new_val(TAC_VAL_CONSTANT);
    Tac_Const *c           = tac_new_const(TAC_CONST_LONG_DOUBLE);
    c->u.long_double_val   = v;
    tv->u.constant         = c;
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
            Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_COPY);
            in->u.copy.src      = src;
            in->u.copy.dst      = dst;
            tac_append(ctx, in);
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
            Tac_InstructionKind op  = from_float       ? TAC_INSTRUCTION_FLOAT_TO_INT
                                    : from_long_double ? TAC_INSTRUCTION_LONG_DOUBLE_TO_INT
                                                       : TAC_INSTRUCTION_DOUBLE_TO_INT;
            Tac_Instruction *in     = tac_new_instruction(op);
            in->u.double_to_int.src = src;
            in->u.double_to_int.dst = dst;
            tac_append(ctx, in);
        } else {
            Tac_InstructionKind op   = from_float       ? TAC_INSTRUCTION_FLOAT_TO_UINT
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
            Tac_InstructionKind op  = to_float       ? TAC_INSTRUCTION_INT_TO_FLOAT
                                    : to_long_double ? TAC_INSTRUCTION_INT_TO_LONG_DOUBLE
                                                     : TAC_INSTRUCTION_INT_TO_DOUBLE;
            Tac_Instruction *in     = tac_new_instruction(op);
            in->u.int_to_double.src = src;
            in->u.int_to_double.dst = dst;
            tac_append(ctx, in);
        } else {
            Tac_InstructionKind op   = to_float       ? TAC_INSTRUCTION_UINT_TO_FLOAT
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
            Tac_Instruction *in        = tac_new_instruction(TAC_INSTRUCTION_FLOAT_TO_DOUBLE);
            in->u.float_to_double.src  = src;
            in->u.float_to_double.dst  = dst;
            tac_append(ctx, in);
        } else if (from->kind == TYPE_DOUBLE && to->kind == TYPE_FLOAT) {
            Tac_Instruction *in       = tac_new_instruction(TAC_INSTRUCTION_DOUBLE_TO_FLOAT);
            in->u.double_to_float.src = src;
            in->u.double_to_float.dst = dst;
            tac_append(ctx, in);
        } else if (from->kind == TYPE_LONG_DOUBLE && to->kind == TYPE_DOUBLE) {
            Tac_Instruction *in              = tac_new_instruction(TAC_INSTRUCTION_LONG_DOUBLE_TO_DOUBLE);
            in->u.long_double_to_double.src  = src;
            in->u.long_double_to_double.dst  = dst;
            tac_append(ctx, in);
        } else if (from->kind == TYPE_DOUBLE && to->kind == TYPE_LONG_DOUBLE) {
            Tac_Instruction *in              = tac_new_instruction(TAC_INSTRUCTION_DOUBLE_TO_LONG_DOUBLE);
            in->u.double_to_long_double.src  = src;
            in->u.double_to_long_double.dst  = dst;
            tac_append(ctx, in);
        } else if (from->kind == TYPE_LONG_DOUBLE && to->kind == TYPE_FLOAT) {
            Tac_Instruction *in             = tac_new_instruction(TAC_INSTRUCTION_LONG_DOUBLE_TO_FLOAT);
            in->u.long_double_to_float.src  = src;
            in->u.long_double_to_float.dst  = dst;
            tac_append(ctx, in);
        } else if (from->kind == TYPE_FLOAT && to->kind == TYPE_LONG_DOUBLE) {
            Tac_Instruction *in             = tac_new_instruction(TAC_INSTRUCTION_FLOAT_TO_LONG_DOUBLE);
            in->u.float_to_long_double.src  = src;
            in->u.float_to_long_double.dst  = dst;
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
        Tac_Type *ts        = tac_new_type(TAC_TYPE_STRUCTURE);
        ts->u.structure.tag = t->u.struct_t.name ? xstrdup(t->u.struct_t.name) : NULL;
        return ts;
    }
    default:
        fatal_error("ast_type_to_tac_type: unsupported type kind %d", (int)t->kind);
    }
}

//
// Top-level translation
//

static Tac_TopLevel *translate_fn(ExternalDecl *ast)
{
    const char *name  = ast->u.function.name;
    const Symbol *sym = symtab_get(name);

    Tac_TopLevel *tl      = tac_new_toplevel(TAC_TOPLEVEL_FUNCTION);
    tl->u.function.name   = xstrdup(name);
    tl->u.function.global = sym->u.func.global;
    tl->u.function.params   = params_from_type(ast->u.function.type);
    tl->u.function.variadic = ast->u.function.type &&
                              ast->u.function.type->kind == TYPE_FUNCTION &&
                              ast->u.function.type->u.function.variadic;

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
        Symbol *sym = symtab_get(id->name);
        Tac_TopLevel *tl;

        if (id->type->kind == TYPE_FUNCTION) {
            tl                    = tac_new_toplevel(TAC_TOPLEVEL_FUNCTION);
            tl->u.function.name   = xstrdup(id->name);
            tl->u.function.global = sym->u.func.global;
            tl->u.function.params = params_from_type(id->type);
            // body stays NULL — this is a prototype
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
    return head;
}

static Tac_TopLevel *translate_external_decl(ExternalDecl *ast)
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

//
// Convert the AST to TAC.
//
Tac_TopLevel *translate(ExternalDecl *ast) // cppcheck-suppress constParameterPointer
{
    return translate_external_decl(ast);
}
