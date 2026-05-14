# What to do next

Development tasks for the `tacker` pipeline (`typecheck_global_decl` → `label_loops` →
`translate`). All work is in `translator/translate.c` unless noted otherwise. Effort is
**rough engineering time** for someone familiar with the tree (order-of-magnitude).

Tasks are listed in recommended implementation order. Each one builds on the previous.

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
