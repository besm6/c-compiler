# What to do next

Development tasks for the `tacker` pipeline (`typecheck_global_decl` → `label_loops` → `translate`). Effort is **rough engineering time** for someone familiar with the tree (order-of-magnitude; actual time varies with review and test depth).

---

## 1. Scope level for locals and function parameters

**Current behavior:** `scope_level` in `typecheck.c` is already used: `symtab_purge` /
`typetab_purge` on leaving compound statements and `for` blocks; parameters use
`symtab_add_automatic_var_type(..., scope_level)` after `scope_increment()` inside
`typecheck_fn_decl()`. The `map_remove_level` dealloc bug has been fixed:
`symtab_purge` and `typetab_purge` now call `map_remove_level_free`, which invokes the
registered callback on each evicted node's value.

**Gaps vs typical C:**

- **Duplicate names in the same scope** are rejected with `fatal_error` (no shadowing);
  inner blocks cannot reuse an identifier from an outer scope in the way C allows.
- **Typedef** names and ordinary identifiers share the same resolution story only after
  parser `nametab`; the translator has **no** `TYPE_TYPEDEF_NAME` handling in
  `validate_type` yet (see task 3).

**Concrete work:**

- Decide target: full C11 scoping (shadowing, tag namespaces) vs incremental fixes.
- Extend `string_map` level semantics or symbol lookup so inner scopes can shadow outer
  bindings where required.
- Optionally align **struct tags** with block scope rules in concert with `typetab_purge`.

**Effort:** **Medium to large** (~3–10 days) depending on whether you only fix shadowing
or also full namespace rules.

---

## 2. Finish AST → TAC lowering (`translator/translate_gen.c`)

**Current behavior:** Documented as an **initial subset**. `gen_expr` supports literals,
`EXPR_VAR`, unary/binary ops only; many expression kinds `fatal_error`. `gen_stmt`
rejects compound blocks with local declarations, `switch` / `case` / `default`, `goto`;
`for` only supports `FOR_INIT_EXPR`, not declaration init. `translate_external_decl`
only emits TAC for **function definitions** with a body (no file-scope variable TAC).

**Concrete work (phased):**

- Expressions: assignment, calls, casts, lvalues (fields, `*`, etc.) as needed by real
  code.
- Statements: locals (stack/slots), `for` with declaration init, `switch` lowering
  (task 5).
- Optional: non-function top-level (static init, string constants) if the backend needs
  them.

**Effort:** **Large to very large** (multiple weeks) if approaching "most of C";
**medium** (~1–2 weeks) for the next coherent slice (e.g. locals + calls + simple memory
ops) plus tests in the style of `typecheck_tests` or golden TAC output.

---

## 3. Implement `typedef` in the translator

**Current behavior:** The **parser** handles `typedef` and `TYPE_TYPEDEF_NAME` /
`TYPE_SPEC_TYPEDEF_NAME` in the AST. The **translator** does not: `validate_type` treats
unknown kinds as `fatal_error("Unsupported type kind")`.

**Concrete work:**

- A scoped map from typedef name → `Type` (could extend `typetab` or add a dedicated
  table with `typetab_purge`-style scope levels).
- On typedef declarations: register aliases when walking declarations (similar to how
  structs are registered).
- In `validate_type` / size and alignment helpers: resolve `TYPE_TYPEDEF_NAME` to the
  underlying type.

**Effort:** **Medium** (~3–6 days) including scope and tests.

---

## 4. Implement `switch` (semantic + TAC)

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

1. Task **3** (`typedef`) — unblocks many real headers.
2. Task **1** (scoping) as needed for correctness vs. real code.
3. Task **4** then **2** for TAC (switch depends on solid lowering infrastructure).
