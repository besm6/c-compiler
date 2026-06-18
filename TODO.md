# TODO — Import the "Writing a C Compiler" test corpus

Convert the test suite under `tmp/tests/` (from Nora Sandler's *Writing a C Compiler*,
~1627 `.c` files across 20 chapters) into GoogleTest unit tests in our house style,
grouped per chapter and distributed across the right compiler component.

We do these tasks **one at a time**. A task is "done" when its new test files build and
the whole suite stays green (`make test`); programs the BESM-6 backend cannot yet handle
are committed as `DISABLED_` tests with a one-line note rather than left failing.

## Component mapping (by source subdirectory)

| Source subdir | Component | Test file | Style |
|---|---|---|---|
| `invalid_lex` | scanner | `scanner/chapterNN_tests.cpp` | `EXPECT_DEATH` on message |
| `invalid_parse` | parser | `parser/chapterNN_tests.cpp` | `EXPECT_DEATH` on message |
| `invalid_semantics`, `invalid_types`, `invalid_declarations`, `invalid_labels`, `invalid_struct_tags` | semantic | `semantic/chapterNN_tests.cpp` | `EXPECT_DEATH` on message |
| `valid` (+ `extra_credit`, `libraries`) | besm6 backend | `backend/besm6/chapterNN_tests.cpp` | `CompileAndRun`, compare output |
| ch19 `constant_folding` / `copy_propagation` / `dead_store_elimination` / `unreachable_code_elimination` | optimize | `optimize/chapter19_tests.cpp` | TAC-level assertions (+ run) |
| ch19 `whole_pipeline`, chapter_20 | besm6 backend | `backend/besm6/chapter19_tests.cpp`, `chapter20_tests.cpp` | `CompileAndRun` |

Naming: positive tests `Chapter3_FoldBinary`; negative tests get the **`Neg`** suffix,
e.g. `Chapter1_ReturnAtSign_Neg`. Negative tests assert on a **substring of the error
message** — improve the diagnostic first where it is cryptic or missing.

### Run-test wrapper (positive / BESM-6)

`CompileAndRun` captures program stdout; the libc startup calls `void program()`, so each
test wraps the book's `int main(void)`:

```c
int printf(const char *format, ...);
/* ... book program defining int main(void) ... */
void program(void) { printf("%d\n", main()); }
```

Expected value = stdout of the *same wrapped source* compiled with host `cc` (avoids
`mod 256` exit-code truncation). Multi-file `libraries` tests are concatenated into one
translation unit.

## Tasks

- [x] **Task 0 — Pilot + infrastructure (Chapter 1).** Wrote this `TODO.md`; added the
      scanner `lex_error()` path (`scanner/scan_fixture.h` helper) and `token_name()`, and
      readable parser "expected X, got Y" messages. Shared run-test wrapper in
      `backend/besm6/book_run.h` (`WrapMain`). Delivered `scanner/chapter1_tests.cpp`
      (5 lex), `parser/chapter1_tests.cpp` (12 parse),
      `backend/besm6/chapter1_tests.cpp` (7 valid). CMake wired; full suite green.
- [x] **Task 1 — Chapter 2** (Unary): delivered `parser/chapter2_tests.cpp` (7 parse)
      and `backend/besm6/chapter2_tests.cpp` (12 valid). CMake wired; all 19 pass, full
      suite green. No `DISABLED_` needed (negate/complement handled; `int` is 41-bit so
      every constant fits; `printf("%d")` prints negatives).
- [x] **Task 2 — Chapter 3** (Binary): delivered `parser/chapter3_tests.cpp` (8 parse),
      `semantic/chapter3_tests.cpp` (1 — `malformed_paren` "2 (- 3)" is a *parse* error in
      the book but a semantic one for us: our parser accepts a call on any postfix
      expression, so the type checker rejects it), and
      `backend/besm6/chapter3_tests.cpp` (15 valid + 11 ec). CMake wired; all 35 pass,
      full suite green. No `DISABLED_` needed — bitwise `&|^`/shifts all supported, and
      `-5 >> 30` is constant-folded with correct arithmetic-shift semantics before codegen.
- [x] **Task 3 — Chapter 4** (Logical/Relational): delivered `parser/chapter4_tests.cpp`
      (6 parse) and `backend/besm6/chapter4_tests.cpp` (33 valid + 4 ec). CMake wired; all
      43 pass, full suite green. No `DISABLED_` needed — `&&`/`||` short-circuit lowering
      skips the `1/0` divides, integer comparisons use b/eq..b/ge, and `!` is already
      supported.
- [x] **Task 4 — Chapter 5** (Local vars): delivered `parser/chapter5_tests.cpp`
      (12 + 4 ec), `semantic/chapter5_tests.cpp` (10 + 11 ec — first semantic tests),
      and `backend/besm6/chapter5_tests.cpp` (20 + 25 ec). CMake wired; full suite green.
      Fixed `is_lvalue` in [semantic/expressions.c](semantic/expressions.c) — a binary op
      was wrongly treated as an lvalue when its leftmost leaf was a var, so `a + 3 = 4` and
      `++(a+1)` died with a cryptic translator message instead of a clean type-checker
      diagnostic; no C binary operator yields an lvalue. Fixed a copy-prop use-after-free
      crash on deeply chained compound assignments (`x = a += b -= c *= …`): `CopyPair.src`
      borrowed a pointer into the instruction stream that substitution/self-copy removal
      later freed; the copy set now owns a deep copy of `src`
      ([optimize/copy_prop.c](optimize/copy_prop.c)). 3 `DISABLED_` valid tests
      (`empty_function_body`, `local_var_missing_return`, `null_statement`) — main lacks a
      return, so the value is UB and host cc vs BESM-6 disagree.
- [x] **Task 5 — Chapter 6** (if/conditional): delivered `scanner/chapter6_tests.cpp`
      (1 ec), `parser/chapter6_tests.cpp` (9 + 7 ec), `semantic/chapter6_tests.cpp`
      (3 + 5 ec), and `backend/besm6/chapter6_tests.cpp` (24 + 19 ec). CMake wired; full
      suite green. Added a goto/label resolution pass
      ([semantic/resolve_labels.c](semantic/resolve_labels.c), run in `typecheck_decl`):
      per function it rejects duplicate labels (`"Duplicate label"`) and goto to an
      undefined label (`"Undefined label"`, which also covers `goto` to a variable name
      since labels and variables are separate namespaces) — this lights up the
      `duplicate_labels` / `goto_missing_label` / `goto_variable` ec negatives. The
      `goto main` / `goto _main` label-vs-function-name programs run fine. 1 `DISABLED_`
      valid test: `binary_false_condition` (main falls off the end → indeterminate value
      in the WrapMain call context, like the ch5 missing-return cases). `multiple_if`
      initially failed (two back-to-back constant-condition if-else statements returned 5
      instead of 8); this exposed a dead-store-elimination miscompile — the backward
      liveness fixpoint skipped reachable-but-empty join blocks (left empty by
      unreachable-elim's jump/label cleanup), so a predecessor dropped a live else-branch
      store. Fixed in [optimize/dead_store.c](optimize/dead_store.c) by letting reachable
      empty blocks participate as identity nodes; `multiple_if` is enabled, with a TAC-level
      regression `DeadStoreLiveAcrossEmptyBlock` in
      [optimize/dead_store_tests.cpp](optimize/dead_store_tests.cpp).
- [x] **Task 6 — Chapter 7** (Compound stmts): delivered `parser/chapter7_tests.cpp`
      (4 parse), `semantic/chapter7_tests.cpp` (4 + 3 ec invalid, plus 10 shadowing),
      and `backend/besm6/chapter7_tests.cpp` (5 + 1 ec run). CMake wired; all 27 pass,
      full suite green. No `DISABLED_` needed. Chapter 7 is about variable shadowing in
      nested blocks, which we reject by the permanent no-shadowing design decision: 10 of
      the book's 16 "valid" programs redeclare an enclosing name and die with "Duplicate
      variable declaration" (full-scope `symtab_get_opt` lookup in
      [semantic/declarations.c](semantic/declarations.c)), so they became semantic
      negatives rather than run tests. The 6 programs that don't shadow run correctly,
      including sibling-scope same-name reuse (`multiple_vars_same_name`) and cross-block
      goto between siblings (`goto_sibling_scope`).
- [x] **Task 7 — Chapter 8** (Loops): delivered `parser/chapter8_tests.cpp` (19),
      `semantic/chapter8_tests.cpp` (27), and `backend/besm6/chapter8_tests.cpp` (49).
      CMake wired; full suite green. No compiler changes needed — for/while/do-while/
      break/continue/switch are all fully supported. Two reclassifications vs. the book:
      (1) `non_constant_case` is a *parse* error for us — a case value parses as a
      constant expression, so it moved to the parser file ("Expected constant
      expression"), making parser 10 + 8 ec + 1; (2) four programs the book lists as
      *valid* shadow an enclosing name in a nested block / for-init / case block
      (`for_shadow`, `for_nested_shadow`, `case_block`, `switch_decl`) and are rejected
      by the permanent no-shadowing rule, so they became semantic negatives
      ("Duplicate variable declaration"), making semantic 4 + 19 ec + 4 and besm6 valid
      19 + 30 ec. 1 `DISABLED_` run test (`empty_loop_body`): codegen is correct but its
      ~430M-iteration loop takes ~60s on the Dubna simulator, over the 10s ctest timeout.
      Duff's device, goto-into-switch, nested/fallthrough switches all run correctly.
- [ ] **Task 8 — Chapter 9 negative** (Functions): parser (11), semantic
      (declarations 9+4ec, labels 2ec, types 10+6ec).
- [ ] **Task 8b — Chapter 9 run:** besm6 valid (arguments_in_registers, no_arguments,
      stack_arguments, libraries, ec).
- [ ] **Task 9 — Chapter 10 negative**; **9b — Chapter 10 run** (file-scope / storage class).
- [ ] **Task 10 — Chapter 11 negative**; **10b — run** (Long integers; scanner 2 lex).
- [ ] **Task 11 — Chapter 12 negative**; **11b — run** (Unsigned; scanner 2 lex).
- [ ] **Task 12 — Chapter 13 negative**; **12b — run** (Floating-point; scanner 7 lex —
      expect many `DISABLED_`).
- [ ] **Task 13 — Chapter 14 negative**; **13b — run** (Pointers).
- [ ] **Task 14 — Chapter 15 negative**; **14b — run** (Arrays / pointer arithmetic).
- [ ] **Task 15 — Chapter 16 negative**; **15b — run** (Characters/strings; scanner 8 lex).
- [ ] **Task 16 — Chapter 17 negative**; **16b — run** (void / sizeof / dynamic alloc).
- [ ] **Task 17 — Chapter 18 negative**; **17b — run** (Structures/unions — largest set;
      expect `DISABLED_` for struct-by-value backend gaps).
- [ ] **Task 18 — Chapter 19** (optimize): `optimize/chapter19_tests.cpp` for
      constant_folding / copy_propagation / dead_store_elimination /
      unreachable_code_elimination (incl. `dont_*` negatives), plus `whole_pipeline` run.
- [ ] **Task 19 — Chapter 20** (register allocation): besm6 run tests (all_types,
      int_only, with/without coalescing, helper_libs).

## Verification

- Per component: `cd build/<comp> && ./<comp>-tests` (`parser-tests`, `scanner-tests`,
  `semantic/semantic-tests`, `optimize/optimizer-tests`).
- BESM-6 run tests: `cd build/backend/besm6 && ./besm-tests` (run from this dir for
  `libc.bin`; one `besm-tests` process at a time).
- Whole suite: `make test`.
