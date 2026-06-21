# TODO

Work plan for the BESM-6 backend, in no partucular order.

| #  | Task | Description |
|----|------|-------------|
| 1 | Assembler for Unix | Port assembler from Elbrous-B |
| 2 | Linker for Unix | Port linker from Elbrous-B |
| 3 | Switch jump-table optimization | For dense case ranges, replace the linear compare chain with an index-scaled `utc` / `uj` dispatch through a table of `,log, label` words. |
| 4 | Register allocator | Use r1-r5 for word pointers. |
| 5 | Multi-dimensional char arrays | `char m[2][4]={"ab","cd"}` is mis-lowered. Static path (`static.c` `is_char_array`/`char_array_log_items`) treats any char-innermost array as 1-D and flattens all bytes into one stream, losing per-row padding. Automatic path (`translator/stmt.c` `gen_compound_init`) sees an inner string row as `INITIALIZER_SINGLE` of `char[N]` (size>1), so `byte_leaf` is false and it emits a word `COPY_TO_OFFSET` instead of per-byte stores. Fix: detect multi-dim char arrays and emit each row independently with padding/zero-fill in both paths. |
| 6 | Audit static-init guards | `static.c:238` (`"TODO: non-float static init (Phase C)"`) and `static.c:314` (`"unsupported initializer layout"`) are defensive `default`/`else` branches with no reachable trigger today — all `TAC_STATIC_INIT_*` kinds are handled and `static_data_items` always yields a labelable/Z00 item. Confirm unreachability, then downgrade to an assert/internal-error or drop the stale "Phase C" wording. (The real reachable static-init gap is row 5.) |
| 7 | CSE (common subexpression elimination) | Machine-independent TAC pass. Reuse the forward-dataflow framework (`optimize/copy_prop.c`), CFG (`optimize/cfg.c`), and alias pre-analysis (`optimize/alias.c`); add expression hashing/value numbering (handle commutativity; invalidate on Load/Store/FunCall). Register in the `optimize_function` fixed-point loop (`optimize/optimize.c`) plus an `OptFlags`/CLI flag. |
| 8 | LICM (loop-invariant code motion) | Machine-independent TAC pass. Prerequisite: a new dominator-tree and natural-loop analysis (none exists; the loop info from `semantic/label_loops.c` is lost by TAC time). Then hoist loop-invariant computations into a preheader, reusing the liveness framework from `optimize/dead_store.c`. |
| 9 | Generalized strength reduction | Today only power-of-two mul/div is reduced, and only in the backend (`backend/besm6/instr.c`). Add TAC-level reduction for non-power-of-two constants (`a*3 → (a<<1)+a`, magic-number division) so all targets benefit; extend `constant_fold` pattern matching. Needs a per-target cost model. |
| 10 | Function inlining | Machine-independent, interprocedural. Build a call graph over `TAC_INSTRUCTION_FUN_CALL` (none exists; TAC is intraprocedural), add a size/recursion heuristic, substitute bodies (rename `%`-locals, map params→args, return→assign+jump), and run before the fixed-point loop so the existing passes clean up the inlined code. |
