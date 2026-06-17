//
// Internal types for translator.
//
#ifndef TRANSLATE_H
#define TRANSLATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ast.h"
#include "optimize.h"
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
    Tac_Param *locals;              // automatic local names, for the optimizer
    Tac_Param *locals_tail;         // tail of `locals` for O(1) append
    Tac_Param *array_locals;        // names of local arrays (block-scope symbols are
                                    // purged before lowering, so value decay needs this)
    const char *sret_name;          // hidden return-pointer param name when the current
                                    // function returns a multi-word struct by value; else NULL
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
void tac_record_local(TacCtx *ctx, const char *name);
void tac_record_array_local(TacCtx *ctx, const char *name);
bool tac_is_array_local(const TacCtx *ctx, const char *name);
Tac_Val *val_int(int64_t v);
Tac_Val *val_long(long v);
Tac_Val *val_long_long(long long v);
Tac_Val *val_uint(uint64_t v);
Tac_Val *val_ulong(unsigned long v);
Tac_Val *val_ulong_long(unsigned long long v);
Tac_Val *val_float(float v);
Tac_Val *val_double(double v);
Tac_Val *val_long_double(long double v);
Tac_Val *val_var(const char *name);
Tac_Val *new_var_val(TacCtx *ctx);
Tac_Val *emit_cast(TacCtx *ctx, Tac_Val *src, const Type *from, const Type *to);
void emit_jump(TacCtx *ctx, const char *target);
void emit_label(TacCtx *ctx, const char *name);

//
// Struct-by-value support (translate.c)
//
// One target machine word, in bytes (the unit a scalar return value occupies).
int target_word_bytes(void);
// True when `t` is a struct/union too large to return in a single word, so it uses the
// hidden-pointer (sret) calling convention.
bool type_is_byval_sret(const Type *t);
// Copy a whole struct/union value, word by word, from named aggregate `src_name`
// into `dst_name` at byte offset `dst_off`.  Both names denote frame-resident or
// global aggregates (the bases accepted by COPY_TO_OFFSET / COPY_FROM_OFFSET).
void gen_struct_assign(TacCtx *ctx, const char *dst_name, int dst_off, const char *src_name,
                       int nbytes);

//
// Type conversion (translate.c)
//
Tac_Type *ast_type_to_tac_type(const Type *t);

//
// Expression and statement lowering (translate_expr.c, translate_stmt.c)
//
Tac_Val *gen_expr(TacCtx *ctx, Expr *e);
void gen_stmt(TacCtx *ctx, Stmt *stmt);
void gen_compound_init(TacCtx *ctx, const char *var_name, int base_offset, const Initializer *init);

//
// Convert one external declaration to TAC and optimize each function it yields.
// Each function self-describes its params and automatic locals, so the optimizer
// classifies locals vs. globals per function — no whole-program context needed.
//
Tac_TopLevel *translate(const ExternalDecl *ast, OptFlags flags);

#ifdef __cplusplus
}
#endif

#endif /* TRANSLATE_H */
