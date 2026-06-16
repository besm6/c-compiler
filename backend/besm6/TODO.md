# TODO

Work plan for the BESM-6 backend.
The backend consumes binary TAC produced by `lower` and emits Madlen assembly (`.mad` files) for the Dubna monitor system.

### Phase N — Backend

| #  | Task | Description |
|----|------|-------------|
| 38 | Assembler for Unix | TODO |
| 39 | Linker for Unix | TODO |
| 40 | Tests from the book | Port tests from the Writing a C Compiler book |
| 41 | Get rid of DeclareArray | `DeclareArray` TAC top-level (`tac/tacky.asdl` / `tac/tac.h`): records an `extern T name[]` so a backend knows the name decays to its label address  (cross-module array indexing); `global_is_array` consults it. Why do we need it? Fix translator instead. |

### Phase X — Deferred / future

| #   | Task | Description |
|-----|------|-------------|
| 101 | Two-word `long long` / `unsigned long long` | Two-word load/store and software add/sub/mul/div/compare. First fix `codegen_sizeof` in [abi.h](abi.h), which currently returns 1 word for these two-word types. |
| 102 | Two-word `long double` | Two-word native-FP arithmetic (80-bit mantissa, 14-bit exponent biased 8192) via runtime helpers, using the Y/RMR register for double-width intermediates. |
| 103 | Switch jump-table optimization | For dense case ranges, replace the linear compare chain with an index-scaled `utc` / `uj` dispatch through a table of `,log, label` words. |
| 104 | Register allocator | Use r1-r5 for word pointers. |
