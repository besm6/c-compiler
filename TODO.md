# What to do next

Development tasks for the `tacker` pipeline (`typecheck_global_decl` → `label_loops` →
`translate`). All work is in `translator/expr.c` unless noted otherwise. Effort is
**rough engineering time** for someone familiar with the tree (order-of-magnitude).

Tasks are listed in recommended implementation order. Each one builds on the previous.

---

## 7. Pointer-to-struct field access (`p->f`)

**Depends on:** Tasks 1–3, 6.

**Current behavior:** `EXPR_PTR_ACCESS` calls `fatal_error`.

**Work:**

In `gen_lval()`, add `EXPR_PTR_ACCESS`:

1. `gen_expr(ptr)` → ptr_val.
2. Look up field offset (same `structtab_find` walk as Task 6).
3. Emit `ADD_PTR addr ← ptr_val, const_offset, 1`.

In `gen_expr()`, add `EXPR_PTR_ACCESS`: call `gen_lval(e)` → addr, emit
`LOAD dst ← addr`. Assignment through `p->f` is handled by Task 3.

Tests (add to `translator/struct_tests.cpp`):

- `p->x` on rhs → `ADD_PTR` + `LOAD`.
- `p->x = 5;` → `ADD_PTR` + `STORE`.

**Effort:** Small (~2 h).
