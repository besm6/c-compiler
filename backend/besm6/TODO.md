# TODO

Work plan for the BESM-6 backend, in no partucular order.

| #  | Task | Description |
|----|------|-------------|
| 3 | Switch jump-table optimization | For dense case ranges, replace the linear compare chain with an index-scaled `utc` / `uj` dispatch through a table of `,log, label` words. |
| 4 | Register allocator (word-pointer promotion) | Promote frequently-dereferenced **word** pointers into the callee-saved, currently-unused index registers r1–r5. Since `EA = M[reg]+offset+C`, a pointer in `rN` dereferences in a single index-modified `xta off(rN)`/`atx off(rN)` — replacing the 2–3-instr `wtc/xta` (or `utc/wtc/xta` for globals) sequence, folding constant member/element offsets for free, and letting consecutive derefs reuse `rN` (C resets after one instruction, so it can't be shared). Only pointers benefit (BESM-6 has no reg-reg ops, so scalars must pass through A/memory regardless). New backend pass `regalloc.c`, run in `codegen_function` after `frame_build` and before instruction selection: pick ≤5 profitable word-pointer locals by deref/use count (weight loop bodies), excluding fat `char*`/`void*` (byte-offset-encoded — keep them on the WTC/byte path) and address-taken pointers; record on the `Frame` so a promoted name resolves to *ixreg-resident* instead of an auto slot. Plumb a storage-kind through the value helpers (`emit_store_a`/`emit_xta_val`): def → `ati rN`, plain-value use → `ita rN`, deref sites (`LOAD`/`STORE`/`ADD_PTR`) emit the index-modified form. Save/restore each used `rN` in the prologue/epilogue (`c/save` preserves only r6/r7/r13; r1–r5 being callee-saved means a promoted pointer also survives calls with no spill — that fixed save cost is the profitability threshold). Adjust the post-peephole frame shrink for freed auto slots / added save slots. Follow-ups: redundant `ita`/`ati` peephole, `main` skips the save, r8–r12 with spill-around-call. |
| 7 | CSE (common subexpression elimination) | Machine-independent TAC pass. Reuse the forward-dataflow framework (`optimize/copy_prop.c`), CFG (`optimize/cfg.c`), and alias pre-analysis (`optimize/alias.c`); add expression hashing/value numbering (handle commutativity; invalidate on Load/Store/FunCall). Register in the `optimize_function` fixed-point loop (`optimize/optimize.c`) plus an `OptFlags`/CLI flag. |
| 8 | LICM (loop-invariant code motion) | Machine-independent TAC pass. Prerequisite: a new dominator-tree and natural-loop analysis (none exists; the loop info from `semantic/label_loops.c` is lost by TAC time). Then hoist loop-invariant computations into a preheader, reusing the liveness framework from `optimize/dead_store.c`. |
| 9 | Generalized strength reduction | Today only power-of-two mul/div is reduced, and only in the backend (`backend/besm6/instr.c`). Add TAC-level reduction for non-power-of-two constants (`a*3 → (a<<1)+a`, magic-number division) so all targets benefit; extend `constant_fold` pattern matching. Needs a per-target cost model. |
| 10 | Function inlining | Machine-independent, interprocedural. Build a call graph over `TAC_INSTRUCTION_FUN_CALL` (none exists; TAC is intraprocedural), add a size/recursion heuristic, substitute bodies (rename `%`-locals, map params→args, return→assign+jump), and run before the fixed-point loop so the existing passes clean up the inlined code. |

## Multi-assembler support (Unix, then Bemsh)

The backend targets three assembler dialects from one dialect-agnostic `Besm_Module` IR:
**Madlen** (existing, runs on `dubna`), the **Unix `b6as`** assembler (AT&T syntax, `b.out`
objects — needed first), and **Bemsh** (Cyrillic autocode, runs on `dubna` — later). See
[docs/Madlen.md](../../docs/Madlen.md), [docs/Besm6_Unix_Assembler.md](../../docs/Besm6_Unix_Assembler.md),
and [docs/Bemsh.md](../../docs/Bemsh.md).

**Foundation (done).** `Besm_Dialect { BESM_MADLEN, BESM_UNIX, BESM_BEMSH }` and the
`besm_emit_module(out, module, dialect)` dispatcher are in place; `genbesm` accepts
`--madlen` / `--unix` / `--bemsh` (extensions `.mad` / `.s` / `.bem`) and threads the choice
through `codegen_program` / `codegen_static_variable`. Scalar constant operands are carried
structurally on `Besm_Instr.konst` (formatted per-dialect via `besm_const_word`, not baked into
`name`), with the peephole operand tests updated to `has_operand_symbol`. `emit_madlen.c` is
refactored onto the shared `besm_latin_mnem[]` table + `besm_operand_shape()` classifier
(`besm_mnem.c`), so the Unix emitter reuses both. The Unix emitter (`emit_unix.c`, task U1) is
now implemented, so `--unix` produces `b6as` assembly; `--bemsh` still fails with a "not yet
implemented" message. **Unix is now the default dialect** (task U4); Madlen stays reachable
via `--madlen`, which the libc `libc.bin` build and the behavioral run tests request
explicitly so they stay green.

The tasks below are ordered — Unix (`U*`) first, Bemsh (`B*`) later. Each lands independently
with green tests.

**The Unix simulator now exists.** `b6sim` (`~/.local/bin/b6sim`, built from
`v7besm/cmd/sim`) runs a linked `b.out` executable and traps the Unix v7 syscalls (extracode
`$77 N`) onto the host, so a program's `write(1,…)` lands directly on b6sim's stdout — no
listing to scrape. The Unix path is now end-to-end runnable — `puts`/`getch`-class programs
and the full `printf`/`sprintf` family (all conversion specifiers included) already produce
correct output under b6sim (U5 wired the libc syscall leaves + crt0). The Unix path is now
verified end-to-end (task U6 done): `CompileAndRunUnix` in `test/codegen_test.h` mirrors
`CompileAndRun` for the Unix dialect — `genbesm --unix` (in process) → `b6as` → `b6ld` linking
`crt0.o` first, then the program object and `libc.a` → `b6sim`, capturing the simulator's
stdout directly (no `.lst` scraping) — and `test/unix_run_tests.cpp` re-runs a subset of the
Madlen behavioral tests (`puts`/`putchar` and the `printf` conversion specifiers) under b6sim,
asserting output identical to the Madlen path. U1–U3 still validate by two proxies (both in
place) — golden-file the `.s` (`test/unix_tests.cpp`), and assemble+link cleanly via
`b6as`+`b6ld`+`libc.a` (`CompileAndAssembleUnix` in `test/codegen_test.h`, exercised by
`test/unix_link_tests.cpp`) — and observed program behavior stays covered by Madlen-on-`dubna`
(why `--madlen` stays alive) and later Bemsh-on-`dubna`. Only the Bemsh `B*` tasks remain.

| #  | Task | Description |
|----|------|-------------|
| B1 | Bemsh emitter — code + framing | New `emit_bemsh.c`: Cyrillic `besm_cyr_mnem[]` (docs/Bemsh.md §13), directives `СТАРТ`/`ФИНИШ`/`ЭКВ`/`ПАМ`/`КОНД`/`КОНК`/`ТЕКСТ`/`ВХОДН`/`ВНЕШН`, octal in `'…'`, constants via literal operands `=Е'…'`/`=Ю'…'`. Reuse `besm_operand_shape`; route Cyrillic identifiers through `utf8_to_koi7`. *Acceptance:* golden-file `CompileToBemsh` tests for the representative set. |
| B2 | Bemsh naming / symbol mangling | Bemsh sanitizer: ≤6 chars, must begin with a letter, deterministic collision-safe mangling. Runtime-helper names must match a Bemsh `libc`'s symbols (`b/ret`→`_ret`/`._ret`, `b/save`→`_save` per `tmp/bemsh.dub`). Under-specified until the Bemsh libc symbol set exists — deliver a documented provisional map. *Acceptance:* deterministic map, no collisions over a corpus, helper names match the `bemsh.dub` externs. |
| B3 | Bemsh run-integration on dubna | `CompileAndRunBemsh` variant of `CompileAndRun` wrapping output in a `*bemsh` control-card job (model on `tmp/bemsh.dub`), **reusing the existing `libc.bin`**, running `dubna`, parsing `.lst`. This is the only new-dialect path runnable end-to-end today. *Acceptance:* a subset of run tests pass under `*bemsh` with output identical to the Madlen path. |
