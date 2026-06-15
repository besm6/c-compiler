# TODO

Work plan ordered by recommended implementation sequence.
Effort: S = half day, M = 1–2 days, L = 3–5 days, XL = 1–2 weeks.

## BESM-6 Backend

The backend consumes binary TAC produced by `lower` and emits Madlen assembly (`.mad` files)
for the Dubna monitor system. The shared frontend phases (scan/parse/typecheck/lower) are
complete; this list covers the BESM-6 instruction-selection backend only.

### Madlen statement format

Every Madlen statement follows the form `<label> : <index_reg> ,<mnemonic>, <address>`.
The **two commas are mandatory**. The index-register selector (0–15) precedes the first
comma; the mnemonic is between the two commas; the address follows the second comma.

### Calling convention summary (`docs/Besm6_Calling_Conventions.md`)

| Register | Role |
|----------|------|
| r13 | Return address — set by `,VJM, fun` (`,CALL,`) on each call |
| r14 | Negative argument count — set by caller |
| r15 | Stack pointer — grows toward higher addresses |
| r6 | Parameter pointer — set by `b/save`; `r6+i` = param[i] |
| r7 | Auto-variable pointer — set by `b/save`/`b/save0`; `r7+j` = local[j] |
| r1–r7 | Callee-saved |

Caller pushes arguments in direct order with `XTA`/`XTS` (last arg stays in A), sets
`14 ,VTM, -N`, then calls `,CALL, fun`. Every callee starts with `,ITS, 13` then
`,CALL, b/save` (saves r13/r7/r6, sets r6/r7); `b/save0` is used when the callee has no
parameters. Return value goes in A; epilogue is `,UJ, b/ret`. Reading address 0 yields 0
(an architectural guarantee of BESM-6/Dubna), so no `__zero` constant is needed.

---

### Data-representation invariants (read before implementing arithmetic)

These supersede the historical "INT-format" assumptions of the old plan. See
[docs/Besm6_Data_Representation.md](../../docs/Besm6_Data_Representation.md).

- **Integers are raw two's complement with the exponent field (bits 48–42) = 0.** They are
  *not* stored in the historical INT-format (exponent 104 / `0150B`). `b/save` leaves the
  AU mode register **R = 7** (logical ω + suppress-normalization + suppress-rounding), so
  integer `A+X`/`A-X` and every bitwise op act directly on raw words with no `NTR`.
- **`BESM_INT_EXP` (`0150B`) in [abi.h](abi.h) is a transient bridge** used only *inside*
  the multiply/divide runtime helpers to borrow the hardware FP unit; it is never the
  storage format.
- **Signed `int` uses 41 bits** (sign + 40 value); **unsigned uses the full 48 bits**.
  This is why unsigned divide/modulo/compare and logical right-shift need distinct handling
  from their signed counterparts (Phase E adds the TAC op kinds that carry this distinction).
- **Integer constants follow the same rule.** `const_lit_name` ([codegen.c](codegen.c)) masks
  signed constants to 41 bits and unsigned constants to 48 bits, so a signed literal wider
  than 41 bits silently loses its top 7 bits (expected — signed `int`/`long` is a 41-bit
  type). A `U` suffix alone yields a full 48-bit unsigned literal; the `L` is not required.
  See [docs/Besm6_Data_Representation.md](../../docs/Besm6_Data_Representation.md) §5.
- **`float` ≡ `double`** (same 48-bit native FP format); the `FLOAT_TO_DOUBLE` /
  `DOUBLE_TO_FLOAT` conversions are copies. `short` ≡ `long` ≡ `int` (single word).
- **Floating-point ops need normalization**, so FP code must temporarily clear R's
  suppress bits (via `NTR`) around `A+X`/`A-X`/`A*X`/`A/X`.

### Runtime-helper convention

Runtime helpers **do not follow the C ABI**, for efficiency. Per the convention already
documented in [docs/Besm6_Runtime_Library.md](../../docs/Besm6_Runtime_Library.md): the
first operand `a` is on the stack top (r15 just past it), the second operand `b` is in A,
and the result is left in A (r15 unchanged — the caller adjusts the stack after the call).
Helpers may freely use scratch index registers but must preserve r6/r7. The arithmetic and
relational/logical helpers (`b/mul`, `b/div`, `b/mod`, `b/eq`, `b/ne`, `b/lt`, `b/le`,
`b/gt`, `b/ge`, `b/not`) are already specified there alongside `b/save`/`b/save0`/`b/ret`/
`b/true`; the new unsigned and conversion helpers reuse the same convention and `b/` prefix.

### Notes on instruction sequences and testing

The BESM-6 sequences below are sketches; final `NTR`/mode-bit placement is validated against
the Dubna simulator. Each task adds GoogleTest coverage in
[codegen_tests.cpp](codegen_tests.cpp): `CompileToMadlen` assertions for Madlen shape, and
`CompileAndRun` for runtime behavior where the simulator is available.

---

### Phase M — Optimizations

The backend currently emits straight from instruction selection: `codegen_function`
([codegen.c](codegen.c)) builds one `Besm_Block` of `Besm_Instr` via `codegen_instr`
([instr.c](instr.c)) and hands it directly to `emit_madlen_module`. There is no
optimization pass between selection and emission, so the generated Madlen carries the
verbatim store/reload, NTR-bracket, and compare-then-branch shapes that three-address TAC
lowering produces. The tasks below add that missing polish. See
[docs/Peephole_Rewrites.md](../../docs/Peephole_Rewrites.md) for the underlying theory and
the worked before/after sequences.

Two cross-cutting rules govern this phase:

- **Peephole rewrites respect basic-block boundaries.** A `BESM_STMT_LABEL` and every
  branch end a window: control may re-enter at a label, so the accumulator (A), the mode
  register (R, the NTR state), and the logical flag (ω) cannot be assumed to carry across.
  Rewrites that rely on tracked machine state stop at those boundaries.
- **Tests will need updating, behavior must not change.** The `CompileToMadlen` assertions
  in the backend test files pin exact instruction sequences and will shift as rewrites land;
  the `CompileAndRun` results (actual computed values under Dubna) must stay identical.

#### Peephole rules

| # | Task | Description | Effort |
|---|------|-------------|--------|
| 29 | NTR mode coalescing | Track R and delete any `ntr n` whose operand equals the current known R — e.g. the trailing `ntr 7` restore when `b/save` already left R = 7, or the leading `ntr 0` of an FP op when R is already 0. Collapse adjacent `ntr x` / `ntr y` ⇒ `ntr y`, and keep R = 0 across a run of consecutive FP ops, restoring to 7 once at the end. Targets the `ntr 0 … ntr 7` brackets around FP add/sub/mul/div, FP negate, and int→FP conversion. | M |
| 30 | Compare → branch fusion | A relational-helper result (`b/eq` … `b/uge`, `b/flt` … `b/fge`) that feeds a `JUMP_IF_ZERO` / `JUMP_IF_NOT_ZERO`: drop the store+reload of the boolean temp and branch on ω directly, e.g. `call b/lt` / `uza L`. Requires confirming on the simulator that the helpers leave ω consistent with A and that `atx` preserves ω. | S–M |
| 31 | Branch / label cleanup | Drop a `uj` whose target is the immediately following label; remove the duplicate `uj b/ret` (RETURN emits one and the epilogue emits another — already flagged as dead in [instr.c](instr.c)); delete instructions between an unconditional `uj` and the next label as unreachable; invert a conditional that only skips an unconditional jump (`uza L` / `uj M` / `L:` ⇒ `u1a M`). | S |
| 32 | Pointer-register reuse | Back-to-back `LOAD`/`STORE` through the same pointer each reload `ati 1`; skip the second setup when r1 still holds that pointer. Optional / lower priority — only helps adjacent dereferences of one pointer. | S |

#### Instruction-selection improvements

| # | Task | Description | Effort |
|---|------|-------------|--------|
| 33 | Constant strength reduction | In [instr.c](instr.c), where the `Tac_Val` constant operand is still visible, lower multiply by a power of two to a single `asn` (logical left shift, as `ADD_PTR` already does for word scaling) and unsigned divide / remainder by a power of two to `asn` (right shift) / `aax` (mask) instead of calling `b/mul` / `b/udiv` / `b/umod`. Signed divide by 2^k is *not* a plain shift (it must round toward zero), so leave it on `b/div`. | S–M |
| 34 | Direct symbolic global addressing | Investigate replacing the `utc name` / `xta 0` pair that `emit_xta_val` / `emit_arith_val` ([emit.c](emit.c)) emit for every global with a direct `xta name` (and the matching `atx` / `a+x` forms), where the Dubna single-pass assembler and linker permit a relocatable symbol in the address field. Keep `utc` for the index-register-based forms (array indexing, `&global`). Needs simulator validation before adoption. | M |

#### Frame allocation

| # | Task | Description | Effort |
|---|------|-------------|--------|
| 35 | Frame-slot reuse via liveness | `frame_build` ([frame.c](frame.c)) currently gives every distinct `%`-name its own auto slot (`assign_if_new`). Add a linear-scan liveness pass over `%`-temporaries so non-overlapping temporaries share one auto slot, shrinking `num_autos` and the prologue `utm 15` stack extension. The no-shadowing rule keeps name→slot mapping unambiguous. | M–L |

#### Control flow

| # | Task | Description | Effort |
|---|------|-------------|--------|
| 36 | Switch jump-table optimization | For dense case ranges, replace the linear compare chain with an index-scaled `utc` / `uj` dispatch through a table of `,oct, label` words. | M |

### Phase N — Deferred / future

| # | Task | Description | Effort |
|---|------|-------------|--------|
| 101 | Two-word `long long` / `unsigned long long` | Two-word load/store and software add/sub/mul/div/compare. First fix `codegen_sizeof` in [abi.h](abi.h), which currently returns 1 word for these two-word types. | XL |
| 102 | Two-word `long double` | Two-word native-FP arithmetic (80-bit mantissa, 14-bit exponent biased 8192) via runtime helpers, using the Y/RMR register for double-width intermediates. | XL |
