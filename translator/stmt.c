//
// Statement lowering: AST Stmt → TAC instructions.
//

#include <stdlib.h>

#include "structtab.h"
#include "translate.h"
#include "xalloc.h"

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

void gen_compound_init(TacCtx *ctx, const char *var_name, int base_offset,
                       const Initializer *init)
{
    if (init->kind == INITIALIZER_SINGLE) {
        Tac_Val *src                    = gen_expr(ctx, init->u.expr);
        Tac_Instruction *in             = tac_new_instruction(TAC_INSTRUCTION_COPY_TO_OFFSET);
        in->u.copy_to_offset.src        = src;
        in->u.copy_to_offset.dst        = xstrdup(var_name);
        in->u.copy_to_offset.offset     = base_offset;
        tac_append(ctx, in);
        return;
    }
    const Type *t = init->type;
    if (t->kind == TYPE_ARRAY) {
        int elem_size = (int)get_size(t->u.array.element);
        int i         = 0;
        for (const InitItem *item = init->u.items; item; item = item->next, i++)
            gen_compound_init(ctx, var_name, base_offset + i * elem_size, item->init);
    } else if (t->kind == TYPE_STRUCT) {
        const StructDef *def = structtab_find(t->u.struct_t.name);
        const FieldDef  *fld = def->members;
        for (const InitItem *item = init->u.items; item; item = item->next, fld = fld->next)
            gen_compound_init(ctx, var_name, base_offset + fld->offset, item->init);
    } else {
        fatal_error("Compound initializer for unsupported type %d in TAC lowering",
                    (int)t->kind);
    }
}

static void gen_local_decl(TacCtx *ctx, const Declaration *decl)
{
    if (decl->kind != DECL_VAR)
        return;
    // Variables with automatic storage are private to this function; record
    // their names so the optimizer does not mistake them for observable globals.
    // static/extern/typedef/thread-local declarators are not automatics: a
    // static or extern name denotes observable storage and must be left out.
    StorageClass storage =
        decl->u.var.specifiers ? decl->u.var.specifiers->storage : STORAGE_CLASS_NONE;
    bool is_automatic = storage == STORAGE_CLASS_NONE ||
                        storage == STORAGE_CLASS_AUTO ||
                        storage == STORAGE_CLASS_REGISTER;
    for (const InitDeclarator *id = decl->u.var.declarators; id; id = id->next) {
        if (is_automatic && id->name)
            tac_record_local(ctx, id->name);
    }
    for (InitDeclarator *id = decl->u.var.declarators; id; id = id->next) {
        if (id->init && id->init->kind == INITIALIZER_SINGLE) {
            Tac_Val *src        = gen_expr(ctx, id->init->u.expr);
            Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_COPY);
            in->u.copy.src      = src;
            in->u.copy.dst      = val_var(id->name);
            tac_append(ctx, in);
        } else if (id->init && id->init->kind == INITIALIZER_COMPOUND) {
            gen_compound_init(ctx, id->name, 0, id->init);
        }
    }
}

void gen_stmt(TacCtx *ctx, Stmt *stmt)
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

        CaseList cases = { NULL, NULL, NULL };
        cases.tail     = &cases.head;
        collect_cases(ctx, stmt->u.switch_stmt.body, &cases);

        Tac_Val *ctrl_raw     = gen_expr(ctx, stmt->u.switch_stmt.expr);
        Tac_Val *ctrl_dst     = new_var_val(ctx);
        const char *ctrl_name = ctrl_dst->u.var_name; // save before ownership transfer
        Tac_Instruction *cp   = tac_new_instruction(TAC_INSTRUCTION_COPY);
        cp->u.copy.src        = ctrl_raw;
        cp->u.copy.dst        = ctrl_dst;
        tac_append(ctx, cp);

        for (CaseEntry *e = cases.head; e; e = e->next) {
            Tac_Val *cval        = gen_expr(ctx, e->expr);
            Tac_Val *cmp_dst     = new_var_val(ctx);
            const char *cmp_name = cmp_dst->u.var_name;
            Tac_Instruction *bin = tac_new_instruction(TAC_INSTRUCTION_BINARY);
            bin->u.binary.op     = TAC_BINARY_EQUAL;
            bin->u.binary.src1   = val_var(ctrl_name);
            bin->u.binary.src2   = cval;
            bin->u.binary.dst    = cmp_dst;
            tac_append(ctx, bin);
            Tac_Instruction *jnz = tac_new_instruction(TAC_INSTRUCTION_JUMP_IF_NOT_ZERO);
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
