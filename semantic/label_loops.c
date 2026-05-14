//
// Loop labeling
//
#include "semantic.h"
#include "xalloc.h"

typedef struct {
    const char *break_lbl;
    const char *cont_lbl;
} LabelFrame;

static int label_seq;

static char *make_tac_label(void)
{
    char buf[48];
    snprintf(buf, sizeof buf, ".L%d", label_seq++);
    return xstrdup(buf);
}

static void label_statement(Stmt *stmt, LabelFrame *stack, int *depth);

//
// Annotate loops and break/continue statements.
//
void label_loops(ExternalDecl *ast) // cppcheck-suppress constParameterPointer
{
    label_seq = 0;
    if (!ast || ast->kind != EXTERNAL_DECL_FUNCTION || !ast->u.function.body) {
        return;
    }
    LabelFrame stack[256];
    int        depth = 0;
    label_statement(ast->u.function.body, stack, &depth);
}

static void label_statement(Stmt *stmt, LabelFrame *stack, int *depth)
{
    if (!stmt) {
        return;
    }
    switch (stmt->kind) {
    case STMT_EXPR:
    case STMT_GOTO:
    case STMT_RETURN:
        break;
    case STMT_CONTINUE: {
        int i = *depth - 1;
        while (i >= 0 && stack[i].cont_lbl == NULL) {
            i--;
        }
        if (i < 0) {
            fatal_error("continue statement not inside loop");
        }
        stmt->branch_target_label = xstrdup(stack[i].cont_lbl);
        break;
    }
    case STMT_BREAK: {
        if (*depth <= 0) {
            fatal_error("break statement not inside loop or switch");
        }
        stmt->branch_target_label = xstrdup(stack[*depth - 1].break_lbl);
        break;
    }
    case STMT_COMPOUND: {
        for (DeclOrStmt *ds = stmt->u.compound; ds; ds = ds->next) {
            if (ds->kind == DECL_OR_STMT_STMT) {
                label_statement(ds->u.stmt, stack, depth);
            }
        }
        break;
    }
    case STMT_IF:
        label_statement(stmt->u.if_stmt.then_stmt, stack, depth);
        label_statement(stmt->u.if_stmt.else_stmt, stack, depth);
        break;
    case STMT_SWITCH: {
        char *end = make_tac_label();
        stmt->loop_end_label      = end;
        stmt->loop_continue_label = NULL;
        stack[*depth].break_lbl     = end;
        stack[*depth].cont_lbl      = NULL;
        (*depth)++;
        label_statement(stmt->u.switch_stmt.body, stack, depth);
        (*depth)--;
        break;
    }
    case STMT_WHILE: {
        char *end  = make_tac_label();
        char *cont = make_tac_label();
        stmt->loop_end_label        = end;
        stmt->loop_continue_label   = cont;
        stack[*depth].break_lbl     = end;
        stack[*depth].cont_lbl      = cont;
        (*depth)++;
        label_statement(stmt->u.while_stmt.body, stack, depth);
        (*depth)--;
        break;
    }
    case STMT_DO_WHILE: {
        char *end  = make_tac_label();
        char *cont = make_tac_label();
        stmt->loop_end_label        = end;
        stmt->loop_continue_label   = cont;
        stack[*depth].break_lbl     = end;
        stack[*depth].cont_lbl      = cont;
        (*depth)++;
        label_statement(stmt->u.do_while.body, stack, depth);
        (*depth)--;
        break;
    }
    case STMT_FOR: {
        char *end  = make_tac_label();
        char *cont = make_tac_label();
        stmt->loop_end_label        = end;
        stmt->loop_continue_label   = cont;
        stack[*depth].break_lbl     = end;
        stack[*depth].cont_lbl      = cont;
        (*depth)++;
        label_statement(stmt->u.for_stmt.body, stack, depth);
        (*depth)--;
        break;
    }
    case STMT_LABELED:
        label_statement(stmt->u.labeled.stmt, stack, depth);
        break;
    case STMT_CASE:
        label_statement(stmt->u.case_stmt.stmt, stack, depth);
        break;
    case STMT_DEFAULT:
        label_statement(stmt->u.default_stmt, stack, depth);
        break;
    default:
        break;
    }
}
