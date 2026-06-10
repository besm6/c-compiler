# TAC Optimizer — Work Plan

Reference: [docs/TAC_Optimization.md](../docs/TAC_Optimization.md)

## Completed

- Task 1 — Directory and build system
- Task 2 — Public API header
- Task 3 — Pipeline loop skeleton
- Task 4 — CLI flags
- Task 5 — Test harness stub
- Phase 1 verification
- Task 6 — Unary folding
- Task 7 — Binary folding (integer)
- Task 8 — Binary folding (floating-point)
- Task 9 — Type conversion folding
- Task 10 — Conditional jump folding
- Task 11 — Tests for constant folding
- Phase 2 verification
- Task 12 — CFG data structures
- Task 13 — BFS reachability
- Task 14 — Post-removal cleanup
- Task 15 — Tests for unreachable elimination
- Phase 3 verification
- Task 16 — Alias pre-analysis
- Task 17 — Reaching-copies dataflow

---

## Phase 1 — Infrastructure, CLI Options, and Pipeline Loop

### Task 1 — Directory and build system

Create `optimize/CMakeLists.txt`:

```cmake
add_library(optimize STATIC
    optimize.c
    const_fold.c
    cfg.c
    unreachable.c
    copy_prop.c
    dead_store.c
)
target_include_directories(optimize PUBLIC .)
target_link_libraries(optimize tac libutil)

add_executable(optimizer-tests EXCLUDE_FROM_ALL
    optimizer_tests.cpp
)
target_link_libraries(optimizer-tests optimize tac GTest::gtest_main)
gtest_discover_tests(optimizer-tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 10)
```

In the top-level `CMakeLists.txt`:
- Add `add_subdirectory(optimize)` after `add_subdirectory(translator)`.
- Change `target_link_libraries(lower translator)` to
  `target_link_libraries(lower optimize translator)`.
- Add `optimizer-tests` to the `build_tests` custom target.

### Task 2 — Public API header

Create `optimize/optimize.h`:

```c
#pragma once
#include <stdbool.h>
#include "tac.h"

typedef struct {
    bool unreachable_elim;   // --no-unreachable disables
    bool copy_propagation;   // --no-copy-prop disables
    bool dead_store_elim;    // --no-dead-store disables
} OptFlags;

OptFlags opt_flags_default(void);

// Returns the optimized body in place (modifies the list).
// Caller owns the result; caller freed the original list.
Tac_Instruction *optimize_function(Tac_Instruction *body, OptFlags flags);
```

Constant folding is always enabled and has no flag.

### Task 3 — Pipeline loop skeleton (`optimize/optimize.c`)

Implement:

```c
OptFlags opt_flags_default(void) {
    return (OptFlags){ .unreachable_elim  = true,
                       .copy_propagation  = true,
                       .dead_store_elim   = true };
}

Tac_Instruction *optimize_function(Tac_Instruction *body, OptFlags flags) {
    if (!body) return NULL;
    for (;;) {
        body = constant_fold(body);           // Phase 2

        OptCfg *cfg = cfg_build(body);

        if (flags.unreachable_elim)
            eliminate_unreachable(cfg);       // Phase 3

        if (flags.copy_propagation)
            propagate_copies(cfg);            // Phase 4

        if (flags.dead_store_elim)
            eliminate_dead_stores(cfg);       // Phase 5

        Tac_Instruction *new_body = cfg_flatten(cfg);
        cfg_free(cfg);

        if (!new_body || tac_compare_instruction(new_body, body))
            return new_body;

        tac_free_instruction(body);
        body = new_body;
    }
}
```

Stubs for the five called functions return their argument unchanged until the
corresponding phase is implemented.

### Task 4 — CLI flags in `translator/main.c`

Add to `Args`:
```c
int no_unreachable;   // --no-unreachable
int no_copy_prop;     // --no-copy-prop
int no_dead_store;    // --no-dead-store
```

Add long options and `print_usage()` lines. Construct `OptFlags` from args and call
`optimize_function()` inside the processing loop after `translate()` and before
`emit_tac_toplevel()`:

```c
if (tac->kind == TAC_TOPLEVEL_FUNCTION) {
    OptFlags flags = opt_flags_default();
    flags.unreachable_elim = !args.no_unreachable;
    flags.copy_propagation = !args.no_copy_prop;
    flags.dead_store_elim  = !args.no_dead_store;
    tac->u.function.body =
        optimize_function(tac->u.function.body, flags);
}
```

### Task 5 — Test harness stub (`optimize/optimizer_tests.cpp`)

Minimal GoogleTest file; one test verifying `optimize_function(nullptr, opt_flags_default())`
returns `nullptr` without crashing.

### Phase 1 verification

- `make` builds without warnings.
- `./build/lower --help` lists `--no-unreachable`, `--no-copy-prop`, `--no-dead-store`.
- `./build/lower input.c --yaml` produces output identical to pre-optimizer output
  (all passes are no-ops at this stage).

---

## Phase 2 — Constant Folding

All work in `optimize/const_fold.c`. The pass is a single linear scan over the flat
`Tac_Instruction` linked list; it does **not** need a CFG.

Function signature:
```c
Tac_Instruction *constant_fold(Tac_Instruction *body);
```

### Task 6 — Unary folding

For `TAC_INSTRUCTION_UNARY` where `src` is `TAC_VAL_CONSTANT`:

| Operator | Action |
|---|---|
| `TAC_UNARY_COMPLEMENT` | Bitwise complement of integer constant (type-width-aware) |
| `TAC_UNARY_NEGATE` | Arithmetic negation; for unsigned, wraps modulo 2^width |
| `TAC_UNARY_NOT` | Logical not: 0 → 1, nonzero → 0; result is `ConstInt` |

Replace the instruction with `TAC_INSTRUCTION_COPY(folded_const, dst)`.
Free the old instruction with `tac_free_instruction`.

### Task 7 — Binary folding (integer)

For `TAC_INSTRUCTION_BINARY` where both `src1` and `src2` are `TAC_VAL_CONSTANT`
and both constants are integer kinds (`ConstInt`, `ConstLong`, `ConstLongLong`,
`ConstUInt`, `ConstULong`, `ConstULongLong`, `ConstChar`, `ConstUChar`):

Dispatch on `Tac_BinaryOperator`. The result constant has the same kind as `src1`
unless the operator produces a boolean (comparisons → `ConstInt` 0 or 1).

Integer widths and signedness for all 23 operators:
- Arithmetic: ADD, SUBTRACT, MULTIPLY use signed or unsigned C arithmetic per kind.
- DIVIDE, REMAINDER: skip if divisor is zero (leave instruction unchanged).
- DIVIDE_UNSIGNED, REMAINDER_UNSIGNED: unsigned division; skip if divisor is zero.
- Comparisons (EQUAL through GREATER_OR_EQUAL, and UNSIGNED variants): result is
  `ConstInt` 0 or 1.
- Bitwise: AND, OR, XOR operate on integer bits.
- LEFT_SHIFT, RIGHT_SHIFT (arithmetic for signed), RIGHT_SHIFT_LOGICAL (unsigned):
  shift amount masked to type width.

Replace with `TAC_INSTRUCTION_COPY` carrying the result constant.

### Task 8 — Binary folding (floating-point)

Same pattern for `ConstFloat`, `ConstDouble`, `ConstLongDouble` operands.
Use host `double` or `long double` arithmetic. Float constant uses `double` storage
(matching the existing `tac.h` convention where `float_val` is typed `double`).
Only applicable operators: ADD, SUBTRACT, MULTIPLY, DIVIDE (never skip — IEEE
division by zero is defined), EQUAL, NOT_EQUAL, LESS_THAN, LESS_OR_EQUAL,
GREATER_THAN, GREATER_OR_EQUAL.

### Task 9 — Type conversion folding

For all 14 conversion instruction kinds, when `src` is `TAC_VAL_CONSTANT`:

| Instruction | Produces |
|---|---|
| `SIGN_EXTEND` | `ConstLong` or `ConstLongLong` from narrower signed integer |
| `ZERO_EXTEND` | `ConstULong` or `ConstULongLong` from narrower unsigned integer |
| `TRUNCATE` | narrower integer constant, truncated by value |
| `INT_TO_DOUBLE` | `ConstDouble` |
| `UINT_TO_DOUBLE` | `ConstDouble` |
| `DOUBLE_TO_INT` | `ConstInt` (C truncation toward zero) |
| `DOUBLE_TO_UINT` | `ConstUInt` |
| `INT_TO_FLOAT` | `ConstFloat` |
| `UINT_TO_FLOAT` | `ConstFloat` |
| `FLOAT_TO_INT` | `ConstInt` |
| `FLOAT_TO_UINT` | `ConstUInt` |
| `FLOAT_TO_DOUBLE` | `ConstDouble` |
| `DOUBLE_TO_FLOAT` | `ConstFloat` |
| `INT_TO_LONG_DOUBLE` | `ConstLongDouble` |
| `UINT_TO_LONG_DOUBLE` | `ConstLongDouble` |
| `LONG_DOUBLE_TO_INT` | `ConstInt` |
| `LONG_DOUBLE_TO_UINT` | `ConstUInt` |
| `LONG_DOUBLE_TO_DOUBLE` | `ConstDouble` |
| `DOUBLE_TO_LONG_DOUBLE` | `ConstLongDouble` |
| `LONG_DOUBLE_TO_FLOAT` | `ConstFloat` |
| `FLOAT_TO_LONG_DOUBLE` | `ConstLongDouble` |

Replace with `TAC_INSTRUCTION_COPY(new_const, dst)`.

### Task 10 — Conditional jump folding

For `TAC_INSTRUCTION_JUMP_IF_ZERO(cond, target)` where cond is a constant:
- cond == 0 → replace with `TAC_INSTRUCTION_JUMP(target)`.
- cond ≠ 0 → delete instruction (unlink from list, free it).

For `TAC_INSTRUCTION_JUMP_IF_NOT_ZERO(cond, target)` where cond is a constant:
- cond ≠ 0 → replace with `TAC_INSTRUCTION_JUMP(target)`.
- cond == 0 → delete instruction.

"Is zero" for floating-point: treat as zero iff value == 0.0 exactly.

### Task 11 — Tests for constant folding

In `optimizer_tests.cpp`:
- `6 / 2` → `Copy(ConstInt(3), dst)`.
- `~0` → `Copy(ConstInt(-1), dst)`.
- `!0` → `Copy(ConstInt(1), dst)`.
- `SignExtend(ConstInt(3))` → `Copy(ConstLong(3), dst)`.
- `Truncate(ConstLong(256+7))` as int → `Copy(ConstInt(7), dst)` (low byte).
- `JumpIfZero(ConstInt(0), T)` → `Jump(T)`.
- `JumpIfNotZero(ConstInt(0), T)` → instruction deleted.
- Function with only constants: verify `optimize_function()` reaches fixed point.

### Phase 2 verification

- All existing tests still pass (`make test`).
- `./build/lower --yaml` of a small file with constant expressions shows folded output.

---

## Phase 3 — Unreachable Code Elimination

### Task 12 — CFG data structures (`optimize/cfg.h`, `optimize/cfg.c`)

Define in `optimize/cfg.h`:

```c
typedef struct OptBlock {
    int id;
    Tac_Instruction *first;   // first instruction in block
    Tac_Instruction *last;    // last instruction in block (NULL tail)
    struct OptBlock **succs;  // successor blocks (0, 1, or 2)
    int nsucc;
    bool reachable;
} OptBlock;

typedef struct OptCfg {
    OptBlock **blocks;        // array, index == id
    int nblocks;
} OptCfg;

OptCfg *cfg_build(Tac_Instruction *body);
Tac_Instruction *cfg_flatten(OptCfg *cfg);
void cfg_free(OptCfg *cfg);
```

`cfg_build` algorithm:
1. Linear scan over the instruction list.
2. Start a new block at the first instruction, at every `Label`, and after every
   terminal (Jump, JumpIfZero, JumpIfNotZero, Return).
3. Build a `StringMap` mapping label name → block id.
4. Second pass: for each block, set `succs` by inspecting the block's terminal:
   - `Jump(T)` → one successor (block of label T).
   - `JumpIfZero(c,T)` / `JumpIfNotZero(c,T)` → two successors: block of T and
     the block that immediately follows in id order (fall-through).
   - `Return` → no successors.
   - Non-terminal last instruction → one successor: the next block (fall-through).

Ownership: `cfg_build` does **not** copy instructions; blocks point into the
existing linked list. `cfg_flatten` rebuilds the `next` pointers by concatenating
block instruction sequences. `cfg_free` frees the `OptCfg` and `OptBlock` structs
and the `succs` arrays, but not the instructions themselves.

### Task 13 — BFS reachability (`optimize/unreachable.c`)

```c
void eliminate_unreachable(OptCfg *cfg);
```

1. BFS from `cfg->blocks[0]` (the entry block), setting `reachable = true`.
2. For each unreachable block: free its instructions with `tac_free_instruction`
   on each node, and mark the block as having `first = last = NULL`.

### Task 14 — Post-removal cleanup

After reachability pass, before `cfg_flatten`:

**Useless jump removal.** For each reachable block whose terminal is `Jump(T)`,
check whether the next reachable block (next id) starts with `Label(T)`. If so,
unlink and free the Jump from the block's instruction list and update `last`.

**Unused label removal.** Collect the set of all jump targets (strings) from
remaining Jump/JumpIfZero/JumpIfNotZero instructions. Remove any `Label(name)`
instruction whose name is not in that set, unless it is the very first instruction
of the function's body (the function entry label). Use `StringMap` for the target set.

### Task 15 — Tests for unreachable elimination

- Function ending with explicit `return` has no backstop `Return(NULL)` after optimization.
- Dead else-branch after `JumpIfZero(ConstInt(0), Else)` → else block removed.
- `Jump(L); /* dead block */; L: Return(x)` → dead block gone and useless jump removed.

### Phase 3 verification

- `make test` passes.
- YAML output of a function with a backstop return shows only one Return.

---

## Phase 4 — Copy Propagation

### Task 16 — Alias pre-analysis (`optimize/copy_prop.c`)

Before the dataflow, collect two sets stored as `StringMap` (name → 1):

- **Static names**: walk the full `Tac_TopLevel` linked list for
  `TAC_TOPLEVEL_STATIC_VARIABLE` entries; insert each `name`.
  Pass the program's top-level list as a parameter to `propagate_copies`.
- **Address-taken names**: walk all instructions across all blocks; for each
  `TAC_INSTRUCTION_GET_ADDRESS`, insert `src->u.var_name` into the address-taken set.
  (Only `Var` operands can be address-taken; skip `Constant`.)

These sets are used during gen/kill.

### Task 17 — Reaching-copies dataflow

Copy-set representation per block: `StringMap` mapping `dst_name → Tac_Val *`.
A `NULL` value means the name is killed (conflicting copies). An absent entry
means no reaching copy.

**Meet** (intersection): `in[b]` = for each name in the copy-set of the first
predecessor, keep it with the same `src` only if every other predecessor has the
same `(src, dst)` pair. Use `StringMap` with a two-pass approach: first build from
predecessor 0, then intersect against each remaining predecessor.

**Transfer function** (forward, per instruction):
- `Copy(src, dst)`:
  - Kill: remove every entry where key == `dst->u.var_name` or where the stored
    `src` val is a `Var` with name == `dst->u.var_name`.
  - Gen: insert `dst->u.var_name → src`.
- Any other defining instruction (the instruction writes to a `dst` variable):
  - Kill: same kill as above for `dst`.
- `FunCall`:
  - Kill all entries where key ∈ static_names or stored src ∈ static_names.
  - Kill all entries where key ∈ address_taken or stored src ∈ address_taken.
  - Also kill the entry for the FunCall's `dst` (it is redefined).
- `Store`:
  - Kill all entries where key ∈ address_taken or stored src ∈ address_taken.

Iterate over blocks in reverse-post-order (or simply iterate repeatedly until
no `out` set changes; convergence is guaranteed since the lattice is finite).

### Task 18 — Substitution

After dataflow converges, make a second pass:
- At each instruction, using the `in` copy-set for that instruction's program point
  (maintained by applying the transfer function instruction by instruction within a block),
  replace each `Var(x)` operand with `src` if `in` maps `x → src`.
- Duplicate the `src` val node (do not share nodes; allocate new `Tac_Val` and
  `Tac_Const` where needed with `tac_new_val` / `tac_new_const`).
- After substitution, detect and remove `Copy(x, x)` self-copies: if a `Copy`
  instruction has `src` and `dst` both `Var` with the same name, unlink and free it.

### Task 19 — Tests for copy propagation

- `t.0 = 3; Return(t.0)` → `Return(ConstInt(3))` (constant propagated).
- Cross-branch: `t.0 = 4; JIZ(flag, Else); t.0 = 3; Else:; Return(t.0)` →
  `t.0` NOT replaced at Return (copies do not agree on all paths).
- After FunCall, static-duration variable copy is killed.
- Self-copy `Copy(t.0, t.0)` removed.

### Phase 4 verification

- `make test` passes.
- Simple constant-return function reduces to a single `Return(const)` in YAML.

---

## Phase 5 — Dead Store Elimination

### Task 20 — Alias pre-analysis (`optimize/dead_store.c`)

Reuse the same alias analysis logic as Phase 4 (or factor it into a shared internal
helper `collect_alias_sets` in `optimize/alias.c` / `optimize/alias.h`).

Build `static_names` and `address_taken` `StringMap`s as in Task 16.

### Task 21 — Liveness dataflow

Live-set representation: `StringMap` (name → 1, present means live).

**Initial value at Exit**: union of `static_names` and `address_taken` (they may
be observed after the function returns).

**Meet** (union): `out[b]` = union of `in[s]` for all successors `s`.

**Transfer function** (backward, per instruction):
1. Let `D` = the instruction's defined variable (if any):
   - Copy, Unary, Binary, all conversions, GetAddress, Load, AddPtr, CopyFromOffset →
     `dst` is the defined variable.
   - Store, FunCall → no defined variable (side-effecting; not killed).
   - CopyToOffset → not removable; still kill `dst` (field write may be seen through pointer).
   - Jump, JumpIfZero, JumpIfNotZero, Label, Return → no defined variable.
2. Remove `D` from the live set (it is freshly defined here).
3. Add all source variables referenced by the instruction to the live set
   (only `Var` nodes contribute a name; `Constant` nodes contribute nothing).
4. At `FunCall`: add all `static_names` and `address_taken` to live set before step 2
   (the callee may read them).

Iterate backward over blocks (process blocks in reverse topological or reverse
post-order) until no `in` set changes.

### Task 22 — Dead store removal

For each instruction `i` in a block, compute `live_before` from `live_after` by
applying the transfer function backward. If `D` (the defined variable) is not in
`live_after(i)` AND the instruction is removable → unlink and free `i`.

**Removable** instruction kinds:
`COPY`, `UNARY`, `BINARY`, all 14 conversion kinds, `GET_ADDRESS`, `LOAD`,
`ADD_PTR`, `COPY_FROM_OFFSET`.

**Not removable** (keep regardless of liveness):
`STORE`, `FUN_CALL`, `COPY_TO_OFFSET`,
`JUMP`, `JUMP_IF_ZERO`, `JUMP_IF_NOT_ZERO`, `LABEL`, `RETURN`.

### Task 23 — Tests for dead store elimination

- `t.0 = a + b; t.0 = 2; Return(t.0)` → first `Binary` removed (dst dead).
- `t.0 = 3; Return(t.0)` → after copy prop replaces with `Return(3)`, the
  `Copy(3, t.0)` is dead and removed.
- `Store(v, ptr)` — not removed even when result dst is dead.
- `FunCall(f, [], dst)` — not removed even when `dst` is dead.
- Static variable assignment survives (appears in live-at-exit set).

### Phase 5 verification

- `make test` passes including new `optimizer-tests`.
- End-to-end: a C function `int f(void) { return 6/2; }` lowers to `Return(3)`
  with all passes enabled (constant folding folds division; copy prop removes the
  temp; dead store elimination removes the store to the temp).

---

## Combined pipeline verification

Run with `./build/lower --yaml` on representative snippets:

1. `int f(void) { return 6/2; }` → single `Return(ConstInt(3))`.
2. `int g(int x) { int t = x; return t; }` → `Return(Var("x"))` (copy propagated,
   temp store eliminated).
3. `int h(int x) { if (0) return 1; return 2; }` → single `Return(ConstInt(2))`
   (dead branch removed).
4. Existing regression suite: `make test` passes with all passes on and with each
   individual `--no-*` flag.
