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

### Phase I — Floating point (single word; `float` ≡ `double`)

| # | Task | Description | Effort |
|---|------|-------------|--------|
| 15 | FP arithmetic | Add/Sub/Mul/Div map to `A+X`/`A-X`/`A*X`/`A/X` **with normalization enabled** — temporarily clear R's suppress bits via `NTR` around the FP op (integer mode leaves R=7), then restore. FP negate: `X-A 0`. | M |
| 16 | FP comparisons | `A-X` sets additive ω; `U1A`/`UZA` on the sign for `<`/`>`/`<=`/`>=`; `AEX`+`UZA` for `==`/`!=`. Produce raw 0/1. | S |
| 18 | Int ↔ double conversions | `INT_TO_DOUBLE`/`UINT_TO_DOUBLE`: normalize the raw integer into FP (set the INT-format exponent, then `NTR`+`A+X 0`). `DOUBLE_TO_INT`/`DOUBLE_TO_UINT`: runtime `b/dtoi`/`b/dtou` (shift the mantissa by 104−exp). `FLOAT_*` ≡ `DOUBLE_*`, and `*_TO_FLOAT`/`FLOAT_TO_DOUBLE` are copies. | L |

### Phase K — Pointers, arrays, structs, fat pointers

| # | Task | Description | Effort |
|---|------|-------------|--------|
| 21 | Fat-pointer `char` access | `char*`/`void*` are fat pointers (bit 48 set, byte offset in bits 47–45). **Load byte**: `WTC ptr` / `XTA 0` / `ASX ptr` (shift by offset×8) / `AAX =0377`. **Store byte**: read-modify-write the containing word (mask out the target byte, OR in the new byte shifted into place). | L |
| 22 | `char*` arithmetic & pointer casts | `char*` increment decrements the 3-bit byte offset, borrowing into the word address when it wraps 0→5. Casts: `int*`→`char*` sets the fat marker + offset 5; `char*`→`int*` clears them; `char*`↔`void*` is a bit-pattern copy. | M |

### Phase M — Deferred / future

| # | Task | Description | Effort |
|---|------|-------------|--------|
| 24 | Two-word `long long` / `unsigned long long` | Two-word load/store and software add/sub/mul/div/compare. First fix `codegen_sizeof` in [abi.h](abi.h), which currently returns 1 word for these two-word types. | XL |
| 25 | Two-word `long double` | Two-word native-FP arithmetic (80-bit mantissa, 14-bit exponent biased 8192) via runtime helpers, using the Y/RMR register for double-width intermediates. | XL |
| 26 | Optimizations | Peephole rewrites, redundant load/store and NTR elimination, frame-slot reuse for dead temporaries. (Switch jump tables are tracked separately as task #27.) | L |
| 27 | Switch jump-table optimization | For dense case ranges, replace the linear compare chain with an index-scaled `UTC`/`UJ` dispatch through a table of `,oct, label` words. | M |
