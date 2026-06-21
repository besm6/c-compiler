# TODO

Work plan for the BESM-6 backend, in no partucular order.

| #  | Task | Description |
|----|------|-------------|
| 1 | Assembler for Unix | Port assembler from Elbrous-B |
| 2 | Linker for Unix | Port linker from Elbrous-B |
| 3 | Switch jump-table optimization | For dense case ranges, replace the linear compare chain with an index-scaled `utc` / `uj` dispatch through a table of `,log, label` words. |
| 4 | Register allocator | Use r1-r5 for word pointers. |
| 7 | CSE (common subexpression elimination) | Machine-independent TAC pass. Reuse the forward-dataflow framework (`optimize/copy_prop.c`), CFG (`optimize/cfg.c`), and alias pre-analysis (`optimize/alias.c`); add expression hashing/value numbering (handle commutativity; invalidate on Load/Store/FunCall). Register in the `optimize_function` fixed-point loop (`optimize/optimize.c`) plus an `OptFlags`/CLI flag. |
| 8 | LICM (loop-invariant code motion) | Machine-independent TAC pass. Prerequisite: a new dominator-tree and natural-loop analysis (none exists; the loop info from `semantic/label_loops.c` is lost by TAC time). Then hoist loop-invariant computations into a preheader, reusing the liveness framework from `optimize/dead_store.c`. |
| 9 | Generalized strength reduction | Today only power-of-two mul/div is reduced, and only in the backend (`backend/besm6/instr.c`). Add TAC-level reduction for non-power-of-two constants (`a*3 → (a<<1)+a`, magic-number division) so all targets benefit; extend `constant_fold` pattern matching. Needs a per-target cost model. |
| 10 | Function inlining | Machine-independent, interprocedural. Build a call graph over `TAC_INSTRUCTION_FUN_CALL` (none exists; TAC is intraprocedural), add a size/recursion heuristic, substitute bodies (rename `%`-locals, map params→args, return→assign+jump), and run before the fixed-point loop so the existing passes clean up the inlined code. |
| 11 | Pointer difference for wide-element word pointers | `p - q` between two word pointers to a multi-word element (e.g. `char(*)[N] a, b; a - b`, or pointer-to-struct) yields a wrong value instead of an element count. In `gen_binary` (`translator/expr.c`), the `PTR_DIFF` path (~line 532) only fires when both operands are *fat* (byte) pointers; the wide-element case falls through to the `l_scale \|\| r_scale` block (~line 562), which misreads `p - q` as pointer-minus-integer (treating `q` as a scaled index). Fix: detect `BINARY_SUB` with both operands the same wide word-pointer type *before* the scale block, compute the raw word difference, and divide by the element word size to get the C element count (`(p - q) / sizeof(*p)`). Obscure — no test exercises it; unrelated to multi-dim array indexing. Add a `translate-tests` case plus a `besm-tests` run case once fixed. |
