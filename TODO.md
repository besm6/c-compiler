# What to do next

Development tasks for the `tacker` pipeline (`resolve` → `typecheck_global_decl` → `label_loops` → `translate`). Effort is **rough engineering time** for someone familiar with the tree (order-of-magnitude; actual time varies with review and test depth).

---

## 1. Finish `resolve()` for file-scope variables (`translator/resolve.c`)

**Current behavior:** `resolve()` registers functions (`symtab_add_fun`), locals, and parameters, and resolves expressions against `symtab`. For **file-scope** `DECL_VAR`, the body still has a placeholder: `symtab_add` is not called (see comment near `symbol_table_insert`), and only `resolve_type` / `resolve_initializer` run.

**Why it matters:** Globals are inserted later in `typecheck_file_scope_var_decl()` (`typecheck.c`). That works for **later** declarations because `typecheck` runs after `resolve` on each prior decl, but keeping insertion only in `typecheck` splits responsibility and makes `resolve()` incomplete as a “bind names” pass.

**Concrete work:**

- Insert file-scope variables into `symtab` from `resolve()` (likely `symtab_add_static_var` or a thin wrapper), matching linkage rules already handled in `typecheck_file_scope_var_decl` (extern, static, tentative vs initialized, redeclaration).
- Avoid double insertion or conflicting rules once task 2 is tackled; may require moving linkage/init logic to one place.

**Effort:** **Medium** (~2–4 days): implementation + alignment with existing `typecheck` behavior + regression tests.

---

## 2. Merge `resolve()` with typecheck (`translator/typecheck.c`)

**Current behavior:** Two passes over the same AST: `resolve()` then `typecheck_global_decl()`. Struct handling is **duplicated**: `resolve_struct_decl()` and `typecheck_struct_decl()` both compute layout and call `typetab_add_struct()` (with `validate_struct_definition` only on the typecheck side).

**Concrete work:**

- Single traversal that performs name resolution and type checking where dependencies allow (or a clearly ordered single function per construct).
- Deduplicate struct/union layout and `typetab` registration; keep one authoritative path for `validate_type` / `validate_struct_definition`.
- After merge, drop redundant walks or share helpers between former `resolve_*` and `typecheck_*` entry points.

**Effort:** **Large** (~1–2 weeks): refactor + parity tests against current `typecheck_tests.cpp` behavior.

---

## 3. Scope level for locals and function parameters

**Current behavior:** `scope_level` in `translator.c` is already used: `symtab_purge` / `typetab_purge` on leaving compound statements and `for` blocks; parameters use `symtab_add_automatic_var_type(..., scope_level)` after `scope_increment()` inside `resolve_function_declaration()`.

**Gaps vs typical C:**

- **Duplicate names in the same scope** are rejected with `fatal_error` (no shadowing); inner blocks cannot reuse an identifier from an outer scope in the way C allows.
- **Typedef** names and ordinary identifiers share the same resolution story only after parser `nametab`; the translator has **no** `TYPE_TYPEDEF_NAME` handling in `resolve_type` / `validate_type` yet (see task 6).

**Concrete work:**

- Decide target: full C11 scoping (shadowing, tag namespaces) vs incremental fixes.
- Extend `string_map` level semantics or symbol lookup so inner scopes can shadow outer bindings where required.
- Optionally align **struct tags** with block scope rules in concert with `typetab_purge`.

**Effort:** **Medium to large** (~3–10 days) depending on whether you only fix shadowing or also full namespace rules.

---

## 4. Finish `label_loops()` and unit tests (`translator/translator.c`)

**Current behavior:** `label_loops()` is **implemented** (not a stub): it assigns `loop_end_label` / `loop_continue_label` on loops and `switch`, and fills `branch_target_label` for `break` / `continue`. `translate_gen.c` checks for missing labels (e.g. `while`).

**Remaining work:**

- **Automated tests:** There is no dedicated translator test for loop/switch labeling in the active test tree (older `translator/attic/resolve_*` tests exist but are not the main harness). Add tests that build a small AST or parse a snippet, run `label_loops`, and assert labels on `Stmt` nodes.
- Edge cases: `break` in `switch`, nested loops/switches, invalid `continue` outside a loop (already `fatal_error`), `default` / `case` nesting.

**Effort:** **Small** (~1–2 days) for tests + small fixes if gaps appear; update `docs/TECHNICAL.md` where it still calls `label_loops` a stub.

---

## 5. Finish AST → TAC lowering (`translator/translate_gen.c`)

**Current behavior:** Documented as an **initial subset**. `gen_expr` supports literals, `EXPR_VAR`, unary/binary ops only; many expression kinds `fatal_error`. `gen_stmt` rejects compound blocks with local declarations, `switch` / `case` / `default`, `goto`; `for` only supports `FOR_INIT_EXPR`, not declaration init. `translate_external_decl` only emits TAC for **function definitions** with a body (no file-scope variable TAC).

**Concrete work (phased):**

- Expressions: assignment, calls, casts, lvalues (fields, `*`, etc.) as needed by real code.
- Statements: locals (stack/slots), `for` with declaration init, `switch` lowering (task 7).
- Optional: non-function top-level (static init, string constants) if the backend needs them.

**Effort:** **Large to very large** (multiple weeks) if approaching “most of C”; **medium** (~1–2 weeks) for the next coherent slice (e.g. locals + calls + simple memory ops) plus tests in the style of `typecheck_tests` or golden TAC output.

---

## 6. Implement `typedef` in the translator

**Current behavior:** The **parser** handles `typedef` and `TYPE_TYPEDEF_NAME` / `TYPE_SPEC_TYPEDEF_NAME` in the AST. The **translator** does not: `resolve_type` has no `TYPE_TYPEDEF_NAME` case; `validate_type` treats unknown kinds as `fatal_error("Unsupported type kind")`.

**Concrete work:**

- A scoped map from typedef name → `Type` (could extend `typetab` or add a dedicated table with `typetab_purge`-style scope levels).
- On typedef declarations: register aliases when walking declarations (similar to how structs are registered).
- In `resolve_type` / `validate_type` / size and alignment helpers: resolve `TYPE_TYPEDEF_NAME` to the underlying type.

**Effort:** **Medium** (~3–6 days) including scope and tests.

---

## 7. Implement `switch` (semantic + TAC)

**Current behavior:** Parser and AST support `switch` / `case` / `default`. `label_loops()` sets a break target for `switch`. `translate_gen.c` treats `STMT_SWITCH`, `STMT_CASE`, and `STMT_DEFAULT` as **not implemented** (`fatal_error`). No dedicated lowering to compare dispatch value against case constants and jump.

**Concrete work:**

- **Semantic checks** (likely in typecheck): integer-promoted controlling expression, constant case expressions, duplicate case values, `default` at most once.
- **TAC:** generate compare/jump chains or a jump table strategy; attach labels to each `case`/`default`; wire `break` to the switch end label already produced by `label_loops()`.

**Effort:** **Medium** (~3–7 days) for a first correct lowering + tests.

---

## Suggested order

1. Task **4** (tests + doc accuracy) — low risk, locks in behavior.
2. Task **1** + **2** — reduce duplication before adding typedef/switch widely.
3. Task **6** (`typedef`) — unblocks many real headers.
4. Task **3** (scoping) as needed for correctness vs. real code.
5. Task **7** then **5** for TAC (switch depends on solid lowering infrastructure).
