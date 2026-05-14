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
├── semantic/            # symtab, structtab, typetab, typecheck, label_loops, const_convert
├── tac/                 # TAC IR: alloc, print, free, compare, export/import, YAML, Graphviz
├── translator/          # AST→TAC lowering (translate, expr, stmt); tacker driver
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

**TAC lowering status:** Mostly complete. Arithmetic, control flow, direct function calls, pointers, arrays, structs/unions, and type casts all lower correctly. Known remaining gaps: enum constants as expressions (`LITERAL_ENUM`), compound initializers for local aggregates (`INITIALIZER_COMPOUND`), indirect function-pointer calls, `_Generic` selection, and compound literals. See [TODO.md](../TODO.md) for the full task list.

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

### Semantic analysis (`semantic/`)

| File | Role |
|------|------|
| `symtab.c`, `symtab.h` | Scoped identifier → Symbol map |
| `structtab.c`, `structtab.h` | Scoped struct/union/enum tag → StructDef map |
| `typetab.c`, `typetab.h` | Scoped typedef name → TypeDef map |
| `typecheck.c`, `typecheck.h` | Type checking and name binding (single-pass) |
| `label_loops.c` | Annotates loop/switch statements with break/continue jump targets |
| `type_utils.c` | Type helpers: `get_size`, `get_alignment`, `is_integer`, etc. |
| `const_convert.c` | Constant-expression evaluation and conversion |

Tests: `symtab_tests.cpp` → `symtab-tests`; `structtab_tests.cpp` → `structtab-tests`; `typetab_tests.cpp` → `typetab-tests`; `typecheck_tests.cpp` → `typecheck-tests`. The file `const_convert_tests.cpp` exists but is not yet registered in `semantic/CMakeLists.txt`.

### Translator (`translator/`)

| File | Role |
|------|------|
| `translate.h`, `translate.c` | Shared helpers, type conversion, top-level entry points |
| `expr.c` | AST `Expr` → TAC instruction lowering |
| `stmt.c` | AST `Stmt` → TAC instruction lowering; local declaration init |
| `main.c` | `tacker` entry: import → semantic passes → translate → emit |

Tests: `decl_tests.cpp`, `expr_tests.cpp`, `stmt_tests.cpp`, `cast_tests.cpp`, `incdec_tests.cpp`, `switch_tests.cpp`, `ptr_tests.cpp`, `struct_tests.cpp` → `translate-tests`.

The `translator/attic/` directory holds older experiments; it is not linked into the main libraries.

### TAC (`tac/`)

| File | Role |
|------|------|
| `tacky.asdl` | Canonical TAC description |
| `tac.h` | TAC structs and enums |
| `tac_alloc.c`, `tac_free.c` | Allocation and free |
| `tac_print.c` | Human-readable TAC printing |
| `tac_compare.c` | Structural comparison |
| `tac_export.c`, `tac_import.c` | Binary wire format (read/write via `wio`) |
| `tac_yaml.c` | YAML listing (debug/test; not re-importable) |
| `tac_graphviz.c` | Graphviz DOT output |

### TAC YAML format

`tac_export_yaml()` (`tac/tac_yaml.c`) emits one `- toplevel:` block per call. Indentation is 2 spaces per level. **Not re-importable** — debug/test use only.

#### Toplevel kinds

```yaml
- toplevel:
  kind: function
  name: f
  global: true          # false for static
  params:               # omitted when empty
    - param: x
  body:                 # omitted for prototypes
    - instruction:
      kind: ...

- toplevel:
  kind: static_variable
  name: g
  global: true
  type:
    kind: int
  init_list:            # omitted when absent (tentative definition)
    - init:
      kind: i32
      value: 42

- toplevel:
  kind: static_constant  # used for string literals
  name: _str0
  type:
    kind: array
    elem_type:
      kind: char
    size: 6
  init:
    kind: string
    value: hello
    null_terminated: true
```

#### Values — appear under `src:`, `dst:`, `condition:`, `args:`

```yaml
kind: var
name: x

kind: constant
const:
  kind: int      # int | long | long_long | uint | ulong | ulong_long | double | char | uchar
  value: 42      # double uses %a (hex float) format
```

**Instructions** (all have `- instruction:` header; fields follow at +2 indent)

| `kind:` | Additional fields |
|---------|-------------------|
| `return` | `src:` val (omitted for void return) |
| `copy` | `src:` `dst:` |
| `sign_extend` | `src:` `dst:` |
| `zero_extend` | `src:` `dst:` |
| `truncate` | `src:` `dst:` |
| `int_to_double` | `src:` `dst:` |
| `uint_to_double` | `src:` `dst:` |
| `double_to_int` | `src:` `dst:` |
| `double_to_uint` | `src:` `dst:` |
| `unary` | `op:` `src:` `dst:` |
| `binary` | `op:` `src1:` `src2:` `dst:` |
| `get_address` | `src:` `dst:` |
| `load` | `src_ptr:` `dst:` |
| `store` | `src:` `dst_ptr:` |
| `add_ptr` | `ptr:` `index:` `scale: N` `dst:` |
| `copy_to_offset` | `src:` `dst: name` (bare string) `offset: N` |
| `copy_from_offset` | `src: name` (bare string) `offset: N` `dst:` |
| `jump` | `target: label` |
| `jump_if_zero` | `condition:` `target: label` |
| `jump_if_not_zero` | `condition:` `target: label` |
| `label` | `name: label` |
| `fun_call` | `fun_name: f` `args:` list (omitted when none) `dst:` (omitted for void) |

Unary ops: `complement`, `negate`, `not`.
Binary ops: `add`, `subtract`, `multiply`, `divide`, `remainder`, `equal`, `not_equal`, `less_than`, `less_or_equal`, `greater_than`, `greater_or_equal`, `bitwise_and`, `bitwise_or`, `bitwise_xor`, `left_shift`, `right_shift`.

**Types** (appear under `type:`, `elem_type:`, `target:`, `ret_type:`, `param_types:`)

```yaml
kind: int | uint | long | ulong | long_long | ulong_long
     | char | schar | uchar | short | ushort | float | double | void

kind: pointer
target:
  kind: ...

kind: array
elem_type:
  kind: ...
size: N

kind: fun_type
param_types:        # omitted when none
  - type:
    kind: ...
ret_type:
  kind: ...

kind: structure
tag: MyStruct
```

**Static init kinds** (list items under `init_list:`, single item under `init:`)

| `kind:` | Fields |
|---------|--------|
| `i8` / `i32` / `i64` | `value:` (signed) |
| `u8` / `u32` / `u64` | `value:` (unsigned) |
| `double` | `value:` (hex float) |
| `zero` | `bytes: N` |
| `string` | `value:` `null_terminated: true\|false` |
| `pointer` | `name:` |

### Grammar (`grammar/`)

Reference grammars and notes. See [grammar/README.md](../grammar/README.md) for the relationship between Yacc, Lex, and ASDL files (`c11.y`, `c11.l`, `c11.asdl`).

### Utilities (`libutil/`)

| Module | Files | Purpose |
|--------|--------|---------|
| **xalloc** | `xalloc.c`, `xalloc.h` | Tracked allocation, `xfree_all`, leak reporting in debug paths |
| **wio** | `wio.c`, `wio.h` | Binary I/O for AST and TAC streams |
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
| `parser-tests` | `parser/simple_tests.cpp`, …, `serialize_tests.cpp` (8 files) |
| `ast-tests` | `ast/clone_tests.cpp` |
| `libutil-tests` | `libutil/string_map_tests.cpp` |
| `wio-tests` | `libutil/wio_tests.cpp` |
| `symtab-tests` | `semantic/symtab_tests.cpp` |
| `structtab-tests` | `semantic/structtab_tests.cpp` |
| `typetab-tests` | `semantic/typetab_tests.cpp` |
| `typecheck-tests` | `semantic/typecheck_tests.cpp` |
| `tac-yaml-tests` | `tac/tac_yaml_tests.cpp` |
| `tac-dot-tests` | `tac/tac_graphviz_tests.cpp` |
| `tac-binary-tests` | `tac/tac_binary_tests.cpp` |
| `translate-tests` | `translator/decl_tests.cpp`, `expr_tests.cpp`, `stmt_tests.cpp`, `cast_tests.cpp`, `incdec_tests.cpp`, `switch_tests.cpp`, `ptr_tests.cpp`, `struct_tests.cpp` |

Disabled or unwired: `parser/negative_tests.cpp` (commented out in CMake); `semantic/const_convert_tests.cpp` (not registered in CMake).

Run a single binary from `build/`:

```bash
./build/parser-tests
./build/semantic/typecheck-tests
./build/translator/translate-tests
```

## Development notes

### Memory

`xalloc` tracks allocations; `xfree_all()` frees everything in bulk at shutdown. Debug paths can report leaks (see `xalloc` usage with `-D` in the tools).

### Debugging

- **`cast -D`:** parser debug, AST pretty-print to stdout before export, import/export/wio debug, `xreport_lost_memory` at end.
- **`tacker -D`:** translator debug, AST print on import, import/export/wio debug; prints TAC with `print_tac_toplevel` after lowering.

### Visualization

AST DOT export works:

```bash
cast --dot input.c ast.dot
dot -Tpng ast.dot -o ast.png
```

TAC DOT and YAML export work the same way — pass `--dot` or `--yaml` to `tacker`:

```bash
tacker --yaml input.ast -        # YAML TAC to stdout
tacker --dot input.ast tac.dot
dot -Tpng tac.dot -o tac.png
```

## References

- **Book:** [Nora Sandler, *Writing a C Compiler*](https://nostarch.com/writing-c-compiler) — pedagogical pipeline similar to this project’s stages.
- **C11 grammar:** Yacc/Lex heritage (e.g. Jeff Lee’s ANSI C grammar) updated toward C11; see `grammar/`.
- **Unix v7 on BESM-6:** [v7besm](https://github.com/besm6/v7besm)
- **Dubna monitor:** [dubna](https://github.com/besm6/dubna)
