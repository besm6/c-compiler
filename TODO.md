# TODO

Work plan, ordered by recommended implementation sequence.

## TAC lowering gaps

| # | Task | Description | Effort |
|---|------|-------------|--------|
| 1 | `LITERAL_ENUM` in `gen_expr` | Add a `LITERAL_ENUM` case in the `EXPR_LITERAL` switch in `translator/expr.c`. Enum constants carry an `ident enum_const` field; look up the integer value already resolved by typecheck and return `val_int`. | S |
| 2 | Compound local-variable initializers | Extend `gen_local_decl` in `translator/stmt.c` to walk `INITIALIZER_COMPOUND` lists and emit `copy_to_offset` instructions for each element, using field offsets from `structtab`. | M |
| 3 | Indirect function calls | Remove the `fatal_error` at `translator/expr.c:487`. When `func->kind != EXPR_VAR`, call `gen_expr` to load the function-pointer value and use it as the callee in `TAC_INSTRUCTION_FUN_CALL`. | M |
| 4 | `EXPR_GENERIC` type-check | Add an `EXPR_GENERIC` case to `typecheck_expr` in `semantic/typecheck.c`. Type-check the controlling expression and select the matching `GenericAssoc` by comparing types. | M |
| 5 | `EXPR_GENERIC` TAC lowering | Add an `EXPR_GENERIC` case in `gen_expr` (`translator/expr.c`). After typecheck has selected the matching branch, lower only that branch's expression. | S |
| 6 | `EXPR_COMPOUND` type-check | Add an `EXPR_COMPOUND` case to `typecheck_expr`. Treat it as a synthetic local of the specified type initialized with the given initializer list. | M |
| 7 | `EXPR_COMPOUND` TAC lowering | Add an `EXPR_COMPOUND` case in `gen_expr`. Allocate a temp, emit initializer stores (reuse compound-init logic from task 2), and return the address or value. | M |

## Disabled / unregistered tests

| # | Task | Description | Effort |
|---|------|-------------|--------|
| 8 | Register `semantic/const_convert_tests.cpp` | Add to `semantic/CMakeLists.txt` (or a dedicated target); fix any failures. | S |
| 9 | Re-enable `parser/negative_tests.cpp` | Uncomment in `parser/CMakeLists.txt`; investigate and fix the underlying parser failures. | M |

## Parser gaps

| # | Task | Description | Effort |
|---|------|-------------|--------|
| 10 | `_Static_assert` in struct/union member lists | `parser/parser.c:1835` parses the construct but discards the result. Add a `FIELD_STATIC_ASSERT` node kind (or equivalent under `Declaration`) and handle it in typecheck. | M |

## BESM-6 backend (future)

| # | Task | Description | Effort |
|---|------|-------------|--------|
| 12 | BESM-6 machine model | Define the register file, instruction encoding, and calling convention in `backend/besm6.h`. | L |
| 13 | Instruction selection | Walk each `Tac_Instruction` and emit the corresponding BESM-6 instruction(s). Start with arithmetic, load/store, and control flow. | XL |
| 14 | Register allocation | Implement a register allocator (linear scan or graph colouring) over TAC temporaries, targeting BESM-6's accumulator-based architecture. | XL |
| 15 | Assembly / object output | Write output in a format consumable by the Dubna monitor or a BESM-6 assembler. | L |
| 16 | Preprocessor | Integrate an existing CPP (e.g. `mcpp`) or implement a minimal `#include` / `#define` / `#if` pass as a separate tool. | XL |
