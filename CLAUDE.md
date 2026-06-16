# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Test

```sh
make              # configure + build (RelWithDebInfo) into ./build/
make test         # build + run all tests via ctest
make debug        # build with Debug flags
make clean        # remove ./build/
```

Run a single test binary directly (semantic and translator tests live in subdirectories):
```sh
./build/ast/ast-tests
./build/parser-tests
./build/tac/tac-tests
./build/semantic/semantic-tests
./build/translator/translate-tests
./build/backend/besm6/besm-tests
./build/optimize/optimizer-tests
```

Run a specific GoogleTest case:
```sh
./build/parser-tests --gtest_filter="*ExprTest*"
./build/semantic/semantic-tests --gtest_filter="PipelineTest.*"
```

**Run tests from the build directory** to avoid polluting the source tree with temporary
files that GoogleTest writes during test discovery:
```sh
cd build/semantic && ./semantic-tests
ctest --test-dir build -R "Typecheck|Pipeline"
```

**Always run the BESM-6 tests (`besm-tests`) from `build/backend/besm6`** — the directory
that holds the assembled runtime library `libc.bin`. The Dubna simulator job links
`libc.bin` from the current working directory, so running from anywhere else uses a stale
or missing library and the run tests report `ERROR`:
```sh
cd build/backend/besm6 && ./besm-tests
```

**Debugging a failing run with Dubna instruction tracing.** A `CompileAndRun` test leaves
its job file `<TestName>.dub` in `build/backend/besm6`. Re-run it under the simulator with
full instruction/register tracing via `-d c`:

```sh
cd build/backend/besm6
dubna -d c RemainderRun.dub > RemainderRun.trace
```

The trace lists every instruction with the effective address, the memory word read/written,
and the resulting `ACC` / `RAU` (mode register R) / index-register values, e.g.:

```
01065 R: 00 037 0000 ntr
      RAU = 00
01066 L: 16 015 0014 aox 14(16)
      Memory Read [01101] = 6400 0000 0000 0000
      ACC = 6400 0000 0000 0002
      RAU = 04
```

Search the trace for a routine label (e.g. `B/MOD`, `B/DIV`) to follow a runtime helper and
inspect the accumulator/exponent at each step — the fastest way to localize a wrong
mode-bit, exponent, or addressing error in hand-written Madlen. To build a focused job by
hand: assemble a `.mad` with `genbesm`, wrap it with the `*name/*disc/*file:libc,40/*assem
… *library:40/*execute/*end file` boilerplate (see `codegen_test.h` `CompileAndRun`), and
run `dubna [-d c] job.dub`.

Static analysis (requires cppcheck):
```sh
ctest --test-dir build -R cppcheck
```

Try the compiler tools:
```sh
./build/parse input.c               # parse → binary AST to input.ast (NOT stdout)
./build/parse input.c --yaml        # parse → YAML AST
./build/parse input.c --dot         # parse → Graphviz DOT
./build/parse -D input.c            # debug: parser trace + AST dump + leak report

# IMPORTANT: parse and lower do NOT write binary output to stdout — they create a new
# file with an .ast / .tac suffix next to the input.  To emit to stdout (e.g. to pipe or
# redirect), pass "-" as the output argument:
./build/parse input.c -          > /tmp/input.ast   # "-" = binary AST to stdout
./build/parse input.c               # → input.ast (default file output)
./build/lower /tmp/input.ast              # → binary TAC to /tmp/input.tac (default target: besm6)
./build/lower --yaml /tmp/input.ast -     # → YAML TAC to stdout ("-" = stdout)
./build/lower -t x86_64 /tmp/input.ast -  # → TAC with x86_64 type sizes/offsets
./build/lower -D /tmp/input.ast           # debug: translator trace
```

Compiler flags in use: `-Wall -Werror -Wshadow` — all warnings are errors.

## Architecture

This is a multi-platform C11 compiler. The shared frontend emits TAC; machine backends under `backend/` consume TAC and emit target assembly. The pipeline:

```
Source (.c)
  → [parse]      Scanner → Parser → AST (binary/YAML/DOT)
  → [lower]      Typecheck → Translate → Optimize → TAC (binary/YAML/DOT)
  → [genx86]     Frame alloc → Instruction select → GNU AT&T assembly (.s)
  → [genbesm]    Frame alloc → Instruction select → Madlen assembly (.mad)
```

**`parse`** (`parser/main.c`): Lexes and parses a C source file, outputs a binary AST stream (via `wio`) to stdout, or `--yaml`/`--dot` for human-readable forms.

**`lower`** (`translator/main.c`): Reads the binary AST, runs semantic analysis and TAC lowering, then runs the TAC optimizer, and outputs TAC. Lowering is complete. The optimizer runs four passes in a fixed-point loop: constant folding, unreachable code elimination, copy propagation, and dead store elimination. Optimizer flags: `--no-unreachable`, `--no-copy-prop`, `--no-dead-store`, `--opt-debug`. The TAC YAML format is documented in [docs/Technical_Reference.md](docs/Technical_Reference.md).

### Compiler phases

| Phase | Location | Status |
|---|---|---|
| Lexer | `scanner/` | Complete |
| Parser | `parser/` | Complete |
| AST | `ast/` | Complete (alloc/free/export/import/yaml/graphviz/clone/compare) |
| Type checking | `semantic/typecheck.c` | Complete |
| Loop labeling | `semantic/label_loops.c` | Complete |
| Const conversion | `semantic/const_convert.c` | Complete |
| AST → TAC lowering | `translator/translate.c`, `expr.c`, `stmt.c` | Complete |
| TAC optimizer | `optimize/` | Complete (const fold, unreachable elim, copy prop, dead store elim) |
| x86_64 code gen | `backend/x86/` | Planned |
| BESM-6 code gen | `backend/besm6/` | In progress (frame alloc, static data, UTF-8→KOI7, main entry, global variable access, COPY/GET_ADDRESS/LOAD/STORE/BINARY (incl. shifts, unsigned add via b/uadd, unsigned sub via b/usub, multiply via b/mul, unsigned multiply via b/umul, divide via b/div, unsigned divide via b/udiv, remainder via b/mod, unsigned remainder via b/umod, FP add/sub/mul/div inline via a+x/a-x/a*x/a/x with NTR-bracketed normalization, FP comparisons via b/flt/b/fle/b/fgt/b/fge ordering helpers with FP ==/!= reusing b/eq/b/ne)/UNARY negate (int/unsigned/FP)/UNARY complement/UNARY not/FUN_CALL/RETURN/LABEL/JUMP/JUMP_IF_ZERO/JUMP_IF_NOT_ZERO/integer width conversions (TRUNCATE/ZERO_EXTEND/SIGN_EXTEND)/int↔FP conversions (signed INT_TO_DOUBLE/INT_TO_FLOAT inline via INT-format+normalize, UINT_TO_DOUBLE/UINT_TO_FLOAT via b/utod, DOUBLE_TO_INT/FLOAT_TO_INT via b/dtoi, DOUBLE_TO_UINT/FLOAT_TO_UINT via b/dtou, FLOAT_TO_DOUBLE/DOUBLE_TO_FLOAT copies)/ADD_PTR (word pointer & array index scaling; char*/void* byte arithmetic via scale=1 — constant ±1 through b/pinc/b/pdec, other byte deltas through b/padd's floored divide by 6)/COPY_TO_OFFSET/COPY_FROM_OFFSET (aggregate member access — word-aligned members, plus packed char members via the COPY_BYTE_TO_OFFSET/COPY_BYTE_FROM_OFFSET kinds: byte extract for reads, b/stb RMW for writes)/ALLOCATE_LOCAL (contiguous multi-word frame slots for local arrays & structs)/fat-pointer char access (char*/void* byte LOAD inline via WTC/XTA/ASX/AAX, byte STORE via b/stb RMW helper, GET_ADDRESS_BYTE of a char sets the fat marker, int*↔char* casts via PTR_TO_CHAR_PTR/CHAR_PTR_TO_PTR; byte access uses the dedicated LOAD_BYTE/STORE_BYTE/GET_ADDRESS_BYTE kinds; char-array indexing & string/array decay to a fat pointer at offset_enc 5 via GET_ADDRESS_DECAY + static FAT_POINTER init; frontend routes char* +/-/+=/-=/++/-- to ADD_PTR scale=1)/PTR_DIFF (char*−char* difference → ptrdiff_t byte count via b/pdiff: decode both fat pointers to absolute byte positions word*6+(5−offset_enc) and subtract) done; peephole pass (`peephole.c`, run in `codegen_function` before emission) with rule #27 redundant-reload elimination, rule #28 dead temp-store elimination — drops an `atx` to a `%`-temporary slot whose value is never re-read before overwrite/block end; restricted to never-aliased temporaries via `frame_slot_is_temp`, with a single-basic-block guard for temporaries live across an edge — and rule #29 NTR mode coalescing — tracks the mode register R (seeded to 7 after `b/save`), deletes any `ntr n` whose operand equals the current R, and drops a dead `ntr` overwritten by a later `ntr` before any R-dependent use, so consecutive FP ops keep R=0 and restore to 7 once at the end, and rule #30 compare→branch fusion — a relational helper's 0/1 result feeding a `JUMP_IF_ZERO`/`JUMP_IF_NOT_ZERO` is consumed directly by the `uza`/`u1a`; needs no dedicated rule, it is the emergent product of #27+#28 given the runtime helpers' logical-ω exit contract) |
| AArch64 / RISC-V / ARM32 code gen | — | Planned |

### Key data structures

- **`Program`** (`ast/ast.h`): Root node; linked list of `ExternalDecl` (function definition or declaration).
- **`ExternalDecl`**, **`Declaration`**, **`Stmt`**, **`Expr`**, **`Type`**: Core AST nodes. Defined in `ast/ast.h`, spec in `ast/ast.asdl`.
- **`Tac_Instruction`**, **`Tac_Value`**, **`Tac_Type`**: TAC IR nodes. Defined in `tac/tac.h`, spec in `tac/tacky.asdl`.
- **`Besm_Module`**, **`Besm_Func`**, **`Besm_Block`**, **`Besm_Instr`**: BESM-6 backend IR nodes. Defined in `backend/besm6/besm.h`, spec in `backend/besm6/besm6.asdl`.
- **`symtab`** (`semantic/symtab.c/h`): Scoped identifier → `Symbol` map; `symtab_purge(level)` removes block-scope entries on block exit.
- **`structtab`** (`semantic/structtab.c/h`): Struct/union/enum tag → `StructDef` map; scoped, purged on block exit.
- **`typetab`** (`semantic/typetab.c/h`): Typedef name → `TypeDef` map; `typetab_resolve(name)` returns the underlying `Type*`.

### Important design decisions

- **No identifier shadowing**: Inner blocks may not redeclare a name that already exists in any enclosing scope. `symtab` / `structtab` / `typetab` reject duplicates with `fatal_error`. This is a permanent design decision — do not add shadowing support.
- **TAC name convention — frame-resident names start with `%`**: A TAC `var` name encodes its storage class by its first character. `%`+digit is a compiler temporary (`new_temp`); `%`+letter/`_` is a parameter or automatic local; a leading letter/`_`/`$` is a module-level global, static, string constant, or function. Loop and branch-target labels are also `%`-prefixed (`%L`+digit from `label_loops`, or a `%`+digit temporary). `percent_locals_in_function` in `translator/translate.c` (run just before the optimizer) prefixes parameter and automatic-local names — in the body and in the `params`/`locals` lists — with `%`. This lets a backend classify a name without the non-serialized `locals` list: `backend/besm6/frame.c` gives a stack slot to any `%`-prefixed name and treats every other referenced name as an external global. The no-shadowing rule makes the renaming unambiguous. Keep the body and the `params`/`locals` lists consistent (the optimizer's alias analysis matches names against both).
- **`.asdl` files are canonical specs, not code generators.** `ast/ast.asdl` and `tac/tacky.asdl` document the IR; `ast/ast.h` and `tac/tac.h` are maintained manually and must stay in sync.
- **Word I/O (`libutil/wio`)**: AST and TAC binary streams use `size_t`-wide words for portability. Use `wio` for all IR serialization.
- **TAC binary tags (`tac/tags.h`)**: Each TAC node header uses one `size_t`-wide word encoding `TAG_BASE + kind`. Tag constants are readable 4-letter ASCII (e.g. `cnst`, `insr`, `tval`). Stream magic is `TAC2`. See `tac/tags.h`, `tac_export.c`, `tac_import.c`.
- **`xalloc` (`libutil/xalloc`)**: All allocations go through `xalloc`/`xfree`. In debug builds, `xalloc_report()` prints leak totals.
- **Single-pass semantics**: `typecheck_global_decl()` binds names and type-checks in a single pass.
- **Scope tracking**: `scope_level` is incremented on block entry; `scope_decrement()` decrements it and calls `symtab_purge`, `structtab_purge`, and `typetab_purge` — all backed by `map_remove_level_free`.
- **`typedef` handling**: `STORAGE_CLASS_TYPEDEF` declarations are registered in `typetab`. `validate_type` resolves `TYPE_TYPEDEF_NAME` recursively; all type-utility helpers (`get_size`, `get_alignment`, `is_integer`, etc.) resolve typedef names transparently.
- **`switch` semantic validation**: Integer controlling expression with `int` promotion via `convert_to_kind`; constant integer case values evaluated by `try_eval_const_int`; duplicates detected via a `SwitchCtx` stack; multiple defaults and stray case/default labels rejected.
- **TAC lowering coverage**: `translate.c` calls `fatal_error()` on unimplemented constructs. All C11 constructs are now lowered.

### AST quirks

- **Function prototypes vs. definitions**: Only function *definitions* (with a body) parse as `EXTERNAL_DECL_FUNCTION`. A bare prototype such as `int f(int);` parses as `EXTERNAL_DECL_DECLARATION` / `DECL_VAR` with a `TYPE_FUNCTION` declarator type. The typecheck pass (`typecheck_file_scope_var_decl`) detects `TYPE_FUNCTION` and registers it via `symtab_add_fun()`.
- **`f(void)` sentinel**: The parser represents a `(void)` parameter list as a single `Param` node with `TYPE_VOID` and a NULL name (not as an empty list). `typecheck_fn_decl()` strips this sentinel before param processing. Params with a NULL name are skipped when adding to symtab.
- **`_Static_assert` in struct/union bodies**: The parser accepts `_Static_assert` as a struct/union member. The AST uses a `FieldKind` discriminator to distinguish it from regular member declarations.

## Tests

Tests are GoogleTest (C++17). Source lives alongside the module it tests:

- `ast/clone_tests.cpp` → `ast-tests`
- `scanner/tests.cpp` → `scanner-tests`
- `parser/simple_tests.cpp`, `statement_tests.cpp`, … (9 files, including `negative_tests.cpp`) → `parser-tests`
- `tac/yaml_tests.cpp`, `graphviz_tests.cpp`, `binary_tests.cpp` → `tac-tests`
- `semantic/symtab_tests.cpp`, `structtab_tests.cpp`, `typetab_tests.cpp`, `typecheck_tests.cpp`, `real_tests.cpp`, `pipeline_tests.cpp`, `label_loops_tests.cpp`, `const_convert_tests.cpp`, `coercion_tests.cpp` → `semantic-tests`
- `backend/besm6/codegen_tests.cpp`, `arith_tests.cpp`, `copy_tests.cpp`, `flow_tests.cpp`, `run_tests.cpp`, `unary_tests.cpp`, `convert_tests.cpp`, `frame_tests.cpp`, `init_tests.cpp`, `label_tests.cpp`, `ptr_tests.cpp`, `struct_tests.cpp`, `char_tests.cpp`, `peephole_tests.cpp` → `besm-tests`
- `translator/decl_tests.cpp`, `expr_tests.cpp`, `stmt_tests.cpp`, `cast_tests.cpp`, `incdec_tests.cpp`, `switch_tests.cpp`, `ptr_tests.cpp`, `struct_tests.cpp` → `translate-tests`
- `optimize/const_fold_tests.cpp`, `jump_unreachable_tests.cpp`, `copy_prop_tests.cpp`, `dead_store_tests.cpp`, `type_conv_tests.cpp`, `pipeline_tests.cpp` → `optimizer-tests`
- `libutil/string_map_tests.cpp`, `wio_tests.cpp`, `xalloc_tests.cpp` → `libutil-tests`

## Documentation

- [README.md](README.md) — goals, getting started, component overview
- [docs/Technical_Reference.md](docs/Technical_Reference.md) — detailed reference: repo layout, components, build system, TAC YAML format, development notes
- [docs/Memory_Allocation.md](docs/Memory_Allocation.md) — memory allocator (`xalloc`) design and usage
- [docs/String_Map.md](docs/String_Map.md) — `libutil/string_map` key-value store
- [docs/Word_Oriented_IO.md](docs/Word_Oriented_IO.md) — word-oriented I/O (`wio`) for binary IR streams
- [backend/x86/TODO.md](backend/x86/TODO.md) — x86_64 backend work plan with effort estimates
- [backend/besm6/TODO.md](backend/besm6/TODO.md) — BESM-6 backend work plan with effort estimates
- [docs/Besm6_Data_Representation.md](docs/Besm6_Data_Representation.md) — BESM-6 data representation: bit layouts, ranges, and sizeof for every C scalar type
- [docs/Besm6_Calling_Conventions.md](docs/Besm6_Calling_Conventions.md) — BESM-6 C calling convention (registers, b/save, b/ret)
- [docs/Besm6_Instruction_Set.md](docs/Besm6_Instruction_Set.md) — BESM-6 instruction set reference
- [docs/Besm6_Runtime_Library.md](docs/Besm6_Runtime_Library.md) — BESM-6 runtime helper library specifications (`b/save`, `b/mul`, `b/div`, comparisons, etc.)
- [docs/Madlen.md](docs/Madlen.md) — Madlen assembler syntax for the Dubna monitor
- [docs/Type_Coercion.md](docs/Type_Coercion.md) — C11 type coercion and arithmetic conversion rules
- [docs/Type_Sizes_Alignment.md](docs/Type_Sizes_Alignment.md) — type sizes and alignment per target architecture
- [docs/TAC_Optimization.md](docs/TAC_Optimization.md) — machine-independent TAC optimization: constant folding, unreachable code elimination, copy propagation, dead store elimination
- [docs/Peephole_Rewrites.md](docs/Peephole_Rewrites.md) — peephole optimization in the BESM-6 backend: concept, the planned `besm_peephole` pass, and the catalogue of store/reload, NTR, compare/branch, and strength-reduction rewrites (Phase M)
- [docs/C_Grammar.md](docs/C_Grammar.md) — C grammar article: scanner (`c11.l`), parser (`c11.y`), ASDL (`c11.asdl`), and how they relate to the hand-written implementation
- [grammar/README.md](grammar/README.md) — C11 grammar coverage notes
- [grammar/c11.y](grammar/c11.y), [grammar/c11.l](grammar/c11.l), [grammar/c11.asdl](grammar/c11.asdl) — reference grammar and abstract syntax (not used for code generation)
