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

## Compiler intrinsics

[docs/Besm6_Intrinsics.md](../../docs/Besm6_Intrinsics.md) specifies nine intrinsics that give C
direct access to the machine operations it cannot otherwise express: the two supervisor instructions
that reach *every* peripheral (`033 ext`, `002 mod` — the BESM-6 has no I/O address space, no
memory-mapped device registers and no channel programs), the bit-manipulation instructions with no C
equivalent (`apx`/`aux` — PEXT/PDEP twenty years early, `acx` popcount, `anx` highest-set-bit, `arx`
end-around-carry add), the halt instruction, and the extracode trap. They are what lets the BESM-6
Unix kernel in the sibling `v7besm` tree drive the hardware from C instead of assembly.

**Lowering strategy.** An intrinsic *is* a call in the IR (LLVM's design). The intrinsics are declared
as ordinary prototypes in [libc/besm6/include/besm6.h](../../libc/besm6/include/besm6.h) (task I1,
done), so `typecheck_expr`'s `case EXPR_CALL` already checks arity and coerces every argument against
the prototype. They arrive at the backend as a plain `TAC_INSTRUCTION_FUN_CALL` whose `fun_name`
begins with `__besm6_`, and `codegen_intrinsic` (`intrinsics.c`, tasks I2–I5, all done) intercepts
them at the top of that case in `instr.c` and emits inline machine instructions instead of a `,call,`.
**`tac/`, `optimize/`, `ast/` and `translator/` are untouched** — and `optimize/dead_store.c` already
treats a FUN_CALL as never-dead, which is exactly the "side-effecting and never eliminable" contract
the intrinsics need. `semantic/` has exactly one change, for the one argument that is not a value but
an opcode: `expressions.c` constant-folds and range-checks `__besm6_extracode`'s `op` (task I5).

**All nine names collide under Madlen's 8-character truncation** — they share the `__besm6_` prefix —
so `codegen_intrinsic` must intercept *every* one of them. Any intrinsic left unintercepted does not
fail to link; it silently aliases to whichever other one the assembler saw first. Hence its bottom
line is a `fatal_error`, not a `return false`.

**Five traps** that silently miscompile if missed: `peephole.c`'s `instr_reads_auto_slot` is a
whitelist with `default: return false` (a new memory-operand kind that is not added to it lets
rule #28 delete the store that feeds it); `arx` leaves *multiplicative* ω, not the logical ω that
compiled code contracts for (handled in I2 — the five Tier-2 ω classes are now verified against
[docs/Besm6_Instruction_Set.md](../../docs/Besm6_Instruction_Set.md), and only `arx` needed the no-op
`,aox,` correction); the halt is **resumable**, so `stop` must not be treated as a terminator
(handled in I3 — it is out of rule #31(b)'s unreachable-run trigger, and `__besm6_stop` is not
`_Noreturn`); `BESM_MOD_*` is already taken by the C-register address-modification group
(`BESM_MOD_UTC`/`BESM_MOD_WTC`), so the privileged `002 mod` instruction must not reuse that prefix
(handled in I4 — the kinds are `BESM_IO_EXT`/`BESM_IO_MOD`, joined in I5 by `BESM_IO_EXTRACODE`); and
`ext`/`mod` **rewrite the AU mode register R** — on a read address (`04000` for `ext`, `0200` for
`mod`) the hardware switches it to logical, which is exactly the register peephole rules #29(a)/(b)
track (handled in I4 — both kinds are `is_block_boundary`, so the tracked R and A are dropped across
them; the extracode is one too, since the monitor's handler runs arbitrary code before returning).

**Madlen names no halt.** `,stop,` is "ошибка в коп"; the halt goes out as Madlen's raw octal machine
code, and the digit count picks the format: `,33,` is the Format-2 opcode 033 (the halt), while
`,033,` is the Format-1 033 — that is `ext`, which faults. `emit_madlen.c`'s `mad_mnem` holds that
one override; Bemsh (`стоп`) and `b6as` (`stop`) spell it normally. `ext` is the *other* 033, and
Madlen spells that one normally: `,ext, 2073` assembles to opcode 033 (verified under `dubna`). The
extracode is the third spelling problem: its mnemonic *is* its opcode, and each dialect writes that
differently — Madlen `,*74,`, Bemsh `э74`, `b6as` `$77` (a raw octal opcode; `b6as` names no mnemonic
for 050–077) — so `BESM_IO_EXTRACODE` is `BESM_SHAPE_SPECIAL`, with a case in each of the three
emitters.

**Every numeric address field is decimal** in all three dialects, so the octal addresses of the
peripherals map come out converted: `__besm6_ext(04031, …)` emits `,ext, 2073`, and `b6_grp_read()`
(`__besm6_mod(0237, 0)`) emits `,mod, 159`. Same rule as I3's `__besm6_stop(0377)` → `,33, 255`.

| #  | Task | Description |
|----|------|-------------|
| I6 | Documentation | `backend/besm6/besm6.asdl` — the new instruction kinds (it is the canonical spec; `besm.h` is hand-maintained alongside it). [docs/Besm6_Intrinsics.md](../../docs/Besm6_Intrinsics.md) — an implementation section: how each tier lowers, the r12 scratch register, the ω correction after `arx`, the r14 clobber, and the fact that Tier 1 has no run test. [docs/Peephole_Rewrites.md](../../docs/Peephole_Rewrites.md) — the standing obligation that any new memory-operand kind be added to `instr_reads_auto_slot` and any A/R/ω-clobbering kind to `is_block_boundary`. [docs/Besm6_Instruction_Set.md](../../docs/Besm6_Instruction_Set.md) — **it has the two privileged opcodes wrong**: it labels `032` as `EXT (зпп)` and leaves `033` nameless (`счп`), whereas `033` *is* `ext`/`увв` and `002` is `mod`/`рег` (checked against `v7besm/cmd/as/tables.c`, `dubna`'s own Madlen table, and a historical Bemsh source that writes `увв '4007'`); also cover the extracode range. [docs/Bemsh.md](../../docs/Bemsh.md) — its instruction table lists `УВВ` for *both* `032` and `033`. `CLAUDE.md` — the BESM-6 row of the compiler-phases table. *Acceptance:* `make run` green; the ASDL and `besm.h` agree. |
