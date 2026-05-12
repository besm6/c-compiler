# Technical reference: BESM-6 C compiler

This document lists repository layout, build details, components, tests, and development notes. The [README](../README.md) is the overview for new contributors.

## Repository layout

```
c-compiler/
├── ast/                 # AST: types, alloc, import/export, YAML, Graphviz, print, clone, compare, free
├── docs/                # Project documentation (this file)
├── grammar/             # C11 Yacc/Lex/ASDL reference; see grammar/README.md
├── libutil/             # xalloc, wio, string_map
├── parser/              # Recursive-descent parser, nametab; cast driver
├── scanner/             # Hand-written lexer
├── scripts/             # googletest.xml (cppcheck), validate_asdl.py
├── tac/                 # TAC IR: alloc, print, free, compare (export/import TODO in CMake)
├── translator/          # resolve, typecheck, symtab, typetab; tacker driver
├── translator/attic/    # Older experiments (not linked by CMake)
├── CMakeLists.txt       # Root CMake project (project name: c-scanner)
├── Makefile             # Convenience: mkdir build, cmake, make test
└── LICENSE
```

## Executables: `cast` and `tacker`

Both tools are built from the root `CMakeLists.txt`. Install or run them from the build directory (e.g. `./build/cast`).

### `cast` (parser)

**Input:** one C source file (preprocessor is not implemented; feed the compiler preprocessed C if you rely on `#include` / `#define`).

**Output:** one of:

- Binary AST (default): `--ast` or omit format flag; file extension often `.ast`.
- `--yaml` — YAML dump of the AST.
- `--dot` — Graphviz DOT for structure visualization.

**Options** (see `parser/main.c`): `--ast`, `--yaml`, `--dot`, `-v` / `--verbose`, `-D` / `--debug`, `-h` / `--help`.

**Examples:**

```bash
cast input.c output.ast
cast --yaml input.c output.yaml
cast --dot input.c output.dot
cast input.c -                 # stdout
```

### `tacker` (translator driver)

**Input:** binary AST stream as produced by `cast` (opened with `ast_import_open` / `import_external_decl`).

**Processing order** (per top-level declaration): `resolve` → `typecheck_global_decl` → `label_loops` → `translate` → emit.

**Current limitations:**

- `translate()` in `translator/translator.c` is a stub: it returns `NULL`, so no TAC is produced for normal runs.
- `label_loops()` assigns break/continue target labels on loop and switch statements.
- `emit_tac_toplevel()` in `translator/main.c` only contains `TODO` comments for binary, YAML, and DOT export, so **no TAC is written** to the output file even if translation existed.
- The `tac` library does not yet build `tac_export.c`, `tac_import.c`, `tac_yaml.c`, or `tac_graphviz.c` (they are commented out in `tac/CMakeLists.txt`).

**Options:** `--tac`, `--yaml`, `--dot`, `-v`, `-D`, `-h` (see `translator/main.c`).

**Debug (`-D`):** enables translator/import/export/wio debug flags and, when TAC exists, could print TAC via `print_tac_toplevel`; also prints imported AST with `print_external_decl` before analysis.

## Components

### Scanner (`scanner/`)

Hand-written lexer. Token set follows C11-style tokens for preprocessed source.

| File | Role |
|------|------|
| `scanner.h`, `scanner.c` | Token definitions and lexer |
| `tests.cpp` | GoogleTest suite (`scanner-tests`) |

### Parser (`parser/`)

Recursive-descent parser guided by the C11 grammar in `grammar/` (not generated from Yacc). Builds AST nodes and manages a name table for ordinary identifiers.

| File | Role |
|------|------|
| `parser.h`, `parser.c` | Parser API and implementation |
| `nametab.c` | Identifier name table |
| `main.c` | `cast` entry: `parse` → `export_ast` / `export_yaml` / `export_dot` |
| `fixture.h` | Test helpers |

Parser tests: `simple_tests.cpp`, `statement_tests.cpp`, `operator_tests.cpp`, `type_tests.cpp`, `struct_tests.cpp`, `declaration_tests.cpp`, `constant_tests.cpp`, `serialize_tests.cpp` → `parser-tests`. `negative_tests.cpp` is present but **commented out** in `parser/CMakeLists.txt`.

### AST (`ast/`)

AST values are implemented in C (`ast.h` and companion `.c` files). Binary serialization and YAML/DOT export are used by `cast` and by `tacker` when importing ASTs.

| File | Role |
|------|------|
| `ast.asdl` | Canonical IR description (not auto-generated into C by the build) |
| `ast.h`, `internal.h`, `tags.h` | Types and internals |
| `ast_alloc.c`, `ast_free.c` | Allocation and free |
| `ast_export.c`, `ast_import.c` | Binary wire format |
| `ast_yaml.c`, `ast_graphviz.c` | YAML and DOT |
| `ast_print.c`, `ast_clone.c`, `ast_compare.c` | Print, clone, compare |

A stray file `clone_tests.cpp-` in `ast/` is not part of the CMake build.

### Translator (`translator/`)

| File | Role |
|------|------|
| `translator.h`, `translator.c` | `fatal_error`, `label_loops`, `translate` (stub) |
| `resolve.c` | Identifier and type resolution |
| `typecheck.c` | Type checking and related transforms |
| `symtab.c`, `symtab_print.c` | Symbol table |
| `typetab.c`, `typetab_print.c` | Type table |
| `type_utils.c` | Type helpers |
| `const_convert.c` | Constant conversions |
| `main.c` | `tacker` entry |

Tests: `symtab_tests.cpp`, `typetab_tests.cpp`, `typecheck_tests.cpp`. The file `const_convert_tests.cpp` exists but is **not** registered in `translator/CMakeLists.txt`.

The `translator/attic/` directory holds older experiments (e.g. alternate TAC generation); it is not linked into the main libraries.

### TAC (`tac/`)

| File | Role |
|------|------|
| `tacky.asdl` | Canonical TAC description |
| `tac.h` | TAC structs and enums |
| `tac_alloc.c`, `tac_free.c` | Allocation |
| `tac_print.c` | Human-readable TAC printing |
| `tac_compare.c` | Structural comparison |

Planned but not built in CMake yet: `tac_export.c`, `tac_import.c`, `tac_yaml.c`, `tac_graphviz.c`.

### Grammar (`grammar/`)

Reference grammars and notes. See [grammar/README.md](../grammar/README.md) for the relationship between Yacc, Lex, and ASDL files (`c11.y`, `c11.l`, `c11.asdl`).

### Utilities (`libutil/`)

| Module | Files | Purpose |
|--------|--------|---------|
| **xalloc** | `xalloc.c`, `xalloc.h` | Tracked allocation, `xfree_all`, leak reporting in debug paths |
| **wio** | `wio.c`, `wio.h` | Binary I/O for AST (and future TAC) streams |
| **string_map** | `string_map.c`, `string_map.h` | Map used in symbol and type tables |

Tests: `string_map_tests.cpp` → `libutil-tests`; `wio_tests.cpp` → `wio-tests`.

### Scripts (`scripts/`)

| File | Purpose |
|------|---------|
| `googletest.xml` | cppcheck library hints for GoogleTest macros |
| `validate_asdl.py` | Optional ASDL validation (requires Python package `pyasdl`): `python3 scripts/validate_asdl.py path/to/file.asdl` |

## Build system

- **CMake** minimum 3.10; root project name: `c-scanner`.
- **C** standard: C11; **C++** for tests: C++17.
- **Compiler flags:** `-Wall -Werror -Wshadow` for C++ (see root `CMakeLists.txt`).
- **GoogleTest:** FetchContent, tag `v1.15.2`, `BUILD_GMOCK=OFF`.
- **cppcheck:** If `cppcheck` is found, it is attached to C and C++ targets with project-specific suppressions and `scripts/googletest.xml` for tests.
- **Makefile:** Creates `build/`, runs `cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo`, delegates `all` to `$(MAKE) -C build`. Targets: `make`, `make test`, `make clean`, `make debug` (cmake Debug build into `build`).

Common build types: `Debug`, `RelWithDebInfo`, `Release`.

## ASDL and C code

The `.asdl` files (`ast/ast.asdl`, `tac/tacky.asdl`, `grammar/c11.asdl`) describe the intended shape of the AST and TAC. The **CMake build does not generate C headers from ASDL**; `ast.h` and `tac.h` are maintained manually to match those specs. Use `scripts/validate_asdl.py` if you change ASDL and want a quick parse check.

## Testing

Run all tests:

```bash
make test
# or
ctest --test-dir build
```

Test executables and their sources:

| Executable | Sources (under repo root) |
|------------|---------------------------|
| `scanner-tests` | `scanner/tests.cpp` |
| `parser-tests` | `parser/simple_tests.cpp`, …, `serialize_tests.cpp` |
| `libutil-tests` | `libutil/string_map_tests.cpp` |
| `wio-tests` | `libutil/wio_tests.cpp` |
| `symtab-tests` | `translator/symtab_tests.cpp` |
| `typetab-tests` | `translator/typetab_tests.cpp` |
| `typecheck-tests` | `translator/typecheck_tests.cpp` |

Disabled or unwired: `parser/negative_tests.cpp` (commented out in CMake); `translator/const_convert_tests.cpp` (not in CMake); `ast/clone_tests.cpp-` (not a valid registered test file).

Run a single binary from `build/`:

```bash
./build/parser-tests
./build/typecheck-tests
```

## Development notes

### Memory

`xalloc` tracks allocations; `xfree_all()` frees everything in bulk at shutdown. Debug paths can report leaks (see `xalloc` usage with `-D` in the tools).

### Debugging

- **`cast -D`:** parser debug, AST pretty-print to stdout before export, import/export/wio debug, `xreport_lost_memory` at end.
- **`tacker -D`:** translator debug, AST print on import, import/export/wio debug; would print TAC with `print_tac_toplevel` if `translate()` returned non-NULL.

### Visualization

AST DOT export works:

```bash
cast --dot input.c ast.dot
dot -Tpng ast.dot -o ast.png
```

TAC DOT/YAML export is not implemented in the emitter until `emit_tac_toplevel` and TAC export code exist.

## References

- **Book:** [Nora Sandler, *Writing a C Compiler*](https://nostarch.com/writing-c-compiler) — pedagogical pipeline similar to this project’s stages.
- **C11 grammar:** Yacc/Lex heritage (e.g. Jeff Lee’s ANSI C grammar) updated toward C11; see `grammar/`.
- **Unix v7 on BESM-6:** [v7besm](https://github.com/besm6/v7besm)
- **Dubna monitor:** [dubna](https://github.com/besm6/dubna)
