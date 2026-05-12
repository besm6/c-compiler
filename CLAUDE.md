# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Test

```sh
make              # configure + build (RelWithDebInfo) into ./build/
make test         # build + run all tests via ctest
make debug        # build with Debug flags
make clean        # remove ./build/
```

Run a single test binary directly (translator tests live in a subdirectory):
```sh
./build/ast/ast-tests
./build/parser-tests
./build/translator/typecheck-tests
./build/translator/symtab-tests
./build/translator/structtab-tests
```

Run a specific GoogleTest case:
```sh
./build/parser-tests --gtest_filter="*ExprTest*"
./build/translator/typecheck-tests --gtest_filter="PipelineTest.*"
```

**Run tests from the build directory** to avoid polluting the source tree with temporary
files that GoogleTest writes during test discovery:
```sh
cd build/translator && ./typecheck-tests
ctest --test-dir build -R "Typecheck|Pipeline"
```

Static analysis (requires cppcheck):
```sh
ctest --test-dir build -R cppcheck
```

Try the compiler tools:
```sh
./build/cast input.c               # parse → binary AST to stdout
./build/cast input.c --yaml        # parse → YAML AST
./build/cast input.c --dot         # parse → Graphviz DOT
./build/cast -D input.c            # debug: parser trace + AST dump + leak report

# tacker requires a filename — write AST to a file first, then pass it:
./build/cast input.c > /tmp/input.ast
./build/tacker /tmp/input.ast           # → binary TAC to stdout
./build/tacker --yaml /tmp/input.ast    # → YAML TAC
./build/tacker -D /tmp/input.ast        # debug: translator trace
```

Compiler flags in use: `-Wall -Werror -Wshadow` — all warnings are errors.

## Architecture

This is a C11 frontend compiler targeting the BESM-6 architecture. The backend is not yet implemented. The pipeline has two executables:

```
Source (.c)
  → [cast]    Scanner → Parser → AST (binary/YAML/DOT)
  → [tacker]  Resolve → Typecheck → LabelLoops → Translate → TAC (binary/YAML/DOT)
```

**`cast`** (`parser/main.c`): Lexes and parses a C source file, outputs a binary AST stream (via `wio`) to stdout, or `--yaml`/`--dot` for human-readable forms.

**`tacker`** (`translator/main.c`): Reads the binary AST, runs semantic analysis and (partial) TAC lowering, outputs TAC.

### Compiler phases

| Phase | Location | Status |
|---|---|---|
| Lexer | `scanner/` | Complete |
| Parser | `parser/` | Complete for C11 subset |
| Name resolution | `translator/resolve.c` | Complete |
| Type checking | `translator/typecheck.c` | Mostly complete |
| Loop labeling | `translator/translator.c` | Complete |
| AST → TAC lowering | `translator/translate_gen.c` | Partial |
| BESM-6 code gen | — | Not started |

### Key data structures

- **`Program`** (`ast/ast.h`): Root node; linked list of `ExternalDecl` (function definition or declaration).
- **`ExternalDecl`**, **`Declaration`**, **`Stmt`**, **`Expr`**, **`Type`**: Core AST nodes. Defined in `ast/ast.h`, spec in `ast/ast.asdl`.
- **`Tac_Instruction`**, **`Tac_Value`**, **`Tac_Type`**: TAC IR nodes. Defined in `tac/tac.h`, spec in `tac/tacky.asdl`.
- **`symtab`** (`translator/symtab.c/h`): Scoped identifier → `Symbol` map. `symtab_purge(level)` removes block-scope entries on block exit.
- **`structtab`** (`translator/structtab.c/h`): Struct/union/enum registration.

### Important design decisions

- **`.asdl` files are canonical specs, not code generators.** `ast/ast.asdl` and `tac/tacky.asdl` document the IR; `ast/ast.h` and `tac/tac.h` are maintained manually and must stay in sync.
- **Word I/O (`libutil/wio`)**: AST and TAC binary streams use `size_t`-wide words for portability. Use `wio` for all IR serialization.
- **`xalloc` (`libutil/xalloc`)**: All allocations go through `xalloc`/`xfree`. In debug builds, `xalloc_report()` prints leak totals.
- **Two-pass semantics**: `resolve()` binds names, then `typecheck_global_decl()` type-checks. TODO.md tracks merging them into one pass.
- **Scope tracking**: `scope_level` is incremented on block entry and `symtab_purge(scope_level)` + decrement on exit.
- **Known bug — `map_remove_level` dealloc**: `map_remove_level()` in `string_map.c` frees AVL node structs but does **not** call the dealloc callback on `node->value`. As a result, `symtab_purge()` leaks `Symbol` pointers for block-scope entries (e.g. named function params added by `resolve()` at scope level 1 and purged on exit). See TODO.md item 3.
- **Partial TAC lowering**: `translate_gen.c` calls `fatal_error()` on unimplemented constructs. Many `Expr` and `Stmt` kinds are not yet handled.

### AST quirks

- **Function prototypes vs. definitions**: Only function *definitions* (with a body) parse as `EXTERNAL_DECL_FUNCTION`. A bare prototype such as `int f(int);` parses as `EXTERNAL_DECL_DECLARATION` / `DECL_VAR` with a `TYPE_FUNCTION` declarator type. The typecheck pass (`typecheck_file_scope_var_decl`) detects `TYPE_FUNCTION` and registers it via `symtab_add_fun()`.
- **`f(void)` sentinel**: The parser represents a `(void)` parameter list as a single `Param` node with `TYPE_VOID` and a NULL name (not as an empty list). `typecheck_fn_decl()` strips this sentinel before param processing. `resolve_function_declaration()` skips params with a NULL name when adding to symtab.

### Serialization pipeline

Binary AST flows between tools via stdout/stdin using `wio`. `ast_export.c` writes and `ast_import.c` reads. The YAML and DOT outputs are for debugging only and cannot be re-imported.

## Tests

Tests are GoogleTest (C++17). Source lives alongside the module it tests:

- `ast/clone_tests.cpp` → `ast-tests`
- `scanner/tests.cpp` → `scanner-tests`
- `parser/simple_tests.cpp`, `statement_tests.cpp`, … (8 files) → `parser-tests`
- `translator/symtab_tests.cpp`, `structtab_tests.cpp`, `typecheck_tests.cpp` → 3 separate executables
- `libutil/string_map_tests.cpp`, `wio_tests.cpp` → `libutil-tests`, `wio-tests`

Two test files are disabled in CMake (known failures): `parser/negative_tests.cpp` and `translator/const_convert_tests.cpp`.

## Documentation

- [README.md](README.md) — goals, getting started, component overview
- [docs/TECHNICAL.md](docs/TECHNICAL.md) — detailed reference: repo layout, ASDL, build system, development notes
- [TODO.md](TODO.md) — work plan with 7 major tasks and effort estimates
- [grammar/README.md](grammar/README.md) — C11 grammar coverage notes
- [grammar/c11.y](grammar/c11.y), [grammar/c11.l](grammar/c11.l) — reference grammar (not used for code generation)
