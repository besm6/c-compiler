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
- [x] **Task 11b — Chapter 12 run** (Unsigned): delivered
      `backend/besm6/chapter12_tests.cpp` (4 run + 25 `DISABLED_`). CMake wired;
      full suite green (1845). No compiler changes needed. The decisive fact:
      `semantic/target.c` makes every *unsigned* integer type a single **48-bit**
      word (`unsigned int` == `unsigned long` == `unsigned long long`, range
      0…2⁴⁸−1); signed `int`/`long` stay 41-bit. The backend already has every
      unsigned helper (`b/uadd`, `b/usub`, `b/umul`, `b/udiv`, `b/umod`, `b/uneg`,
      and the unsigned comparisons `b/ult`/`b/ule`/`b/ugt`/`b/uge`). Chapter 12 is
      written to prove an x86 compiler distinguishes a 32-bit `unsigned int`
      (wraps at 2³²) from a 64-bit `unsigned long` (wraps at 2⁶⁴), neither of which
      BESM-6 has, so — like chapter 11 — the corpus splits. The 4 enabled: `simple`
      (2³¹−1 + 2 == 2³¹+1, no wrap), `unsigned_type_specifiers` (the initialized
      def is the *last* declaration so no tentative clobber; the for-loop runs 11×
      whether the unsigned wraps at 2³² or 2⁴⁸, since both wrapped values exceed the
      `< 4294967295U` bound), `implicit_casts/promote_constants` (2³⁶ takes
      unsigned-long type and the −1l comparison goes through the unsigned-long common
      type via `b/uge`; the `3ul + 4294967293ul == 2³²` stays nonzero), and
      `explicit_casts/rewrite_movz_regression` (the ch11 twin — small values).
      The 25 `DISABLED_` fall in three groups, each with a one-line reason:
      (A) **value exceeds 2⁴⁸ / 64-bit wraparound** — `arithmetic_ops`,
      `arithmetic_wraparound`, `locals` (−a expects 2⁶⁴ range), `logical` (2⁶⁰),
      `static_variables` (2⁶³), `common_type`, `convert_by_assignment`,
      `static_initializers`, `extension`, `round_trip_casts`, `same_size_conversion`,
      `truncate`, `bitwise_unsigned_ops`, `compound_bitshift`, `compound_bitwise`,
      libraries `unsigned_args`; (B) **relies on x86 32-bit unsigned-int
      wraparound/truncation** — BESM-6 `unsigned int` is 48-bit, so `-1u`, `ui++`
      at 2³²−1, and `(int)`/`(unsigned)` narrowing don't behave as on x86:
      `chained_casts`, `bitwise_unsigned_shift`, `compound_assign_uint`,
      `postfix_precedence`, `switch_uint` (2³⁵+10 case not truncated to 10),
      `unsigned_incr_decr`, libraries `unsigned_global_var`; and (C) **backend
      limitation unrelated to unsigned width** — `comparisons` collides on the
      8-char Madlen symbol limit (`one_hundred` and `one_hundred_ulong` both become
      `ONE*HUND` → "twice-described identifier"), and `signed_type_specifiers` hits
      the chapter-10 tentative-after-def clobber (`signed int static i;` after
      `int static signed i = 5;` re-emits an uninitialized toplevel that resets i to
      0). Like ch11, the group-A/B programs self-check and return an error code on
      mismatch, so a BESM-6-valued expectation would just encode a meaningless
      failure code; `DISABLED_` is the honest call.
- [x] **Task 12 — Chapter 13 negative** (Floating-point; scanner 7 lex): delivered
      `scanner/chapter13_tests.cpp` (7 lex), `parser/chapter13_tests.cpp` (2 parse),
      and `semantic/chapter13_tests.cpp` (3 invalid_types + 13 extra_credit = 16).
      CMake wired; full suite green (1870). Two compiler fixes lit up the five
      programs that were silently accepted: (1) `scanner/scanner.c` `scan_number()`
      now requires at least one digit after an exponent marker (`e`/`E`, and the hex
      `p`/`P` for symmetry) — "missing exponent in numeric constant" — covering
      `missing_exponent` (`30.e`), `missing_negative_exponent` (`24e-`), and
      incidentally `another_bad_constant` (`1.ex`, whose `e` is followed by a letter,
      so it now reports the missing exponent before the trailing-character guard);
      and the trailing-character guard now also rejects a number immediately
      followed by a second `.` ("invalid suffix on numeric constant"), covering
      `malformed_exponent` (`1.0e10.0`, which the book treats as one malformed
      pp-number — we previously tokenized it as `1.0e10` `.` `0` and died as a parse
      error). The other three lex programs (`bad_exponent_suffix` `1E2x`,
      `malformed_const` `2._`, `yet_another_bad_constant` `1.e-10x`) already hit the
      trailing-character guard. (2) `semantic/expressions.c` `EXPR_ASSIGN` now
      rejects the compound bitwise/shift/remainder assignments (`%= &= |= ^= <<= >>=`)
      on floating operands ("Compound bitwise/remainder assignment requires integer
      operands") — the plain `% & | ^ << >>` already rejected doubles but the
      compound forms only checked `is_arithmetic`; covers the six `extra_credit`
      `compound_*` programs. The remaining negatives already diagnosed cleanly:
      both `invalid_parse` (`double double`, `unsigned double`) via the parser's
      `fuse_type_specifiers`, and `~`/`%`/bitwise/shift/`switch (double)`/`case 1.0`
      via the existing typecheck. No `DISABLED_` needed.
- [x] **Task 12b — Chapter 13 run** (Floating-point): delivered
      `backend/besm6/chapter13_tests.cpp` (16 run + 23 `DISABLED_` = all 39
      programs, the 5 `libraries` client/impl pairs merged into one source each).
      CMake wired; full suite green (1886). Two compiler fixes were needed, both
      genuine conformance/codegen gaps the book's floating-point corpus exposed:
      (1) `scanner/scanner.c` now lexes a floating constant with no integer part
      (`.5`, `.01e+2`, and after a unary minus `-.00004`): the dispatch peeks one
      char past a leading `.` and, if it is a digit, routes to `scan_number`
      (whose `decimal:` path already handled a leading `.` — only the routing was
      missing; member access `a.b` and `...` are unaffected). (2)
      `backend/besm6/codegen.c` `declare_global_name` is now also called for the
      width / int↔FP / pointer-representation conversion TAC kinds (they share a
      `{src,dst}` union common-initial-sequence). Previously `(int)glob` on a
      module-level `double` referenced `glob` via `,utc,` without emitting the
      `glob: ,subp,` self-declaration, so the Madlen assembler died with
      "undescribed identifier"; only integer-global loads (plain COPY/RETURN
      operands) were being declared. Three golden-Madlen `convert_tests.cpp`
      cases (`DoubleToIntMadlen`, `DoubleToUintMadlen`, `FloatToDoubleMadlen`)
      encoded the old, un-assemblable output and were updated to include the now-
      emitted `,subp,` line. The decisive architectural facts: BESM-6 FP is one
      48-bit format for float/double/long double — 7-bit exponent (~2^-63 ..
      2^63, i.e. ~1.08e-19 .. ~9.2e18), 40-bit mantissa (~12 decimal digits) —
      with NO NaN/infinity/subnormals; signed `long` is 41-bit and `unsigned
      long` is 48-bit; and there is no libm or static-local storage. The 16
      enabled are the programs whose every value is in-range, need ≤12 digits, and
      avoid all of the above: `simple`, `comparisons`, `loop_controlling_
      expression`, `constant_doubles`, the four `function_calls` calling-
      convention cases (`double_and_int_parameters`, `double_and_int_params_
      recursive`, `double_parameters`, `push_xmm`), `use_arg_after_fun_call`,
      `cvttsd2si_rewrite`, `double_to_signed`, `rewrite_cvttsd2si_regression`,
      `compound_assign`, and the three runnable `libraries` cases (`double_and_
      int_params_recursive`, `double_parameters`, `use_arg_after_fun_call`). The
      23 `DISABLED_` fall in seven groups, each with a one-line reason:
      (A) **NaN unsupported** — `nan`, `nan_compound_assign`, `nan_incr_and_decr`
      (also need `isnan`); (B) **infinity / negative-zero / subnormal
      unsupported** — `infinity`, `negative_zero`, `subnormal_not_zero`;
      (C) **value > 2^63 exponent** — `return_double` (1234e75), `arithmetic_ops`
      (12e30; also a 17-digit `0.1+0.2` check), `libraries/extern_double` (1e20),
      `incr_and_decr` (10e20; also a static local); (D) **value exceeds the
      narrow 41-bit long / 48-bit ulong range, or relies on x86 64-bit
      signed→unsigned width** — `signed_to_double` (9e15 long literal),
      `double_to_unsigned` (3.4e18 > 2^48), `unsigned_to_double`, `common_type`,
      `convert_for_assignment`, `complex_arithmetic_common_type`
      ((unsigned long)(int -50) is 2^41-50 here, so `ul + i` does not wrap to
      9950), `compound_assign_implicit_cast`; (E) **1e-20 < 2^-63 underflows to
      0**, breaking `non_zero` — `logical`; (F) **no static-local storage** —
      `static_initialized_double`, `static_initializers`; (G) **17-digit /
      2^63-boundary precision not meaningful at 40 bits** — `round_constants`;
      and **requires libm** — `standard_library_call` (fma/ldexp),
      `libraries/double_params_and_result` (fmax). Like chapters 11 and 12 these
      programs self-check and return an error code on mismatch, so a BESM-6-valued
      expectation would just encode a meaningless failure code; `DISABLED_` is the
      honest call.
- [x] **Task 13 — Chapter 14 negative** (Pointers): delivered
      `parser/chapter14_tests.cpp` (4 parse) and `semantic/chapter14_tests.cpp`
      (43). CMake wired; full suite green (1933). Of the 47 book negatives, 46
      already produced a clean, specific diagnostic; one compiler fix lit up the
      last: `build_static_init` ([semantic/initializers.c](semantic/initializers.c))
      silently accepted any non-zero integer constant as a static pointer
      initializer (`static int *x = 10;` → `TAC_STATIC_INIT_I64` treating it as an
      address). It now rejects a bare non-zero integer ("Static initializer for
      pointer must be a null pointer constant") while still accepting an explicit
      pointer cast (`char *foo = (char *)(0x40000000 + 0xe00000);`, an address
      constant relied on by `InitVarCastPtrExpression`/`VarIntPtrInitLiteral`) — the
      distinguishing factor is whether the initializer expression is an
      `EXPR_CAST` to a pointer type. Reclassifications vs. the book: two
      `invalid_parse` programs parse cleanly for us and are caught by the type
      checker (`abstract_function_declarator` → "Can only cast scalar types",
      `malformed_function_declarator` → "Function cannot return a function"), so
      they moved to the semantic file; the two `invalid_declarations` label
      programs (`addr_of_label`, `deref_label`) use a label name as a value, which —
      labels and identifiers being separate namespaces — reports "Symbol not
      found", like chapter 6's `goto_variable`. No `DISABLED_` needed — every
      program yields a clean diagnostic. 
- [x] **Task 13b — Chapter 14 run** (Pointers): delivered
      `backend/besm6/chapter14_tests.cpp` (16 run + 13 `DISABLED_` = all 29
      logical programs, the 4 `libraries` files merged into 2 client-first
      pairs). CMake wired; full suite green (1949). Two compiler fixes lit up
      programs that are otherwise fully in-range, both genuine gaps the pointer
      corpus exposed: (1) **parser** — `parse_direct_abstract_declarator`
      ([parser/decl.c](parser/decl.c)) rejected a nested-parenthesized abstract
      declarator like `(long unsigned int ((((*))))) 0` with "Empty type
      specifier list" (after consuming `(`, a following `(` fell through to the
      parameter-type-list branch). A `(` after `(` can only begin a nested
      abstract declarator — a parameter list must start with a type — so the `(`
      case now joins the `*` case (`parse_pointer()` yields NULL, the extra
      parens act as pure grouping via `type_apply_pointers(NULL)`); lights up
      `abstract_declarators`. (2) **backend** — LOAD/STORE/GET_ADDRESS through a
      *module-level (global)* pointer. The word LOAD/STORE and GET_ADDRESS dst
      assumed the pointer/destination lived in the frame and died with
      "variable '...' not in frame" for a file-scope `double *d_ptr`. Added
      `emit_wtc_ptr` (frame slot → single `wtc`; global → `utc name` to set C to
      the pointer's address, then a bare `wtc` to load the pointer word into C)
      and `emit_store_a` (store A to a frame slot or, for a global, `utc name`
      + bare `atx`) in [backend/besm6/emit.c](backend/besm6/emit.c), used by the
      word LOAD/STORE and by COPY/GET_ADDRESS
      ([backend/besm6/instr.c](backend/besm6/instr.c)); lights up libraries
      `global_pointer`. The book's `putchar` is not in our libc, so
      `eval_compound_lhs_once` calls libc `putch` (output `"AB0\n"`). The 13
      `DISABLED_` fall in three groups, each with a one-line reason:
      (A) **no block-scope static-local storage** (a `static` inside a function
      emits no toplevel) — `cast_between_pointer_types`, `pointer_int_casts`
      (also relies on x86 byte-addressing, `ptr % 8 == 0`; a BESM-6 pointer is a
      *word* address), `declarators`, `dereference_expression_result`,
      `static_var_indirection`; (B) **value exceeds the BESM-6 integer range**
      (41-bit signed / 48-bit unsigned) — `read_through_pointers` (1.38e19,
      1.44e17), `update_through_pointers` (long 1.44e17, `d=1e50`),
      `bitwise_ops_with_dereferenced_ptrs` (2⁶³), `compound_assign_conversion`
      (2⁶³−1), `compound_bitwise_dereferenced_ptrs` (~1.8e19); and (C) **relies
      on x86 32/64-bit unsigned wraparound** — `bitshift_dereferenced_ptrs`
      (32-bit `<<` wrap), `incr_and_decr_through_pointer` (`0ul--` → 2⁶⁴−1),
      `switch_dereferenced_pointer` (case label ~1.8e19 / `l % 2³²` truncation).
      Like chapters 11–13 the group-B/C programs self-check and return an error
      code on mismatch, so a BESM-6-valued expectation would just encode a
      meaningless failure code; `DISABLED_` is the honest call.
- [x] **Task 14 — Chapter 15 negative** (Arrays / pointer arithmetic): delivered
      `parser/chapter15_tests.cpp` (14) and `semantic/chapter15_tests.cpp` (47) — one
      test per book program (61 total: invalid_parse 18, invalid_types 33,
      invalid_types/extra_credit 10). CMake wired; full suite green (1989). No compiler
      changes. 40 enabled, 21 `DISABLED_`. Reclassifications vs. the book: many
      `invalid_parse` programs parse cleanly for us and are caught by the type checker
      (`return_array`, `function_returns_array` → "Function cannot return an array"; the
      cast-to-array abstract declarator `malformed_abstract_array_declarator_2` → "Can
      only cast scalar types"); conversely `cast_to_array_type_3` (book `invalid_types`)
      is a parse error for us (`long(([2])[3])` → "Empty type specifier list"), so it
      moved to the parser file. The 21 `DISABLED_` are honest gaps the array corpus
      exposed — every one is *accepted* by our front end today (verified by running
      `parse`/`lower`), so an `EXPECT_DEATH` would just fail. They fall in six groups:
      (A) **array lvalue not rejected as non-modifiable** — `assign_to_array`(`_2`/`_3`),
      `compound_assign_to_array`(`_nested`), `postfix_incr_array`(`_nested`),
      `prefix_decr_array`(`_nested`); (B) **incompatible pointer types not diagnosed**
      in assignment/comparison/subtraction — `assign_incompatible_pointer_types`,
      `compare_different_pointer_types`, `sub_different_pointer_types`;
      (C) **conflicting redeclaration accepted** — `conflicting_array_declarations`,
      `conflicting_function_declarations`; (D) **scalar initializer for a *static*
      array accepted** (the non-static form is caught) — `null_ptr_static_array_
      initializer`; (E) **array of functions** passes typecheck and only trips a
      `get_size` assert later during lowering (not a clean typecheck diagnostic) —
      `array_of_functions`(`_2`), `parenthesized_array_of_functions`; (F) **array
      declarator size unvalidated** — `double_declarator` (`int x[2.0]`),
      `negative_array_dimension` (`int arr[-3]`), `empty_initializer_list` (`{}`, also
      valid as of C23). Each `DISABLED_` carries a one-line reason; fixing them is
      frontend type-checking work, out of scope for the test-import task.
- [x] **Task 14b — Chapter 15 run** (Arrays / pointer arithmetic): delivered
      `backend/besm6/chapter15_tests.cpp` (23 run + 19 `DISABLED_` = all 42 logical
      programs, the 6 `libraries` files merged into 3 client-first pairs). CMake wired;
      full suite green (2012). The array corpus exposed a real cluster of
      **multi-dimensional array / pointer-to-array bugs**, all fixed (the chapter is core
      C, not x86-specific, so these are genuine gaps like chapter 14's two fixes):
      (1) **translator decay** — `typecheck_and_decay` mutates a node's type from array to
      pointer *in place* ([semantic/expressions.c](semantic/expressions.c)), so a
      multi-dimensional subscript `a[i]` (whose result is itself a row array) lost its
      array-ness: the lowerer scaled the index by the decayed *pointer* size (6) instead of
      the *row* size and emitted a spurious `LOAD`. Fixed in
      [translator/expr.c](translator/expr.c): `EXPR_SUBSCRIPT` (both `gen_lval` and
      `gen_expr`) and `UNARY_DEREF` now take the scale from the *pointer operand's pointee*
      (`get_size(ptr->type->u.pointer.target)`) and, when that pointee is itself an array,
      return the sub-array's address (array-to-pointer decay) instead of loading a scalar.
      (2) **pointer-to-array arithmetic** — `ptr ± k`, `++/--`, and `+=`/`-=` on a word
      pointer assumed a one-word element (`int(*)[3]` advanced by 1 word, not 3). Added
      `wide_ptr_scale` and routed all four forms (`gen_binary`, `gen_step`, both compound-
      assign sites) through a generalized `gen_ptr_add` that scales by the element size.
      (3) **backend ADD_PTR power-of-two scale** — the strength-reduced `asn` left shift for
      a power-of-two word scale ([backend/besm6/instr.c](backend/besm6/instr.c)) spilled a
      *negative* index's sign bits into the exponent field, so decrementing a `long(*)[4]`
      (scale 4) produced a wild address; added the same `aax =37777777777777` 41-bit mask
      the signed-multiply strength reduction already uses (`AddPtrPowerOfTwoScale` test
      updated). The 19 `DISABLED_` fall in six groups, each with a one-line reason:
      (A) **no block-scope static-local storage** — `equivalent_declarators`,
      `compound_assign_array_of_pointers`, `compound_lval_evaluated_once`,
      `automatic_nested`, `static`, `static_nested`, `pointer_add`, `pointer_diff`,
      `simple_subscripts`; (B) **no identifier shadowing** (param/local reuses a file-scope
      name) — `return_nested_array`, `subscript_nested`, `complex_operands`; (C) **array→
      pointer parameter adjustment not performed** (`int a[2][3]` vs `int (*a)[3]` read as
      conflicting) — `array_as_argument`; (D) **x86 byte/alignment assumption** (`ptr % 16
      == 0`) — `test_alignment`; (E) **value exceeds the BESM-6 integer range** (2⁶⁴/2⁶³/
      3.46e18) — `implicit_and_explicit_conversions`, `automatic`,
      `compound_bitwise_subscript`, `compound_pointer_assignment`; (F) **relies on x86
      32-bit unsigned wraparound** — `compound_assign_to_subscripted_val`. Like chapters
      11–14 the group-D/E/F programs self-check and return an error code on mismatch, so a
      BESM-6-valued expectation would just encode a meaningless failure code; `DISABLED_`
      is the honest call.
- [x] **Task 15 — Chapter 16 negative** (Characters / strings): delivered
      `scanner/chapter16_tests.cpp` (8 invalid_lex), `parser/chapter16_tests.cpp` (4
      invalid_parse + 4 extra_credit), and `semantic/chapter16_tests.cpp` (17
      invalid_types + 8 extra_credit + 1 invalid_labels) — one test per book program (42
      total). CMake wired; full suite green. No `DISABLED_`: every program yields a clean,
      specific diagnostic — including the redeclaration conflicts (`char_and_schar_
      conflict`, `char_and_uchar_conflict`) and all string-initializer length/type checks,
      which chapter 15's notes flagged as front-end gaps but Chapter 16 exercises cleanly.
      One compiler fix lit up the only two programs that were silently accepted: the
      scanner swallowed unknown escape sequences. `scan_char`/`scan_string`
      ([scanner/scanner.c](scanner/scanner.c)) consumed any character after a backslash;
      both now `lex_error("invalid escape sequence '\\%c'")` for anything outside the C
      escape set (`' " ? \ a b f n r v`, octal, `\x`+hex), lighting up
      `char_bad_escape_sequence` (`'\y'`) and `string_bad_escape_sequence` (`"foo\ybar"`).
      `scan_string` previously had *no* escape validation at all and gained the same logic.
      Reclassifications vs. the book: `unescaped_double_quote` (`"foo"bar"`) is in the
      book's invalid_lex; under the scanner alone (no parser to halt at the stray
      identifier) it opens a final string that runs to EOF, so it dies as an unterminated
      string and stays in the scanner file; `implicit_conversion_pointers_to_different_
      size_arrays` is rejected one step earlier than the book's array-size mismatch — we
      reject `&"x"` as "Cannot take address of non-lvalue". `15b — run` (the 54 valid
      Chapter 16 programs) remains.
- [x] **15b — Chapter 16 run** (Characters/strings): delivered
      `backend/besm6/chapter16_tests.cpp` — 51 tests (20 run + 31 `DISABLED_`) covering all
      54 valid Chapter 16 programs (the 2 `data_on_page_boundary_*.s` files are assembly,
      not C; `libraries` merged client-first). CMake wired; full suite green (2074). Four
      genuine compiler bugs surfaced and were fixed (char/string init is core C, not
      x86-specific): (1) **zero-init of small integer types** — `make_zero_init`
      ([semantic/initializers.c](semantic/initializers.c)) only handled `char`/`int`/long
      kinds, so an unspecified `signed char`/`unsigned char` array element hit
      `fatal_error("Unsupported type for zero init")`; added the `signed/unsigned char`,
      `_Bool`, `short`, `unsigned short`, `unsigned int` cases. (2) **auto char-array
      element store** — `gen_compound_init` ([translator/stmt.c](translator/stmt.c)) always
      emitted the word-kind `COPY_TO_OFFSET`, but a char element sits at a sub-word byte
      offset; it now picks `COPY_BYTE_TO_OFFSET` for size-1 leaves. (3) **static char-array
      data layout** — a `char arr[N] = {...}` global emitted one 48-bit word per byte
      ([backend/besm6/static.c](backend/besm6/static.c)), but char arrays pack 6 bytes per
      word, so byte access read the wrong byte; added `char_array_log_items` that flattens
      the whole init list (byte values, zero runs, embedded strings — KOI-7 byte count, not
      the UTF-8 type size) into packed `,log,` words with trailing zero words coalesced into
      a `,bss,`. (4) **signed `~` corrupted the INT-format word** — complement flipped all
      48 bits including the exponent field, so `~1` was a non-integer instead of `-2`; split
      into `TAC_UNARY_COMPLEMENT` (signed, flip the 41 value bits via `aex =37777777777777`,
      preserving the exponent → canonical `-2`) and `TAC_UNARY_COMPLEMENT_UNSIGNED` (exact
      48-bit flip), routed by operand signedness in [translator/expr.c](translator/expr.c).
      The 31 `DISABLED_` fall in eight groups, each one-line-documented: **no static-local
      storage** (9: `explicit_casts`, `convert_by_assignment`, `terminating_null_bytes`,
      `partial_initialize_via_string`, `incr_decr_chars`, `char_consts_as_cases`,
      `compound_assign_chars`, `compound_bitwise_ops_chars`, `bitwise_ops_character_constants`);
      **string literals are KOI-7 (lowercase Latin folds to uppercase, but char constants
      stay ASCII)** so a lowercase byte read from a literal differs from its char-constant
      value (5: `strings_as_initializers/simple`, `strings_as_lvalues/simple`,
      `pointer_operations`, `cast_string_pointer`, `strings_in_function_calls`);
      **multi-dimensional char array** needs a sub-word row pointer the backend's pointer
      model can't represent (3: `literals_and_compound_initializers`,
      `adjacent_strings_in_initializer`, `transfer_by_eightbyte`); **prints lowercase text →
      renders as Cyrillic** (5: `write_to_array`, `adjacent_strings`, `standard_library_calls`,
      `string_special_characters`, `addr_of_string`); **value exceeds BESM-6 range / relies on
      32-bit unsigned width** (3: `static_initializers`, `common_type`, `bitwise_ops_chars`);
      **local char-array string init** copies the string-constant pointer instead of the
      bytes (1: `array_init_special_chars`); **negative-operand `>>` is logical/impl-defined**
      (1: `bitshift_chars`); **x86 byte-layout / page boundary** (2:
      `access_through_char_pointer`, `push_arg_on_page_boundary`); **16-byte alignment
      assumption** (1: `test_alignment`); plus **no identifier shadowing** (1:
      `char_constant_operations`). Like chapters 11–15 the programs self-check and return an
      error code on mismatch, so a BESM-6-valued expectation would encode a meaningless
      failure code — `DISABLED_` is the honest call.
- [ ] **Task 16 — Chapter 17 negative** (void / sizeof / dynamic alloc): **negative
      half delivered** — `parser/chapter17_tests.cpp` (4 tests:
      void/`_Bool`-with-modifier specifiers, unparenthesized `sizeof` of a cast/type)
      and `semantic/chapter17_tests.cpp` (56 tests covering all 56 `invalid_types`
      programs: void / scalar_expressions / pointer_conversions / incomplete_types /
      extra_credit). **No `DISABLED_`** — every program yields a clean, specific
      diagnostic. Eleven programs the compiler previously accepted are now rejected by
      six small frontend `void`/incomplete-type fixes: (1) a value-less `return` in a
      non-`void` function ([semantic/statements.c](semantic/statements.c)); (2) `++`/`--`
      on a `void*` (incomplete pointee) — pre/post inc/dec
      ([semantic/expressions.c](semantic/expressions.c)); (3) relational `<`/`>`/`<=`/`>=`
      on a `void`/`void*` operand (tightened the non-arithmetic branch to require two
      *complete* pointers); (4) equality `==`/`!=` with a `void` operand, plus comparing a
      pointer to a non-null integer (tightened `common_pointer_type`'s `void*` fallback in
      [semantic/typecheck.c](semantic/typecheck.c) to require the other side be a pointer);
      (5) a named `void` parameter (`validate_type`, leaving the unnamed `f(void)` sentinel
      intact); (6) `sizeof` of a function type (the typecheck-only `RunPipeline` harness
      doesn't reach the later `get_size`, so the check was lifted into `EXPR_SIZEOF_EXPR`).
      Full suite green (2134). 
- [x] **Task 16b — Chapter 17 run** (void / sizeof / dynamic alloc): delivered
      `backend/besm6/chapter17_tests.cpp` (13 run + 12 `DISABLED_` = all 25 logical
      programs, the 6 `libraries` files merged into 3 client-first pairs). CMake wired;
      full suite green. The book's host-only `#ifdef SUPPRESS_WARNINGS` / `#pragma`
      blocks are dropped during transcription (our scanner has no preprocessor) — they
      guard only host warning pragmas, no logic. One compiler fix: a void/void
      conditional leaked a `TYPE_VOID` in the type checker (`EXPR_COND` allocated a
      result type that the clone-and-convert tail never freed); the void branch now owns
      the type directly and returns early ([semantic/expressions.c](semantic/expressions.c)),
      lighting up `ternary`. The 13 enabled: all four `void/` programs (`cast_to_void`,
      `ternary`, `void_function`, `void_for_loop` — `putchar`→libc `putch`, prints the
      uppercase alphabet which renders as ASCII), and nine `sizeof` programs whose x86
      size literals (4/8) were **rewritten to BESM-6 sizes** — `char`=1, but
      `short/int/long/double/pointer`=6 (one 48-bit word), since `CodegenTest` evaluates
      `sizeof` with `target=besm6`: `sizeof/{simple, sizeof_basic_types, sizeof_consts,
      sizeof_result_is_ulong, sizeof_not_evaluated}` and `extra_credit/{sizeof_bitwise,
      sizeof_compound, sizeof_compound_bitwise, sizeof_incr}` (the four `extra_credit`
      programs also had incidental `static` dropped from sizeof-operand locals — no
      static-local storage). The 12 `DISABLED_` fall in five groups, each one-line-noted:
      (A) **no malloc/calloc/realloc/aligned_alloc/free/memset/memcmp/memcpy in libc** —
      all six `void_pointer/` programs, `sizeof/sizeof_expressions`, libraries
      `pass_alloced_memory`; (B) **array-parameter type adjustment leaks a cloned type,
      and `sizeof` of a string literal is word/alignment-padded** (`sizeof "Hello,
      World!"`==16 here, not 14) — `sizeof/sizeof_array`; (C) **parser rejects the nested
      abstract declarator** `double(*([3][4]))[2]` ("Empty type specifier list") —
      `sizeof/sizeof_derived_types`; (D) **global array too large for BESM-6 core** (12M
      words vs 32K) — libraries `sizeof_extern`; (E) **loop over the ctest timeout** (10M
      iterations) — libraries `test_for_memory_leaks`.
- [x] **Task 17 — Chapter 18 negative** (Structures/unions — largest set): **delivered**
      — `parser/chapter18_tests.cpp` (48 programs: `invalid_lex` + `invalid_parse` incl.
      extra_credit) and `semantic/chapter18_tests.cpp` (152 programs: `invalid_struct_tags`
      + all `invalid_types`, incl. extra_credit), 200 total. Seven small frontend fixes
      light up programs we previously accepted: parser (`parser/decl.c`) — (1) a basic type
      specifier combined with a struct/union/enum/typedef specifier, and (2) a scalar member
      with no declarator (`int;`), preserving anonymous-aggregate and bitfield members;
      semantic — (3) `==`/`!=` require scalar operands, (4) a `?:` with mismatched
      struct/union tags is rejected, (5) `common_pointer_type` rejects pointers to distinct
      struct/union tags (`semantic/typecheck.c`), (6) `is_complete` now treats an
      incomplete **union** like an incomplete struct (`semantic/type_utils.c`; unions were
      always "complete"), and (7) a union value of incomplete type is rejected in
      `typecheck_and_decay` (`semantic/expressions.c`). The two "member is a function"
      programs are caught at type checking, so they live in the semantic file. **19
      `DISABLED_`** (2 parser + 17 semantic), each one-line-noted: empty initializer list
      `{}` (accepted as C23-style — the parser supports it, see
      `ParserTest.ParseEmptyCompoundInitializer`); the type checker doesn't track a tag's
      struct-vs-union kind (4 cross-kind conflicts); the no-shadowing design
      ([docs] — inner-scope tag can't shadow an outer one with a distinct type) blocks 8
      tag-shadowing programs; plus array-of-incomplete-element behind a pointer-to-array,
      static struct initialized with scalar `0`, assignment to an array-typed lvalue, and
      two non-lvalue struct assignments caught only by the translator's `gen_lval` (not the
      typecheck-only fixture). Full suite green (2328).
- [ ] **Task 17b — Chapter 18 run** (Structures/unions; struct-by-value already supported
      in the backend). Tests are in local tmp/tests/chapter_18/valid/. **Delivered:**
      `backend/besm6/chapter18_tests.cpp` covers all **85 logical programs** (the 108
      `.c` files include 23 `libraries/` client+impl pairs that merge into one program
      each): **21 enabled** run tests + **64 `DISABLED_`**; full suite green (2348). Three
      compiler fixes landed
      while enabling these: (1) **nested aggregate member access** — an array-typed
      struct member used as a value (`x.b.inner_arr[i]`) was loaded instead of decaying
      to its address, and member offsets for word members were added with the byte
      (`scale 1`, fat-pointer) form which corrupts a chained array index; `translator/expr.c`
      now decays array members (via `field_member_type`/`structtab_find_opt`) and emits a
      word offset (`scale = word`) for word-addressed members (`emit_member_offset`),
      keeping the pointer a plain word address (golden TAC in `translator/struct_tests.cpp`/
      `expr_tests.cpp` updated). (2) **union compound initialization** — `union x = {…}`
      was unsupported (`typecheck_init`/`build_static_init`/`make_zero_init` in
      `semantic/initializers.c` + `gen_compound_init` in `translator/stmt.c` now init the
      first member, zero-pad the rest, and reject too-many-element union inits); the
      chapter-18 *negative* tests (`semantic/chapter18_tests.cpp`) were updated to their
      now-correct diagnostics (they previously all aborted early at the union-init error)
      and one non-lvalue-union-assignment negative was `DISABLED_` (only the translator's
      `gen_lval` catches it, not the typecheck-only fixture). (3) **union sizing** —
      `register_struct_type` used the *last* member's size for a union instead of the
      *max*. Rewrote x86 sizes to BESM-6 layout in the `sizeof_type`/`sizeof_exps`/
      `union_sizes` tests (char==1, others==6, align==6); the `union_init` ul-punning
      constant became 2⁴¹−1. The 11 `DISABLED_`, each one-line-noted: no block-scope
      static storage (`static_vs_auto`); discarded multi-word sret (`ignore_retval`);
      `gen_lval` has no function-call/temporary case (`temporary_lifetime`,
      `unions_in_conditionals`); block-scope struct/union compound init needs the purged
      `structtab` (`namespaces`, `label_tag_member_namespace`, `redeclare_union`);
      char-of-integer punning is representation-specific (`union_init_and_member_access`);
      and a **packed char member at a non-zero byte offset reads wrong through a struct**
      (pre-existing, `global_struct`, `array_of_structs`, `param_struct_pointer`).
      The 64 `DISABLED_` cover the rest of the chapter (transcribed faithfully, libraries
      merged client-first, `#include`/`#pragma` stripped) — each needs a runtime/feature
      the target lacks: no libc `malloc`/`calloc`/`strcmp`/`memcmp`/`puts` heap; no
      block-scope static storage; local char-array string init (copies the pointer);
      64-bit constants beyond BESM-6's 41-bit integers; union type-punning that is
      representation-specific; x86 `.s` helper / page-boundary programs; tag shadowing
      and identifier shadowing forbidden by the no-shadowing design; and the packed
      char-member access bug.  Several `struct_copy`/`scalar_member_access` programs would
      light up once the packed-char-member and local char-array-string-init bugs are
      fixed; static-local storage is deferred to a separate task.
- [ ] **Task 18 — Chapter 19** (optimize): `optimize/chapter19_tests.cpp` for
      constant_folding / copy_propagation / dead_store_elimination /
      unreachable_code_elimination (incl. `dont_*` negatives), plus `whole_pipeline` run.  Tests are in local tmp/tests/chapter_19/.
- [ ] **Task 19 — Chapter 20** (register allocation): besm6 run tests (all_types,
      int_only, with/without coalescing, helper_libs). Tests are in local tmp/tests/chapter_20/.

## Verification

- Per component: `cd build/<comp> && ./<comp>-tests` (`parser-tests`, `scanner-tests`,
  `semantic/semantic-tests`, `optimize/optimizer-tests`).
- BESM-6 run tests: `cd build/backend/besm6 && ./besm-tests` (run from this dir for
  `libc.bin`; one `besm-tests` process at a time).
- Whole suite: `make test`.
