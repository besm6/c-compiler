//
// Internal types for translator.
//
#ifndef TRANSLATE_H
#define TRANSLATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ast.h"
#include "semantic.h"
#include "symtab.h"
#include "tac.h"

//
// TAC generation context — one per function being lowered.
//
typedef struct {
    Tac_Instruction *head;
    Tac_Instruction *tail;
    int temp_id;
    Tac_TopLevel *static_constants; // strings accumulated during body lowering
} TacCtx;

//
// Switch case tracking — built by collect_cases(), consumed by gen_stmt STMT_SWITCH.
//
typedef struct CaseEntry {
    Expr *expr;        // case constant expression
    const char *label; // non-owning ptr to stmt->branch_target_label
    struct CaseEntry *next;
} CaseEntry;

typedef struct {
    CaseEntry *head;
    CaseEntry **tail;
    const char *default_label; // non-owning ptr, or NULL
} CaseList;

// Enable debug output
extern int translator_debug;
extern int import_debug;
extern int export_debug;
extern int wio_debug;
extern int xalloc_debug;

//
// Low-level TAC-building helpers (translate.c)
//
void tac_append(TacCtx *ctx, Tac_Instruction *instr);
char *new_temp(TacCtx *ctx);
Tac_Val *val_int(int v);
Tac_Val *val_float(float v);
Tac_Val *val_double(double v);
Tac_Val *val_var(const char *name);
Tac_Val *new_var_val(TacCtx *ctx);
Tac_Val *emit_cast(TacCtx *ctx, Tac_Val *src, const Type *from, const Type *to);
void emit_jump(TacCtx *ctx, const char *target);
void emit_label(TacCtx *ctx, const char *name);

//
// Type conversion (translate.c)
//
Tac_Type *ast_type_to_tac_type(const Type *t);

//
// Expression and statement lowering (translate_expr.c, translate_stmt.c)
//
Tac_Val *gen_expr(TacCtx *ctx, Expr *e);
void gen_stmt(TacCtx *ctx, Stmt *stmt);
void gen_compound_init(TacCtx *ctx, const char *var_name, int base_offset,
                       const Initializer *init);

//
// Convert the AST to TAC.
//
Tac_TopLevel *translate(ExternalDecl *ast);

#ifdef __cplusplus
}
#endif

#endif /* TRANSLATE_H */
