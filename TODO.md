# What to do next

Development tasks for the `tacker` pipeline (`typecheck_global_decl` → `label_loops` →
`translate`). All work is in `translator/expr.c` unless noted otherwise. Effort is
**rough engineering time** for someone familiar with the tree (order-of-magnitude).

Tasks are listed in recommended implementation order. Each one builds on the previous.

---

## 1. `gen_lval()` foundation and address-of (`&x`)

**Current behavior:** `UNARY_ADDRESS` calls `fatal_error`. No `gen_lval` function exists.
The existing `lvalue_name()` helper only supports `EXPR_VAR`.

**Work:**

Add `gen_lval(TacCtx *ctx, Expr *e) → Tac_Val*` that returns a pointer-valued temp
holding the address of an lvalue. Initial support: `EXPR_VAR` only — emit
`GET_ADDRESS dst ← x`.

In `gen_expr()`, add the `UNARY_ADDRESS` case: delegate to `gen_lval(inner)` and return
the result.

Add tests in a new `translator/ptr_tests.cpp` (follow the `CompileToYaml` pattern):

- `int *p = &x;` → `GET_ADDRESS` instruction.

**Effort:** Small (~2 h).

---

## 2. Pointer dereference (`*p`)

**Depends on:** Task 1.

**Current behavior:** `UNARY_DEREF` calls `fatal_error`. `LOAD` is never emitted.

**Work:**

Extend `gen_lval()` with `UNARY_DEREF`: return `gen_expr(inner)` — the pointer value is
already the address.

Add `UNARY_DEREF` case in `gen_expr()`: call `gen_lval(e)` to get the address, emit
`LOAD dst ← src_ptr`.

Tests (add to `translator/ptr_tests.cpp`):

- `int y = *p;` → `LOAD`.
- `int y = **pp;` → two `LOAD`s (validates recursive `gen_lval`).

**Effort:** Small (~2 h).

---

## 3. Assignment and inc/dec through complex lvalues (`STORE`)

**Depends on:** Tasks 1–2.

**Current behavior:** `lvalue_name()` is called for assignment targets and
inc/dec operands; it calls `fatal_error` for anything other than `EXPR_VAR`.
`STORE` is never emitted.

**Work:**

Replace every call to `lvalue_name()` in `gen_expr()` with a dispatch:

- If target is `EXPR_VAR`: keep the existing `COPY` instruction (no change to output).
- Otherwise: call `gen_lval(target)` to get an address temp, emit `STORE` instead.

For compound assignment and inc/dec through a pointer, the read must go through `LOAD`
too. Pattern for `*p += val`:

1. `gen_lval(*p)` → addr
2. `LOAD t ← addr`
3. `BINARY t2 ← t op val`
4. `STORE t2 → addr`

Affected cases: `EXPR_ASSIGN` (simple and compound), `UNARY_PRE_INC`/`DEC`,
`EXPR_POST_INC`/`DEC`. Remove `lvalue_name()` once unused.

Tests (add to `translator/ptr_tests.cpp`):

- `*p = 5;` → `STORE`.
- `(*p)++;` → `LOAD` + `BINARY` + `STORE`.
- `*p += 3;` → `LOAD` + `BINARY` + `STORE`.

**Effort:** Medium (~4 h).

---

## 4. Array subscript (`a[i]`)

**Depends on:** Tasks 1–3.

**Current behavior:** `EXPR_SUBSCRIPT` calls `fatal_error`. `ADD_PTR` is never emitted.

**Work:**

Extend `gen_lval()` with `EXPR_SUBSCRIPT`:

1. Evaluate the base pointer/array with `gen_expr(base)`.
2. Evaluate the index with `gen_expr(index)`.
3. Compute `scale = get_size(element_type)` where `element_type` is the type of the
   subscript expression itself (the type-checker has already resolved it).
4. Emit `ADD_PTR dst ← ptr, index, scale`.

Add `EXPR_SUBSCRIPT` to `gen_expr()`: call `gen_lval(e)` then emit `LOAD`.
Assignment through `a[i]` is handled automatically by Task 3's dispatch.

Tests (add to `translator/ptr_tests.cpp`):

- `int y = a[i];` → `ADD_PTR` + `LOAD`.
- `a[i] = 5;` → `ADD_PTR` + `STORE`.
- `a[i]++;` → `ADD_PTR` + `LOAD` + `BINARY` + `STORE`.

**Effort:** Small–Medium (~3 h).

---

## 5. `sizeof` and `_Alignof` operators

**Current behavior:** `EXPR_SIZEOF_EXPR`, `EXPR_SIZEOF_TYPE`, and `EXPR_ALIGNOF` all
call `fatal_error`.

**Work:**

Add three cases in `gen_expr()` — all produce a compile-time integer constant (no
instructions emitted):

- `EXPR_SIZEOF_EXPR`: `val_int(get_size(e->u.sizeof_expr->type))`. The type-checker has
  already annotated the inner expression's type; there is no need to evaluate it.
- `EXPR_SIZEOF_TYPE`: `val_int(get_size(e->u.sizeof_type))`.
- `EXPR_ALIGNOF`: `val_int(get_alignment(e->u.alignof_type))`.

Tests (add to `translator/decl_tests.cpp` or a new file):

- `sizeof(int)` → constant 4.
- `sizeof(long)` → constant 8.
- `sizeof(x)` where `x` is `int` → constant 4.
- `_Alignof(double)` → constant 8.

**Effort:** Small (~2 h).

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
