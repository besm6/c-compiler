# What to do next

Development tasks for the `tacker` pipeline (`typecheck_global_decl` → `label_loops` →
`translate`). All work is in `translator/translate.c` unless noted otherwise. Effort is
**rough engineering time** for someone familiar with the tree (order-of-magnitude).

Tasks are listed in recommended implementation order. Each one builds on the previous.

---

## 6. Type casts and numeric conversions

**Current behavior:** `EXPR_CAST` calls `fatal_error`. The TAC instructions
`SIGN_EXTEND`, `TRUNCATE`, `ZERO_EXTEND`, `DOUBLE_TO_INT`, `DOUBLE_TO_UINT`,
`INT_TO_DOUBLE`, `UINT_TO_DOUBLE` exist but are never emitted.

**Concrete work:**

- Add `emit_cast(TacCtx *ctx, Tac_Val *src, const Type *from, const Type *to)` that
  dispatches on the (from, to) type pair using `get_size()`, `is_signed()`,
  `is_arithmetic()` from `typecheck.c` (all of which already resolve typedef names):
  - Wider signed integer → narrower integer: `TRUNCATE`
  - Narrower signed integer → wider signed integer: `SIGN_EXTEND`
  - Narrower unsigned integer → wider integer: `ZERO_EXTEND`
  - Integer → double: `INT_TO_DOUBLE` or `UINT_TO_DOUBLE` depending on signedness
  - Double → integer: `DOUBLE_TO_INT` or `DOUBLE_TO_UINT` depending on signedness
  - Same-size / same type: `COPY` or return `src` unchanged
- Wire `EXPR_CAST` to call `emit_cast(ctx, gen_expr(inner), inner->type, e->u.cast.type)`.
- Note on implicit promotions: the typecheck pass annotates `e->type` on every node but
  does not insert explicit `EXPR_CAST` wrappers for implicit conversions (e.g., `char →
  int` in arithmetic). For now those are handled by the backend; document the gap and
  decide later whether to insert cast nodes in `typecheck.c` or handle them here.

**Effort:** Medium (2–3 days; the type-pair dispatch table has many cases).

---

## 7. Pre/post increment and decrement

**Current behavior:** `UNARY_PRE_INC` and `UNARY_PRE_DEC` hit `map_unary_op`'s
`default` and call `fatal_error`. `EXPR_POST_INC` and `EXPR_POST_DEC` call `fatal_error`.

**AST nodes:** `e->u.unary_op.expr` for all four forms.

**Concrete work:**

- Add cases in `gen_expr` before calling `gen_unary`.
- Post-increment `x++`:
  1. `COPY t.old ← x`
  2. `t.new = BINARY ADD x, 1`
  3. `COPY x ← t.new`
  4. Return `t.old`
- Pre-increment `++x`:
  1. `t.new = BINARY ADD x, 1`
  2. `COPY x ← t.new`
  3. Return `t.new`
- Decrement variants use `SUBTRACT` in place of `ADD`.
- Pointer operands need `ADD_PTR` with scale = `sizeof(*ptr)` — defer to task 9 using
  the `lvalue_name` helper (already in translate.c), which will `fatal_error` on
  non-variable operands until then.

**Effort:** Small (half a day including tests).

---

## 8. `switch`/`case`/`default` lowering

**Current behavior:** `STMT_SWITCH`, `STMT_CASE`, and `STMT_DEFAULT` all call
`fatal_error`. The semantic validation (type checking, duplicate-case detection) is
already complete in `typecheck.c`. `label_loops()` already assigns `loop_end_label` to
every `STMT_SWITCH` and wires `break` to it.

**Strategy:** Linear compare-and-jump chain. Correct and simple; a jump-table
optimisation can follow later.

**Concrete work:**

1. Add `collect_cases(Stmt *body, CaseList *out)` — a recursive pre-pass that walks the
   body and collects each `STMT_CASE` node (its constant integer value and a freshly
   generated label) plus any `STMT_DEFAULT` node. Store the assigned labels back onto
   the AST nodes using the `branch_target_label` field (currently unused for
   case/default).
2. In `gen_stmt` for `STMT_SWITCH`:
   a. Evaluate the controlling expression into `t.ctrl`.
   b. For each collected case: `t.cmp = BINARY EQUAL t.ctrl, case_val`, then
      `JUMP_IF_NOT_ZERO t.cmp → case_label`.
   c. If a default was found: `JUMP → default_label`; otherwise `JUMP → loop_end_label`.
   d. Call `gen_stmt` on the body (case/default labels are emitted inline below).
   e. Emit `LABEL loop_end_label`.
3. In `gen_stmt` for `STMT_CASE`: emit `LABEL branch_target_label`, then recurse into
   the inner statement.
4. In `gen_stmt` for `STMT_DEFAULT`: emit `LABEL branch_target_label`, recurse.

`STMT_BREAK` inside a switch already emits a jump to `loop_end_label` (wired by
`label_loops`), so fall-through and explicit breaks both work correctly.

**Effort:** Medium (2–3 days; the `collect_cases` pre-pass is the most involved part).

---

## 9. Pointers, arrays, and struct field access

**Current behavior:** `UNARY_ADDRESS`, `UNARY_DEREF`, `EXPR_SUBSCRIPT`,
`EXPR_FIELD_ACCESS`, and `EXPR_PTR_ACCESS` all call `fatal_error`. The TAC instructions
`GET_ADDRESS`, `LOAD`, `STORE`, `ADD_PTR`, `COPY_TO_OFFSET`, and `COPY_FROM_OFFSET`
exist but are never emitted.

**Design:** This task requires distinguishing lvalue and rvalue context throughout
`gen_expr`. The cleanest approach is two functions:

- `gen_rval(TacCtx *ctx, Expr *e) → Tac_Val*` — current `gen_expr` behaviour (load the
  value).
- `gen_lval(TacCtx *ctx, Expr *e) → Tac_Val*` — returns a pointer-valued temp holding
  the address of the lvalue; error if `e` is not addressable.

Rename the current `gen_expr` to `gen_rval` and replace callers.

**Concrete work (in dependency order):**

1. `UNARY_ADDRESS` (`&x`): delegate to `gen_lval(x)`. For `EXPR_VAR`, emit
   `GET_ADDRESS dst ← x`. Return the pointer temp.
2. `UNARY_DEREF` (`*p`): in rvalue context, call `gen_rval(p)`, emit `LOAD dst ← src_ptr`.
   In lvalue context (`gen_lval`), just return `gen_rval(p)` — the pointer value is
   already the address.
3. `EXPR_SUBSCRIPT` (`a[i]`): compute `ADD_PTR(base, index, scale)` where
   `scale = get_size(element_type)`, then `LOAD` the result. In lvalue context, return
   the computed address without the load.
4. `EXPR_FIELD_ACCESS` (`s.f`): compute the byte offset of field `f` from the struct
   layout (use `get_size` / `get_alignment` to replicate the same layout logic as in
   `typecheck.c`). Emit `COPY_FROM_OFFSET`. In lvalue context, emit `GET_ADDRESS s`
   then `ADD_PTR` with the field offset.
5. `EXPR_PTR_ACCESS` (`p->f`): evaluate `p` with `gen_rval`, then apply the same
   field-offset logic via `LOAD` + offset.
6. Revisit tasks 2 and 8: expand `lvalue_name` into a full `gen_lval`, and emit `STORE`
   (instead of `COPY`) when the assignment or inc/dec target resolves to a pointer.

**Effort:** Large (1–2 weeks; struct layout, pointer arithmetic, and lvalue plumbing
interact across many expression kinds).
