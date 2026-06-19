# TODO

Work plan for the BESM-6 backend.
The backend consumes binary TAC produced by `lower` and emits Madlen assembly (`.mad` files) for the Dubna monitor system.

### Phase N — Backend

| #  | Task | Description |
|----|------|-------------|
| 38 | Assembler for Unix | TODO |
| 39 | Linker for Unix | TODO |
| 40 | Tests from the book | Port tests from the Writing a C Compiler book |

### Phase X — Deferred / future

| #   | Task | Description |
|-----|------|-------------|
| 103 | Switch jump-table optimization | For dense case ranges, replace the linear compare chain with an index-scaled `utc` / `uj` dispatch through a table of `,log, label` words. |
| 104 | Register allocator | Use r1-r5 for word pointers. |
