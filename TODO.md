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
- [x] **Task 8 — Chapter 9 negative** (Functions): delivered `parser/chapter9_tests.cpp`
      (8) and `semantic/chapter9_tests.cpp` (34). CMake wired; full suite green (1734).
      11 of the 42 book negatives produced *no* diagnostic, so this added seven compiler
      checks: (1) `validate_type` rejects a function returning a function/array
      ([semantic/typecheck.c](semantic/typecheck.c)); (2) an initializer on a function
      declaration; (3) a function declaration in a `for` header
      ([semantic/statements.c](semantic/statements.c)); (4) duplicate parameter names
      (`check_duplicate_params`, both prototypes and definitions); (5) a function
      designator as the operand of `++`/`--`/compound assignment
      (`is_function_designator` in [semantic/expressions.c](semantic/expressions.c),
      checked before function→pointer decay); (6) local (block-scope) function
      declarations are now registered as `SYM_FUNC` like file-scope prototypes
      (`register_function_declaration` in [semantic/declarations.c](semantic/declarations.c))
      — `symtab_add_fun` already stores at file scope (external linkage), which fixes the
      `multiple_function_definitions_2` symbol-table corruption and makes cross-function
      conflicts visible; and (7) conflicting redeclarations via `compatible_type`
      ("Conflicting declarations for function"). Reclassifications vs. the book:
      `nested_function_definition` is a *parse* error for us (→ parser file);
      `call_non_identifier`, `function_returning_function`, `initialize_function_as_variable`
      and `fun_decl_for_loop` parse cleanly (the grammar permits the declarations) and are
      rejected by the type checker (→ semantic file); `call_variable_as_function` is caught
      first by the permanent no-shadowing rule (`int x = 0;` shadowing function `x`) as
      "Duplicate variable declaration". No `DISABLED_` needed.
- [x] **Task 8b — Chapter 9 run** (Functions): delivered `backend/besm6/chapter9_tests.cpp`
      (23 run + 2 `DISABLED_`) and extended `semantic/chapter9_tests.cpp` with 4 shadowing
      negatives (38 total). CMake wired; full suite green (1763). Multi-argument calls,
      recursion, argument order/preservation, and per-function goto-label mangling all work
      as-is (BESM-6 passes every argument through the r6 parameter block, so the book's x86
      register-vs-stack distinction is moot). Two compiler fixes were needed: (1) a
      block-scope function declaration (`int f(void);` inside a body) was being recorded as
      an automatic local and percent-prefixed to `%f`, so the call no longer matched the
      file-scope definition (`OTCYTCTBYET *F`); `translate_local_decl` in
      [translator/stmt.c](translator/stmt.c) now skips `TYPE_FUNCTION` declarators
      (fixes `multiple_declarations`, `param_shadows_local_var`); (2) a parameter shadowing
      a file-scope function silently overwrote the function's symtab entry (string_map keys
      by name only) and later miscompiled to a cryptic "Symbol not found", so the
      function-definition param loop in [semantic/declarations.c](semantic/declarations.c)
      now rejects a parameter shadowing an enclosing name with "Duplicate variable
      declaration". Reclassifications vs. the book: four `valid/` programs shadow an
      enclosing name and become semantic negatives under the no-shadowing rule
      (`function_shadows_variable`, `variable_shadows_function`, `parameter_shadows_function`,
      `parameter_shadows_own_function`). The book's `putchar` is not in our libc; the three
      programs that use it call the existing libc `putch` instead, and `hello_world` prints
      uppercase letters (the BESM-6 GOST output charset renders lowercase Latin as Cyrillic).
      2 `DISABLED_`: `stack_alignment` (depends on external x86 `.s` helpers) and
      `test_for_memory_leaks` (10M-iteration loop, over the 10s ctest timeout).
- [x] **Task 9 — Chapter 10 negative** (file-scope / storage class): delivered
      `parser/chapter10_tests.cpp` (11) and `semantic/chapter10_tests.cpp`
      (21 + 2 `DISABLED_`). CMake wired; full suite green (1795). Chapter 10's
      storage-class/linkage machinery was already largely present, so most
      negatives already diagnosed; this added five checks for programs that were
      silently accepted: (1) the parser rejects more than one storage-class
      specifier ("Multiple storage class specifiers", [parser/decl.c](parser/decl.c)
      `parse_declaration_specifiers`) — covers `multi_storage_class_fun/var`,
      `static_and_extern`; (2) the parser rejects a storage class on a parameter
      other than `register` ("A function parameter cannot have a storage class",
      `parse_parameter_declaration`) — covers `extern_param`, `static_param`;
      (3) a block-scope `extern` following a same-scope no-linkage declaration
      (`SYM_LOCAL` local/param) → "Identifier ... declared both with and without
      linkage" ([semantic/declarations.c](semantic/declarations.c)
      `typecheck_local_var_decl`) — covers `extern_follows_local_var`,
      `redefine_param_as_identifier_with_linkage`; (4) a static local
      redeclaring a same-scope name → "Duplicate variable declaration"; and
      (5) `static` on a block-scope function declaration → "Block-scope function
      declaration cannot be static" (`register_function_declaration`, gated on
      `scope_level > 0`). Also improved the cryptic "Unsupported initializer"
      diagnostic to "Static initializer is not a constant" for non-constant
      static initializers ([semantic/initializers.c](semantic/initializers.c)) —
      covers `non_constant_static_initializer(_local)`. Reclassifications vs. the
      book: `static_var_case` is a *parse* error for us (a case label needs a
      constant expression → parser file, like ch8 `non_constant_case`);
      `conflicting_variable_linkage_2` is caught by the no-shadowing rule (the
      inner `extern int x` shadows the enclosing local `int x = 3`) and reported
      as "declared both with and without linkage" rather than the file-scope
      linkage conflict the book intends. 2 `DISABLED_`
      (`extern_follows_static_local_var`, `out_of_scope_extern_var`): a
      block-scope `static`/`extern` is stored at file scope (level 0), so we
      can't yet distinguish it from a genuine file-scope entity — to be enabled
      with the 9b run-tests work.
- [x] **Task 9b — Chapter 10 run** (file-scope / storage class): delivered
      `backend/besm6/chapter10_tests.cpp` (8 run + 15 `DISABLED_`). CMake wired;
      full suite green (1803). Chapter 10 is mostly about static/extern storage,
      much of which the BESM-6 backend does not yet support, so most programs are
      `DISABLED_`. What runs: file-scope globals, file-scope statics with a
      single (or tentative-then-) definition, extern bringing an existing global
      into scope, switch-on-extern, and `int static`/`int extern` (type before
      storage class). The `DISABLED_` groups, each with a one-line reason:
      (a) **no block-scope static-local storage** — a `static int x;` inside a
      function emits no `static_variable` toplevel, so the body references an
      undefined name (confirmed: Madlen has no static-local support):
      `static_local_uninitialized`, `static_variables_in_expressions`,
      `compound_assignment_static_var`, `goto_skip_static_initializer`,
      `switch_skip_static_initializer`, `label_static_var_same_name`,
      `multiple_static_local`; (b) **tentative/extern clobber** — a tentative
      (`int x;`) or `extern int x;` file-scope declaration that *follows* an
      initialized definition (`int x = 3;`) emits a second uninitialized toplevel
      that overwrites the initializer to 0: `static_then_extern`,
      libraries `external_variable`; (d) **internal linkage needs separate
      translation units** — concatenating the two files merges (or redefines)
      the distinct same-named statics: libraries `internal_linkage_var`,
      `internal_linkage_function`, `internal_hides_external_linkage`, and
      ec-libraries `same_label_same_fun`; plus `static_recursive_call` (main
      falls off the end → indeterminate, like the ch5/ch6 cases) and
      `push_arg_on_page_boundary` (needs an external x86 `.s` symbol `zed`).
      Note (c): right shift of a negative int is implementation-defined
      (C11 §6.5.7p5); BESM-6 shifts logically (no sign extension) where x86 is
      arithmetic, so `bitwise_ops_file_scope_vars` is a *run* test whose expected
      value is the BESM-6 result (2), not the x86 result (0) — the constant-fold
      path still matches x86, which is why earlier chapters' constant-shift tests
      pass. Reclassifications vs. the book: chapter 10's central idiom
      `int x = …; { extern int x; }` (an inner extern un-shadowing a file-scope
      variable) is rejected by the permanent no-shadowing rule, so those
      book-"valid" programs are not run tests here (`distinct_local_and_extern`,
      `extern_block_scope_variable`, `shadow_static_local_var`,
      `static_local_multiple_scopes`, `label_file_scope_var_same_name`,
      libraries `external_linkage_function`, `external_var_scoping`); the
      negative direction is already covered in the semantic chapter files. The
      two semantic `DISABLED_` tests from Task 9 stay disabled (the block-scope
      static/extern scope-level + name-mangling refactor was deferred).
- [x] **Task 10 — Chapter 11 negative** (Long integers; scanner 2 lex): delivered
      `scanner/chapter11_tests.cpp` (2 lex), `parser/chapter11_tests.cpp` (8 parse),
      and `semantic/chapter11_tests.cpp` (6). CMake wired; full suite green (1819).
      Compiler change: the scanner now validates the *combination* of an integer
      suffix instead of accepting any run of `u`/`l`/`f`. Added `valid_int_suffix`
      / `valid_float_suffix` to [scanner/scanner.c](scanner/scanner.c) `scan_number()`
      — an integer takes an optional `u`/`U` and an optional `l`/`L` or matched-case
      `ll`/`LL` in either order; a float takes a single `f`/`F` or `l`/`L`. This
      rejects `0lL` and `0LLL` (reusing the existing "invalid suffix on numeric
      constant" diagnostic) while still lexing `10LL`/`10ULL`/`ull`/`1.0f` that the
      translator/semantic tests depend on (and incidentally sets up ch12's
      `0lul`/`0uu`). The 8 parse and 5 invalid_types negatives already produced clean
      errors; one reclassification — `call_long_as_function` declares a file-scope
      `long x(void)` then a local `long x`, which our permanent no-shadowing rule
      rejects as "Duplicate variable declaration" before the call. Omitted (41-bit
      int, not negatives for us): `bitshift_duplicate_cases` and
      `switch_duplicate_cases` — their duplicate case relies on x86 32-bit truncation
      (2³⁵+400 → 400, 2³⁴ → 0) that a 41-bit int doesn't perform, so no error is
      raised; candidates for run tests under 10b. `switch_duplicate_cases_2` (switch
      type `long`, `100l` vs `100`) is a genuine "Duplicate case value" negative.
- [x] **Task 10b — Chapter 11 run** (Long integers): delivered
      `backend/besm6/chapter11_tests.cpp` (15 run + 18 `DISABLED_`). CMake wired;
      full suite green (1834). No compiler changes needed. The decisive fact:
      `semantic/target.c` makes `int` and `long`/`long int` the **same** 41-bit
      signed single word (range ±2⁴⁰ ≈ ±1.1×10¹²); `codegen_sizeof` (abi.h)
      returns one word for both and an int↔long conversion is a plain COPY.
      `long long` (two words) is deferred to task #24 and not exercised. Chapter
      11 is written to prove an x86 compiler distinguishes 32-bit `int` from
      64-bit `long`, a distinction BESM-6 does not have, so the corpus splits:
      programs whose every value fits in 41 bits run correctly and match the book
      (the 15 enabled, e.g. `assign`, `large_constants`, `long_and_int_locals`,
      `long_args`, `multi_op`, `return_long`, `common_type`, `sign_extend`,
      `compound_assign_to_long`, `switch_long`, and the `long_args` /
      `maintain_stack_alignment` / `return_long` library pairs concatenated
      client-first). The 18 `DISABLED_` fall in two groups, each with a one-line
      reason: (A) **value exceeds the 41-bit range** — `arithmetic_ops`
      (complement uses LONG_MAX−1 ≈ 9.2e18), `comparisons`/`logical` (2⁶⁰),
      `simple`/`increment_long` (±(2⁶³−1)), `static_long` (1.15e18),
      `type_specifiers` (for-init 2⁴⁰ = LONG_MAX+1 → negative, loop runs 0×),
      `bitshift` ((40<<40) = 4.4e13), `bitwise_long_op`/`compound_bitwise`
      (~7.2e16), `compound_bitshift` (l<<=33 = 1.06e14); and (B) **relies on x86
      32-bit `int` truncation of a `long`** — BESM-6 `int` is 41-bit so the value
      is not truncated and the self-check fails: `convert_by_assignment`,
      `convert_function_arguments`, `convert_static_initializer`, `truncate`,
      `compound_assign_to_int` (c*=10000), `switch_int` (case 2³³/2³⁵−1 → 0/−1),
      libraries `long_global_var` (return_l_as_int expects (int)2³³ == 0). These
      are target-semantics gaps, not codegen bugs; unlike ch10's logical-shift
      case the programs self-check and return an error code on mismatch, so a
      BESM-6-valued expectation would just encode a meaningless failure code.
- [x] **Task 11 — Chapter 12 negative** (Unsigned; scanner 2 lex): delivered
      `scanner/chapter12_tests.cpp` (2 lex), `parser/chapter12_tests.cpp` (2 parse),
      and `semantic/chapter12_tests.cpp` (2). CMake wired; full suite green (1841).
      The two `invalid_lex` suffix programs (`0uu`, `0lul`) are already rejected by
      the task-10 suffix validation; both `invalid_types` programs
      (`conflicting_signed_unsigned`, `conflicting_uint_ulong`) already diagnose
      ("Variable x redeclared with different type" / "Conflicting declarations for
      function"). One compiler fix: `fuse_type_specifiers`
      ([parser/decl.c](parser/decl.c)) tracked a single `signedness` flag but never
      rejected a *second* signedness specifier, silently last-wins on
      `(signed unsigned)` and `unsigned … unsigned`; the `TYPE_SIGNED`/`TYPE_UNSIGNED`
      cases now reject a duplicate ("duplicate unsigned/signed specifier") or
      conflicting ("unsigned cannot combine with signed" and vice-versa) signedness —
      this lights up both `invalid_parse` negatives (the one change covers the
      declaration and the cast/type-name paths, which share `fuse_type_specifiers`).
      Omitted (target-semantics gap, not a negative for us):
      `invalid_labels/extra_credit/switch_duplicate_cases` relies on x86's 32-bit
      unsigned truncation to collapse `4294967295u` and `1099511627775l` onto the
      same case (both → 2³²−1); BESM-6 `unsigned int` is a single 41-bit word, so
      `1099511627775` (2⁴⁰−1) is not truncated, the cases stay distinct, and the
      program is accepted — like ch11's `switch_duplicate_cases`, it is an 11b run
      candidate. No `DISABLED_` needed.
- [ ] **Task 11b — run** (Unsigned).
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
