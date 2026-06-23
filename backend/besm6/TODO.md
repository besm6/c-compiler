# TODO

Work plan for the BESM-6 backend, in no partucular order.

| #  | Task | Description |
|----|------|-------------|
| 1 | Assembler for Unix | Port assembler from Elbrous-B |
| 2 | Linker for Unix | Port linker from Elbrous-B |
| 3 | Switch jump-table optimization | For dense case ranges, replace the linear compare chain with an index-scaled `utc` / `uj` dispatch through a table of `,log, label` words. |
| 4 | Register allocator (word-pointer promotion) | Promote frequently-dereferenced **word** pointers into the callee-saved, currently-unused index registers r1–r5. Since `EA = M[reg]+offset+C`, a pointer in `rN` dereferences in a single index-modified `xta off(rN)`/`atx off(rN)` — replacing the 2–3-instr `wtc/xta` (or `utc/wtc/xta` for globals) sequence, folding constant member/element offsets for free, and letting consecutive derefs reuse `rN` (C resets after one instruction, so it can't be shared). Only pointers benefit (BESM-6 has no reg-reg ops, so scalars must pass through A/memory regardless). New backend pass `regalloc.c`, run in `codegen_function` after `frame_build` and before instruction selection: pick ≤5 profitable word-pointer locals by deref/use count (weight loop bodies), excluding fat `char*`/`void*` (byte-offset-encoded — keep them on the WTC/byte path) and address-taken pointers; record on the `Frame` so a promoted name resolves to *ixreg-resident* instead of an auto slot. Plumb a storage-kind through the value helpers (`emit_store_a`/`emit_xta_val`): def → `ati rN`, plain-value use → `ita rN`, deref sites (`LOAD`/`STORE`/`ADD_PTR`) emit the index-modified form. Save/restore each used `rN` in the prologue/epilogue (`c/save` preserves only r6/r7/r13; r1–r5 being callee-saved means a promoted pointer also survives calls with no spill — that fixed save cost is the profitability threshold). Adjust the post-peephole frame shrink for freed auto slots / added save slots. Follow-ups: redundant `ita`/`ati` peephole, `main` skips the save, r8–r12 with spill-around-call. |
| 7 | CSE (common subexpression elimination) | Machine-independent TAC pass. Reuse the forward-dataflow framework (`optimize/copy_prop.c`), CFG (`optimize/cfg.c`), and alias pre-analysis (`optimize/alias.c`); add expression hashing/value numbering (handle commutativity; invalidate on Load/Store/FunCall). Register in the `optimize_function` fixed-point loop (`optimize/optimize.c`) plus an `OptFlags`/CLI flag. |
| 8 | LICM (loop-invariant code motion) | Machine-independent TAC pass. Prerequisite: a new dominator-tree and natural-loop analysis (none exists; the loop info from `semantic/label_loops.c` is lost by TAC time). Then hoist loop-invariant computations into a preheader, reusing the liveness framework from `optimize/dead_store.c`. |
| 9 | Generalized strength reduction | Today only power-of-two mul/div is reduced, and only in the backend (`backend/besm6/instr.c`). Add TAC-level reduction for non-power-of-two constants (`a*3 → (a<<1)+a`, magic-number division) so all targets benefit; extend `constant_fold` pattern matching. Needs a per-target cost model. |
| 10 | Function inlining | Machine-independent, interprocedural. Build a call graph over `TAC_INSTRUCTION_FUN_CALL` (none exists; TAC is intraprocedural), add a size/recursion heuristic, substitute bodies (rename `%`-locals, map params→args, return→assign+jump), and run before the fixed-point loop so the existing passes clean up the inlined code. |

## DISABLED book-test triage

The BESM-6 backend book tests (`backend/besm6/chapter*_tests.cpp`) carry **163 `DISABLED_`
test cases** inherited from the "Writing a C Compiler" textbook (an x86-64 / IEEE-754
target). The tasks below adapt each test so it is sensible for the BESM-6 target and
re-enable it, removing only tests with no BESM-6 analogue. One task per adaptation category.
Locate the named tests with `grep -n DISABLED_ backend/besm6/chapter*_tests.cpp`. Principles:
(1) drop `static` on a global where it only blocks the test; (2) replace literals that make
no sense on BESM-6; (3) implement a missing libc function; (4) remove NaN/inf parts;
(5) shorten names that collide in 8 chars; (6) for no-analogue tests, adapt where possible
and remove the rest.

| #  | Task | Description |
|----|------|-------------|
| 23 | Wire up the heap & re-enable ch17 dynamic-allocation tests | `malloc`/`calloc`/`realloc`/`free` already link into `libc.bin` but the run harness never calls `heap_setup`, so allocations fail. Initialize a heap region at program start, then re-enable VoidPointerSimple, ArrayOfPointersToVoid, CommonPointerType, ConversionByAssignment, VoidPointerExplicitCast, SizeofExpressions, PassAllocedMemory. For MemoryManagementFunctions, implement `aligned_alloc` or drop that sub-call. |
| 24b | Redundant-paren declarators | Parse redundant parentheses in declarators: `int(f)(void)`, `long *(p)`, `unsigned(**(g(int(*(*(p))))))`, `double(d1)`. Parser-only. Re-enables ch14 Declarators (chapter14_tests.cpp). |
| 24c | Abstract declarators (nested parens & arrays) | Parse complex abstract declarators such as `double(*([3][4]))[2]` — currently rejected as "Empty type specifier list". Re-enables ch17 SizeofDerivedTypes (chapter17_tests.cpp). |
| 24d | `sizeof` of arrays & string literals (no decay) | Arrays keep their type under `sizeof`: `sizeof arr[3]`=18, `sizeof nested[2]`=30; `sizeof "Hello, World!"`=14 (byte length incl NUL, not word-padded). Re-enables ch17 SizeofArray (chapter17_tests.cpp). Its adjusted-param sub-call depends on 24a. |
| 24e | Adjacent string-literal concatenation | Implement C11 §5.1.1.2 phase 6: scanner/parser joins `"a" "b"`→`"ab"`, including in initializers (`char s[6]="yes" "no"`, `char n[2][3]={"a" "b","c" "d"}`). Re-enables ch16 AdjacentStrings & AdjacentStringsInInitializer (chapter16_tests.cpp). |
| 27 | Triage ch20 register-allocation tests individually | ~8 tests assuming x86 register pressure / struct-by-value ABI / byte-addressed pointers. Adapt those with a BESM-6 equivalent (value/name substitution), remove the register-specific ones. Revisit after task #4 (register allocator) lands. |
| 28 | Re-enable ch19 whole-pipeline all-types fold tests | Constant-folding tests spanning all scalar types. Substitute in-range values per type and drop nan/inf rows; overlaps tasks 12–14 but tracked as a cluster. (FoldCharCondition, FoldExtensionAndTruncation, FoldNegativeValues, Listing195MoreTypes, SignedUnsignedConversion, FoldCompoundAssignAllTypes, FoldCompoundBitwiseAssignAllTypes, FoldIncrDecrDoubles, FoldIncrDecrUnsigned.) |
