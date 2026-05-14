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
./build/tac/tac-yaml-tests
./build/tac/tac-dot-tests
./build/tac/tac-binary-tests
./build/semantic/typecheck-tests
./build/semantic/symtab-tests
./build/semantic/structtab-tests
./build/semantic/typetab-tests
./build/translator/translate-tests
```

Run a specific GoogleTest case:
```sh
./build/parser-tests --gtest_filter="*ExprTest*"
./build/semantic/typecheck-tests --gtest_filter="PipelineTest.*"
```

**Run tests from the build directory** to avoid polluting the source tree with temporary
files that GoogleTest writes during test discovery:
```sh
cd build/semantic && ./typecheck-tests
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
./build/tacker /tmp/input.ast           # → binary TAC to /tmp/input.tac
./build/tacker --yaml /tmp/input.ast    # → YAML TAC to /tmp/input.yaml (NOT stdout)
./build/tacker -D /tmp/input.ast        # debug: translator trace

# tacker writes output to a derived filename, never to stdout.
# The output file is the input path with the extension replaced:
#   /tmp/input.ast --yaml  →  /tmp/input.yaml
#   /tmp/input.ast         →  /tmp/input.tac
# To print to stdout, pass "-" as the output argument:
./build/tacker --yaml /tmp/input.ast -  # → YAML TAC to stdout
```

Compiler flags in use: `-Wall -Werror -Wshadow` — all warnings are errors.

## Architecture

This is a C11 frontend compiler targeting the BESM-6 architecture. The backend is not yet implemented. The pipeline has two executables:

```
Source (.c)
  → [cast]    Scanner → Parser → AST (binary/YAML/DOT)
  → [tacker]  Typecheck → LabelLoops → Translate → TAC (binary/YAML/DOT)
```

**`cast`** (`parser/main.c`): Lexes and parses a C source file, outputs a binary AST stream (via `wio`) to stdout, or `--yaml`/`--dot` for human-readable forms.

**`tacker`** (`translator/main.c`): Reads the binary AST, runs semantic analysis and TAC lowering, outputs TAC. Lowering is mostly complete; see the phases table for remaining gaps.

### Compiler phases

| Phase | Location | Status |
|---|---|---|
| Lexer | `scanner/` | Complete |
| Parser | `parser/` | Complete for C11 subset; `negative_tests.cpp` disabled |
| AST | `ast/` | Complete (alloc/free/export/import/yaml/graphviz/clone/compare) |
| Type checking | `semantic/typecheck.c` | Mostly complete; `EXPR_GENERIC` and `EXPR_COMPOUND` not yet handled |
| Loop labeling | `semantic/label_loops.c` | Complete |
| Const conversion | `semantic/const_convert.c` | Complete; `const_convert_tests.cpp` not yet registered in CMake |
| AST → TAC lowering | `translator/translate.c`, `expr.c`, `stmt.c` | Mostly complete; gaps: `LITERAL_ENUM`, compound initializers, indirect calls, `_Generic`, compound literals |
| BESM-6 code gen | — | Not started |

### Key data structures

- **`Program`** (`ast/ast.h`): Root node; linked list of `ExternalDecl` (function definition or declaration).
- **`ExternalDecl`**, **`Declaration`**, **`Stmt`**, **`Expr`**, **`Type`**: Core AST nodes. Defined in `ast/ast.h`, spec in `ast/ast.asdl`.
- **`Tac_Instruction`**, **`Tac_Value`**, **`Tac_Type`**: TAC IR nodes. Defined in `tac/tac.h`, spec in `tac/tacky.asdl`.
- **`symtab`** (`semantic/symtab.c/h`): Scoped identifier → `Symbol` map. `symtab_purge(level)` removes block-scope entries on block exit.
- **`structtab`** (`semantic/structtab.c/h`): Struct/union/enum tag → `StructDef` map. Scoped; `structtab_purge(level)` on block exit.
- **`typetab`** (`semantic/typetab.c/h`): Typedef name → `TypeDef` (`name` + cloned `Type*`) map. Scoped; `typetab_purge(level)` on block exit. `typetab_resolve(name)` returns the underlying `Type*`.

### Important design decisions

- **No identifier shadowing**: Inner blocks may not redeclare a name that already exists in any enclosing scope. `symtab` / `structtab` / `typetab` reject duplicates with `fatal_error`. This is a permanent design decision — do not add shadowing support.
- **`.asdl` files are canonical specs, not code generators.** `ast/ast.asdl` and `tac/tacky.asdl` document the IR; `ast/ast.h` and `tac/tac.h` are maintained manually and must stay in sync.
- **Word I/O (`libutil/wio`)**: AST and TAC binary streams use `size_t`-wide words for portability. Use `wio` for all IR serialization.
- **`xalloc` (`libutil/xalloc`)**: All allocations go through `xalloc`/`xfree`. In debug builds, `xalloc_report()` prints leak totals.
- **Single-pass semantics**: `typecheck_global_decl()` binds names and type-checks in a single pass.
- **Scope tracking**: `scope_level` is incremented on block entry; `scope_decrement()` decrements it and calls `symtab_purge`, `structtab_purge`, and `typetab_purge` — all backed by `map_remove_level_free`, which fires the dealloc callback on every evicted value.
- **`typedef` handling**: `STORAGE_CLASS_TYPEDEF` declarations are intercepted in `typecheck_local_var_decl` / `typecheck_file_scope_var_decl` and registered in `typetab`. `validate_type` resolves `TYPE_TYPEDEF_NAME` recursively. All type-utility helpers (`get_size`, `get_alignment`, `is_complete`, `is_signed`, `is_arithmetic`, `is_scalar`, `is_integer`, `is_character`) resolve typedef names transparently before dispatching.
- **`switch` semantic validation**: `STMT_SWITCH` requires an integer controlling expression; narrower types (char, short) are promoted to `int` via `convert_to_kind`. `STMT_CASE` requires a constant integer expression evaluated by `try_eval_const_int` (handles literals, casts, and unary `−`/`+`/`~`); duplicates are detected via a `SwitchCtx` stack (`current_switch` in `typecheck.c`). `STMT_DEFAULT` rejects multiple defaults. Case/default labels outside any switch are also rejected.
- **TAC lowering coverage**: `translate.c` calls `fatal_error()` on unimplemented constructs. Lowering is mostly complete; remaining gaps are `LITERAL_ENUM` (enum constants in expressions), `INITIALIZER_COMPOUND` (aggregate local-variable init), indirect function-pointer calls, `EXPR_GENERIC` (`_Generic`), and `EXPR_COMPOUND` (compound literals).

### AST quirks

- **Function prototypes vs. definitions**: Only function *definitions* (with a body) parse as `EXTERNAL_DECL_FUNCTION`. A bare prototype such as `int f(int);` parses as `EXTERNAL_DECL_DECLARATION` / `DECL_VAR` with a `TYPE_FUNCTION` declarator type. The typecheck pass (`typecheck_file_scope_var_decl`) detects `TYPE_FUNCTION` and registers it via `symtab_add_fun()`.
- **`f(void)` sentinel**: The parser represents a `(void)` parameter list as a single `Param` node with `TYPE_VOID` and a NULL name (not as an empty list). `typecheck_fn_decl()` strips this sentinel before param processing. Params with a NULL name are skipped when adding to symtab.

### Serialization pipeline

Binary AST flows between tools via stdout/stdin using `wio`. `ast_export.c` writes and `ast_import.c` reads. The YAML and DOT outputs are for debugging only and cannot be re-imported.

### TAC YAML output format

`tac_export_yaml()` (`tac/tac_yaml.c`) emits one `- toplevel:` block per call. Indentation is
2 spaces per level. **Not re-importable** — debug/test use only.

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
|---|---|
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
Binary ops: `add`, `subtract`, `multiply`, `divide`, `remainder`, `equal`, `not_equal`,
`less_than`, `less_or_equal`, `greater_than`, `greater_or_equal`,
`bitwise_and`, `bitwise_or`, `bitwise_xor`, `left_shift`, `right_shift`.

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
|---|---|
| `i8` / `i32` / `i64` | `value:` (signed) |
| `u8` / `u32` / `u64` | `value:` (unsigned) |
| `double` | `value:` (hex float) |
| `zero` | `bytes: N` |
| `string` | `value:` `null_terminated: true\|false` |
| `pointer` | `name:` |

## Tests

Tests are GoogleTest (C++17). Source lives alongside the module it tests:

- `ast/clone_tests.cpp` → `ast-tests`
- `scanner/tests.cpp` → `scanner-tests`
- `parser/simple_tests.cpp`, `statement_tests.cpp`, … (8 files) → `parser-tests`
- `tac/tac_yaml_tests.cpp`, `tac_graphviz_tests.cpp`, `tac_binary_tests.cpp` → `tac-yaml-tests`, `tac-dot-tests`, `tac-binary-tests`
- `semantic/symtab_tests.cpp`, `structtab_tests.cpp`, `typetab_tests.cpp`, `typecheck_tests.cpp` → 4 separate executables
- `translator/decl_tests.cpp`, `expr_tests.cpp`, `stmt_tests.cpp`, `cast_tests.cpp`, `incdec_tests.cpp`, `switch_tests.cpp`, `ptr_tests.cpp`, `struct_tests.cpp` → `translate-tests`
- `libutil/string_map_tests.cpp`, `wio_tests.cpp` → `libutil-tests`, `wio-tests`

Two test files are disabled in CMake (known failures): `parser/negative_tests.cpp` and `semantic/const_convert_tests.cpp`.

## Documentation

- [README.md](README.md) — goals, getting started, component overview
- [docs/Technical.md](docs/Technical.md) — detailed reference: repo layout, ASDL, build system, development notes
- [TODO.md](TODO.md) — work plan with effort estimates
- [grammar/README.md](grammar/README.md) — C11 grammar coverage notes
- [grammar/c11.y](grammar/c11.y), [grammar/c11.l](grammar/c11.l) — reference grammar (not used for code generation)
