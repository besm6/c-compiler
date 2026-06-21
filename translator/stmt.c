//
// Statement lowering: AST Stmt → TAC instructions.
//

#include <stdlib.h>
#include <string.h>

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

// Lower `char arr[N] = "…"` to a run of byte stores into the frame slot: one
// COPY_BYTE_TO_OFFSET per source byte at successive byte offsets, then zero-fill up to
// the array's size (C string-init semantics — the terminating null and any trailing
// zeros).  Used for a 1-D char array and for each inner string row of a multi-dimensional
// char array (whose contiguous rows the caller has already offset).  Char data keeps its
// source (ASCII) encoding, like every scalar char value; only the static-data path repacks
// to KOI-7 (which differs solely for lowercase Latin — see docs/KOI7_Encoding.md).
static void gen_char_array_string_init(TacCtx *ctx, const char *var_name, int base_offset,
                                       const Expr *str_expr, int array_bytes)
{
    char *decoded = decode_c_string_literal(str_expr->u.literal->u.string_val);
    size_t len    = strlen(decoded);
    for (int i = 0; i < array_bytes; i++) {
        int byte                    = (size_t)i < len ? (unsigned char)decoded[i] : 0;
        Tac_Instruction *in         = tac_new_instruction(TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET);
        in->u.copy_to_offset.src    = val_int(byte);
        in->u.copy_to_offset.dst    = xstrdup(var_name);
        in->u.copy_to_offset.offset = base_offset + i;
        tac_append(ctx, in);
    }
    xfree(decoded);
}

// True for a string literal initializing a char array (`char a[N] = "…"`), whether a
// top-level 1-D array or an inner row of a multi-dimensional char array.
static bool is_char_array_string_init(const Type *type, const Initializer *init)
{
    return init->kind == INITIALIZER_SINGLE && type && unalias(type)->kind == TYPE_ARRAY &&
           init->u.expr->kind == EXPR_LITERAL &&
           init->u.expr->u.literal->kind == LITERAL_STRING;
}

void gen_compound_init(TacCtx *ctx, const char *var_name, int base_offset, const Initializer *init)
{
    if (init->kind == INITIALIZER_SINGLE) {
        // A string literal initializing a char array (e.g. an inner row of a multi-dim
        // char array): store its bytes individually rather than COPY a whole-word pointer.
        if (is_char_array_string_init(init->type, init)) {
            gen_char_array_string_init(ctx, var_name, base_offset, init->u.expr,
                                       (int)get_size(init->type));
            return;
        }
        Tac_Val *src    = gen_expr(ctx, init->u.expr);
        const Type *it0 = init->type ? unalias(init->type) : NULL;
        if (it0 && (it0->kind == TYPE_STRUCT || it0->kind == TYPE_UNION)) {
            // A whole struct/union value (not a scalar leaf) — copy it word by word.
            gen_struct_assign(ctx, var_name, base_offset, src->u.var_name,
                              (int)get_size(init->type));
            tac_free_val(src);
            return;
        }
        // A char/signed char/unsigned char leaf occupies a single packed byte;
        // its element offset is a byte offset, so it must use the byte-store kind
        // (the word-store kind rejects a sub-word offset).
        bool byte_leaf              = init->type && get_size(init->type) == 1;
        Tac_Instruction *in         = tac_new_instruction(
            byte_leaf ? TAC_INSTRUCTION_COPY_BYTE_TO_OFFSET : TAC_INSTRUCTION_COPY_TO_OFFSET);
        in->u.copy_to_offset.src    = src;
        in->u.copy_to_offset.dst    = xstrdup(var_name);
        in->u.copy_to_offset.offset = base_offset;
        tac_append(ctx, in);
        return;
    }
    const Type *t = unalias(init->type);
    if (t->kind == TYPE_ARRAY) {
        int elem_size = (int)get_size(t->u.array.element);
        int i         = 0;
        for (const InitItem *item = init->u.items; item; item = item->next, i++)
            gen_compound_init(ctx, var_name, base_offset + i * elem_size, item->init);
    } else if (t->kind == TYPE_STRUCT) {
        const StructDef *def = structtab_find(t->u.struct_t.name);
        const FieldDef *fld  = def->members;
        for (const InitItem *item = init->u.items; item; item = item->next, fld = fld->next)
            gen_compound_init(ctx, var_name, base_offset + fld->offset, item->init);
    } else if (t->kind == TYPE_UNION) {
        // typecheck_init reduced the union initializer to its single first member,
        // which lives at offset 0 of the union — initialize it there.  No structtab
        // lookup is needed, so this works for block-scope unions too.
        if (init->u.items)
            gen_compound_init(ctx, var_name, base_offset, init->u.items->init);
    } else {
        fatal_error("Compound initializer for unsupported type %d in TAC lowering", (int)t->kind);
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
    bool is_automatic = storage == STORAGE_CLASS_NONE || storage == STORAGE_CLASS_AUTO ||
                        storage == STORAGE_CLASS_REGISTER;
    for (const InitDeclarator *id = decl->u.var.declarators; id; id = id->next) {
        if (!id->name)
            continue;
        const Type *idt = id->type ? unalias(id->type) : NULL;
        // A block-scope function declaration (int f(void);) is not an automatic:
        // it names an external-linkage function with no frame slot, so it must
        // not be percent-prefixed like a local.
        if (idt && idt->kind == TYPE_FUNCTION)
            continue;
        if (!is_automatic) {
            // A static/extern local array has no frame slot — its storage is the
            // module-local static datum, addressed by its label like a global — but it
            // still decays to its address when used as a value, so record it for the decay.
            if (idt && idt->kind == TYPE_ARRAY)
                tac_record_array_local(ctx, id->name);
            continue;
        }
        tac_record_local(ctx, id->name);
        // Aggregate locals (arrays, structs, unions) occupy contiguous frame slots;
        // emit an AllocateLocal so the backend reserves the full size instead of a
        // single slot. Scalars keep their implicit one-slot allocation. Size and
        // alignment are in target bytes (like every other offset in the TAC stream);
        // each backend converts to its own allocation unit (the besm6 backend divides
        // by the 6-byte machine word).
        // A local array decays to a pointer when used as a value; record it so gen_expr
        // can emit the array-decay GET_ADDRESS (symtab locals are gone by then).
        if (idt && idt->kind == TYPE_ARRAY)
            tac_record_array_local(ctx, id->name);
        if (idt && (idt->kind == TYPE_ARRAY || idt->kind == TYPE_STRUCT ||
                    idt->kind == TYPE_UNION)) {
            int bytes = (int)get_size(id->type);
            if (bytes > 0) {
                Tac_Instruction *in       = tac_new_instruction(TAC_INSTRUCTION_ALLOCATE_LOCAL);
                in->u.allocate_local.name = xstrdup(id->name);
                in->u.allocate_local.size = bytes;
                in->u.allocate_local.alignment = (int)get_alignment(id->type);
                tac_append(ctx, in);
            }
        }
    }
    for (InitDeclarator *id = decl->u.var.declarators; id; id = id->next) {
        if (id->init && id->init->kind == INITIALIZER_SINGLE) {
            // `char a[N] = "…"`: copy the string's bytes into the frame slot (a real
            // per-byte copy, not an alias of the string-constant pointer).
            if (is_char_array_string_init(id->type, id->init)) {
                gen_char_array_string_init(ctx, id->name, 0, id->init->u.expr,
                                           (int)get_size(id->type));
                continue;
            }
            Tac_Val *src     = gen_expr(ctx, id->init->u.expr);
            const Type *idt2 = id->type ? unalias(id->type) : NULL;
            if (idt2 && (idt2->kind == TYPE_STRUCT || idt2->kind == TYPE_UNION)) {
                // Whole-struct initialization (e.g. struct r = other; or struct r = f();):
                // copy every word from the source aggregate into the new local.
                gen_struct_assign(ctx, id->name, 0, src->u.var_name, (int)get_size(id->type));
                tac_free_val(src);
            } else {
                Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_COPY);
                in->u.copy.src      = src;
                in->u.copy.dst      = val_var(id->name);
                tac_append(ctx, in);
            }
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
        if (ctx->sret_name && stmt->u.expr) {
            // Multi-word struct return: copy the result, word by word, into the caller's
            // slot through the hidden return pointer, then return the pointer itself.
            Tac_Val *src = gen_expr(ctx, stmt->u.expr); // VAR naming the source aggregate
            int w        = target_word_bytes();
            int nwords   = ((int)get_size(stmt->u.expr->type) + w - 1) / w;
            for (int i = 0; i < nwords; i++) {
                Tac_Val *t          = new_var_val(ctx);
                Tac_Instruction *ld = tac_new_instruction(TAC_INSTRUCTION_COPY_FROM_OFFSET);
                ld->u.copy_from_offset.src    = xstrdup(src->u.var_name);
                ld->u.copy_from_offset.offset = i * w;
                ld->u.copy_from_offset.dst    = t;
                tac_append(ctx, ld);

                Tac_Val *p          = new_var_val(ctx);
                Tac_Instruction *ap = tac_new_instruction(TAC_INSTRUCTION_ADD_PTR);
                ap->u.add_ptr.ptr   = val_var(ctx->sret_name);
                ap->u.add_ptr.index = val_int(i); // word index
                ap->u.add_ptr.scale = w;          // one word per element → plain add
                ap->u.add_ptr.dst   = p;
                tac_append(ctx, ap);

                Tac_Instruction *st = tac_new_instruction(TAC_INSTRUCTION_STORE);
                st->u.store.src     = val_var(t->u.var_name);
                st->u.store.dst_ptr = val_var(p->u.var_name);
                tac_append(ctx, st);
            }
            tac_free_val(src);
            Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_RETURN);
            in->u.return_.src   = val_var(ctx->sret_name);
            tac_append(ctx, in);
            break;
        }
        Tac_Instruction *in = tac_new_instruction(TAC_INSTRUCTION_RETURN);
        in->u.return_.src   = stmt->u.expr ? gen_expr(ctx, stmt->u.expr) : NULL;
        tac_append(ctx, in);
        break;
    }
    case STMT_IF: {
        Tac_Val *cond = gen_cond_val(ctx, stmt->u.if_stmt.condition);
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
        Tac_Val *cond                = gen_cond_val(ctx, stmt->u.while_stmt.condition);
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
        Tac_Val *cond                     = gen_cond_val(ctx, stmt->u.do_while.condition);
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
            Tac_Val *cond                = gen_cond_val(ctx, stmt->u.for_stmt.condition);
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
