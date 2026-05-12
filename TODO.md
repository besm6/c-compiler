# What to do next

Development tasks for the `tacker` pipeline (`resolve` → `typecheck_global_decl` → `label_loops` → `translate`). Effort is **rough engineering time** for someone familiar with the tree (order-of-magnitude; actual time varies with review and test depth).

---

## ~~1. Finish `resolve()` for file-scope variables~~ ✅ Done

**Completed work:**

- Fixed file-scope `DECL_VAR` to loop over **all** `InitDeclarator` nodes in both
  `resolve()` (`resolve.c`) and `typecheck_file_scope_var_decl()` (`typecheck.c`).
  Previously only the first declarator was processed; `int x = 1, y = 2;` silently
  ignored `y`.
- Added a **function-type branch** in `typecheck_file_scope_var_decl()`: a file-scope
  declarator with `TYPE_FUNCTION` (i.e., a function prototype written as a bare
  declaration such as `int f(int);`) is now registered via `symtab_add_fun()` instead
  of `symtab_add_static_var()`, so `has_linkage` is set correctly and the subsequent
  function definition is not rejected as a duplicate.
- Symtab insertion for file-scope variables remains in `typecheck_file_scope_var_decl()`
  (handles linkage, redeclaration, and TAC init correctly); `resolve()` only runs
  `resolve_type()` + `resolve_initializer()` per declarator.

---

## 2. Merge `resolve()` with typecheck (`translator/typecheck.c`)

**Current behavior:** Two passes over the same AST: `resolve()` then
`typecheck_global_decl()`. The concrete duplication bugs that caused crashes have been
fixed (see below), but the passes are still separate.

**Fixed in this session (no longer blocking):**

- **Struct double-registration crash** — `resolve_struct_decl()` previously called
  `typetab_add_struct()`, then `typecheck_struct_decl()` called `validate_struct_definition()`
  which called `typetab_exists()` → always fataled "Structure X was already declared".
  Fixed: `resolve_struct_decl()` now only validates member types; `typecheck_struct_decl()`
  remains the sole registrar.
- **`has_linkage` not set on functions** — `symtab_add_fun()` never set
  `sym->has_linkage = true`, so a prototype followed by a definition crashed with
  "Duplicate declaration". Fixed.
- **`f(void)` void sentinel** — the parser represents `f(void)` as a single unnamed
  `Param` with `TYPE_VOID`. `typecheck_fn_decl()` now strips it (treating the function
  as having no parameters) instead of fataling. `resolve_function_declaration()` now
  skips unnamed params when calling `symtab_add_automatic_var_type`.
- **resolve_function_declaration() placeholder** — previously inserted `defined=true`;
  changed to `defined=false` so `typecheck_fn_decl()` is the sole authority on whether
  a function has a definition (prevents "Defined function X twice").

**Remaining work:**

- Single traversal that performs name resolution and type checking where dependencies
  allow (or a clearly ordered single function per construct).
- After merge, drop redundant walks or share helpers between former `resolve_*` and
  `typecheck_*` entry points.

**Effort:** **Large** (~1–2 weeks): refactor + parity tests against current
`typecheck_tests.cpp` behavior.

---

## 3. Scope level for locals and function parameters

**Current behavior:** `scope_level` in `translator.c` is already used: `symtab_purge` /
`typetab_purge` on leaving compound statements and `for` blocks; parameters use
`symtab_add_automatic_var_type(..., scope_level)` after `scope_increment()` inside
`resolve_function_declaration()`.

**Known pre-existing bug:** `map_remove_level()` in `string_map.c` frees the AVL node
structs but does **not** call the dealloc callback on `node->value`, leaking `Symbol`
pointers when `symtab_purge()` removes block-scope entries. This is observable: a
function like `int f(int n) { return n; }` leaks the `Symbol` for `n` because it is
added at scope level 1 by `resolve()` and purged (without being freed) on scope exit,
then re-added at level 0 by `typecheck_fn_decl()` (which is freed by `symtab_destroy`).

**Gaps vs typical C:**

- **Duplicate names in the same scope** are rejected with `fatal_error` (no shadowing);
  inner blocks cannot reuse an identifier from an outer scope in the way C allows.
- **Typedef** names and ordinary identifiers share the same resolution story only after
  parser `nametab`; the translator has **no** `TYPE_TYPEDEF_NAME` handling in
  `resolve_type` / `validate_type` yet (see task 6).

**Concrete work:**

- Fix the `map_remove_level()` dealloc callback bug in `string_map.c` (call the
  registered callback on `node->value` before freeing the node).
- Decide target: full C11 scoping (shadowing, tag namespaces) vs incremental fixes.
- Extend `string_map` level semantics or symbol lookup so inner scopes can shadow outer
  bindings where required.
- Optionally align **struct tags** with block scope rules in concert with `typetab_purge`.

**Effort:** **Medium to large** (~3–10 days) depending on whether you only fix shadowing
or also full namespace rules. The `map_remove_level` leak fix alone is **Small** (~1 day).

---

## 4. Finish `label_loops()` and unit tests (`translator/translator.c`)

**Current behavior:** `label_loops()` is **implemented** (not a stub): it assigns
`loop_end_label` / `loop_continue_label` on loops and `switch`, and fills
`branch_target_label` for `break` / `continue`. `translate_gen.c` checks for missing
labels (e.g. `while`).

**Remaining work:**

- **Automated tests:** There is no dedicated translator test for loop/switch labeling in
  the active test tree (older `translator/attic/resolve_*` tests exist but are not the
  main harness). Add tests that build a small AST or parse a snippet, run `label_loops`,
  and assert labels on `Stmt` nodes.
- Edge cases: `break` in `switch`, nested loops/switches, invalid `continue` outside a
  loop (already `fatal_error`), `default` / `case` nesting.

**Effort:** **Small** (~1–2 days) for tests + small fixes if gaps appear; update
`docs/TECHNICAL.md` where it still calls `label_loops` a stub.

---

## 5. Finish AST → TAC lowering (`translator/translate_gen.c`)

**Current behavior:** Documented as an **initial subset**. `gen_expr` supports literals,
`EXPR_VAR`, unary/binary ops only; many expression kinds `fatal_error`. `gen_stmt`
rejects compound blocks with local declarations, `switch` / `case` / `default`, `goto`;
`for` only supports `FOR_INIT_EXPR`, not declaration init. `translate_external_decl`
only emits TAC for **function definitions** with a body (no file-scope variable TAC).

**Concrete work (phased):**

- Expressions: assignment, calls, casts, lvalues (fields, `*`, etc.) as needed by real
  code.
- Statements: locals (stack/slots), `for` with declaration init, `switch` lowering
  (task 7).
- Optional: non-function top-level (static init, string constants) if the backend needs
  them.

**Effort:** **Large to very large** (multiple weeks) if approaching "most of C";
**medium** (~1–2 weeks) for the next coherent slice (e.g. locals + calls + simple memory
ops) plus tests in the style of `typecheck_tests` or golden TAC output.

---

## 6. Implement `typedef` in the translator

**Current behavior:** The **parser** handles `typedef` and `TYPE_TYPEDEF_NAME` /
`TYPE_SPEC_TYPEDEF_NAME` in the AST. The **translator** does not: `resolve_type` has no
`TYPE_TYPEDEF_NAME` case; `validate_type` treats unknown kinds as
`fatal_error("Unsupported type kind")`.

**Concrete work:**

- A scoped map from typedef name → `Type` (could extend `typetab` or add a dedicated
  table with `typetab_purge`-style scope levels).
- On typedef declarations: register aliases when walking declarations (similar to how
  structs are registered).
- In `resolve_type` / `validate_type` / size and alignment helpers: resolve
  `TYPE_TYPEDEF_NAME` to the underlying type.

**Effort:** **Medium** (~3–6 days) including scope and tests.

---

## 7. Implement `switch` (semantic + TAC)

**Current behavior:** Parser and AST support `switch` / `case` / `default`.
`label_loops()` sets a break target for `switch`. `translate_gen.c` treats `STMT_SWITCH`,
`STMT_CASE`, and `STMT_DEFAULT` as **not implemented** (`fatal_error`). No dedicated
lowering to compare dispatch value against case constants and jump.

**Concrete work:**

- **Semantic checks** (likely in typecheck): integer-promoted controlling expression,
  constant case expressions, duplicate case values, `default` at most once.
- **TAC:** generate compare/jump chains or a jump table strategy; attach labels to each
  `case`/`default`; wire `break` to the switch end label already produced by
  `label_loops()`.

**Effort:** **Medium** (~3–7 days) for a first correct lowering + tests.

---

## Suggested order

1. Task **4** (tests + doc accuracy) — low risk, locks in behavior.
2. Fix `map_remove_level` dealloc bug (task **3** prerequisite) — small, eliminates a
   known leak before adding more scoped symbols.
3. Task **2** — reduce resolve/typecheck duplication before adding typedef/switch widely.
4. Task **6** (`typedef`) — unblocks many real headers.
5. Task **3** (scoping) as needed for correctness vs. real code.
6. Task **7** then **5** for TAC (switch depends on solid lowering infrastructure).
