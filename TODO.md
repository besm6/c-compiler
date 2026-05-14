# What to do next

Development tasks for the `tacker` pipeline (`typecheck_global_decl` → `label_loops` →
`translate`). All work is in `translator/expr.c` unless noted otherwise. Effort is
**rough engineering time** for someone familiar with the tree (order-of-magnitude).

Tasks are listed in recommended implementation order. Each one builds on the previous.

---

## 6. Struct field read/write (`s.f`, `COPY_FROM/TO_OFFSET`)

**Depends on:** Tasks 1–3.

**Current behavior:** `EXPR_FIELD_ACCESS` calls `fatal_error`. `COPY_FROM_OFFSET` and
`COPY_TO_OFFSET` are never emitted.

**Work:**

Field offset lookup: `structtab_find(tag)->members`; walk the `FieldDef` linked list by
name to find `.offset`.

In `gen_expr()`, add `EXPR_FIELD_ACCESS`:

- If base is `EXPR_VAR`: emit `COPY_FROM_OFFSET dst ← s, offset`.
- Otherwise: `gen_lval(base)` → addr, `ADD_PTR addr2 ← addr, const_offset, 1`,
  `LOAD dst ← addr2`.

In `gen_lval()`, add `EXPR_FIELD_ACCESS`:

- If base is `EXPR_VAR`: emit `GET_ADDRESS tmp ← s`, then
  `ADD_PTR addr ← tmp, const_offset, 1`.
- Otherwise: `gen_lval(base)` → addr, `ADD_PTR addr2 ← addr, const_offset, 1`.

Assignment `s.f = val` falls through Task 3's dispatch. For the direct-variable case,
also emit `COPY_TO_OFFSET val → s, offset` as an optimization (avoids
`GET_ADDRESS` + `ADD_PTR` + `STORE`).

Tests in a new `translator/struct_tests.cpp`:

- `s.x` on rhs → `COPY_FROM_OFFSET`.
- `s.x = 5;` → `COPY_TO_OFFSET`.
- `&s.x` → `GET_ADDRESS` + `ADD_PTR`.

**Effort:** Medium (~4 h).

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
