# What to do next

Development tasks for the `tacker` pipeline (`typecheck_global_decl` → `label_loops` → `translate`). Effort is **rough engineering time** for someone familiar with the tree (order-of-magnitude; actual time varies with review and test depth).

---

## 1. Semantic validation for `switch`

**Current behavior:** Parser and AST support `switch` / `case` / `default`.
`label_loops()` sets a break target for `switch`. `typecheck.c` recursively checks
child statements but performs **no semantic validation**: case expressions are not checked
for being integer constants, duplicate case values are not detected, and multiple
`default` labels are not rejected.

**Concrete work** (in `typecheck.c`):

- Integer-promoted controlling expression.
- Constant integer case expressions.
- Duplicate case values rejected.
- At most one `default` per `switch`.

**Effort:** **Small** (~1–2 days) + tests.

---

## 2. Finish AST → TAC lowering (`translator/translate_gen.c`)

**Current behavior:** `gen_expr` handles `int`/`float` literals, `EXPR_VAR`, and
unary/binary arithmetic and comparison ops. All other expression kinds call
`fatal_error`: `EXPR_ASSIGN`, `EXPR_CALL`, `EXPR_CAST`, `EXPR_COND`,
`EXPR_SUBSCRIPT`, `EXPR_FIELD_ACCESS`, `EXPR_PTR_ACCESS`, `EXPR_SIZEOF_*`, and
char/string/enum literals. `gen_stmt` handles `if`, `while`, `do-while`, `for`
(expr-init only), `break`, `continue`, `return`, and labeled statements. Not yet
handled: compound blocks with local declarations (`DECL_OR_STMT_DECL`), `for` with
declaration init, `goto`, and `switch`/`case`/`default`. `translate_external_decl`
emits TAC only for function definitions; file-scope variable declarations produce no TAC.

**Concrete work (phased):**

- Expressions: assignment, calls, casts, lvalue ops (subscript, field access,
  pointer dereference) as needed by real code.
- Statements: local declarations in compound blocks, `for` with declaration init,
  `goto`.
- `switch` lowering: compare/jump chains for dispatching; labels for each
  `case`/`default`; `break` wired to the end label already produced by `label_loops()`.
- Optional: file-scope variable TAC (static init, string constants) if the backend
  needs them.

**Effort:** **Large to very large** (multiple weeks) if approaching "most of C";
**medium** (~1–2 weeks) for the next coherent slice (e.g. locals + calls + simple
memory ops) plus tests in the style of `typecheck_tests` or golden TAC output.

---

## Suggested order

1. Task **1** (`switch` semantics) — small and self-contained; unblocks correct testing of task 2.
2. Task **2** (TAC lowering) — broader; tackle in slices as the backend needs grow.
