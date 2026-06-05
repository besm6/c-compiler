# TODO

Work plan ordered by recommended implementation sequence.
Effort: S = half day, M = 1–2 days, L = 3–5 days, XL = 1–2 weeks.

## BESM-6 Backend

The backend consumes binary TAC produced by `lower` and emits Madlen assembly (`.mad` files)
for the Dubna monitor system.

### Madlen statement format

Every Madlen statement follows the form `<label> : <index_reg> ,<mnemonic>, <address>`.
The **two commas are mandatory**.  The index register selector (0–15) precedes the first comma;
the mnemonic is between the two commas; the address follows the second comma.

### Calling convention summary (`docs/Besm6_Calling_Conventions.md`)

| Register | Role |
|----------|------|
| r13 | Return address — set by `,CALL, fun` on each call |
| r14 | Negative argument count — set by caller |
| r15 | Stack pointer — grows toward higher addresses |
| r6 | Parameter pointer — set by `b/save`; `r6+i` = param[i] |
| r7 | Auto-variable pointer — set by `b/save`; `r7+j` = local[j] |
| r1–r7 | Callee-saved |

Caller pushes arguments in direct order with `XTA`/`XTS` (last arg stays in A), sets
`14 ,VTM, -N`, then calls `,CALL, fun`.  Every callee starts with `,ITS, 13` (push last
arg, load r13) then `,CALL, b/save` (saves r13/r7/r6, sets r6 and r7).  Return value goes
in A; epilogue is `,UJ, b/ret`.  Reading address 0 yields 0 — an architectural guarantee of
the BESM-6/Dubna system; no `__zero` constant is needed.

---

### Phase B — Instruction Selection

All TAC variables live in the frame (params via r6, autos via r7).  A is the sole computation
register.  The pattern is: load from frame → operate → store to frame.

| # | Task | Description | Effort |
|---|------|-------------|--------|
| 19 | Integer Multiply | `A*X` on two INT-format operands gives FP exponent 104+104−64 = 144. Correct with `,E-N, 150B` (E −= 40; 144−40 = 104). Sequence: `6\|7 ,XTA, src1` / `6\|7 ,A*X, src2` / `,E-N, 150B` / `6\|7 ,ATX, dst`. | M |
| 20 | Integer Divide and Remainder | FP divide on INT operands yields exponent 64 (fractional); integer truncation requires a runtime routine. Implement `b/idiv` and `b/imod` in `backend/besm6/runtime.mad`: pop dividend and divisor, extract signs, FP-divide absolute values, adjust exponent with `,E+N, 150B` (E += 40), apply sign. `b/imod` = a − (a÷b)×b. | L |
| 21 | Integer Negate, Complement, Not | **Negate**: `6\|7 ,XTA, src` / `,NTR, 001B` / `,X-A, 0` (0−A; mem[0]=0 architecturally) / `6\|7 ,ATX, dst`. **Complement** (bitwise NOT): `6\|7 ,XTA, src` / `,AEX, __allones` / `6\|7 ,ATX, dst`. **Not** (logical): `6\|7 ,XTA, src`; in logical ω mode test with `UZA`/branch; load 0 or `__one`. | S |
| 22 | Bitwise And, Or, Xor | Direct: `6\|7 ,XTA, s1` / `6\|7 ,AAX\|AOX\|AEX, s2` / `6\|7 ,ATX, dst`. No `NTR` needed; bitwise instructions do not involve normalization. | S |
| 23 | LeftShift, RightShift | `ASN N` shifts by N−64 bits (left shift by k: N = 64−k; right shift by k: N = 64+k). For constant shift counts, emit `,ASN, (64±k)B` directly. For variable counts, build an ASX control word whose exponent field encodes the shift. Arithmetic right shift: follow logical shift with sign-fill via `,AOX,` and a sign-extension mask. | M |
| 24 | Integer comparisons | All comparisons produce INT-format 0 or 1. **Equal**: `6\|7 ,AEX, src2` (XOR; A=0 iff equal); test with `UZA`; load `__one` or 0. **LessThan** (signed): `,NTR, 001B` / `6\|7 ,A-X, src2`; ω=Additive; `U1A` branches if A[41] set (negative). GreaterThan: swap operands. LessOrEqual: invert branch. Unsigned: `b/ucmp` runtime helper. | M |
| 25 | Floating-point arithmetic | FP Add/Subtract/Multiply/Divide map directly to `A+X`, `A-X`, `A*X`, `A/X`. No `NTR` needed. FP comparisons: `A-X` sets ω=Additive; `U1A` branches if A<0. FP negate: `,X-A, 0` (mem[0] = 0.0 in FP). | M |
| 26 | Type conversions | **Truncate**: `6\|7 ,XTA, src` / `,AAX, __maskN` (AND with N-bit mask). **ZeroExtend**: same with target-width mask. **SignExtend**: mask, test sign bit, OR fill if negative. **IntToDouble**: INT word is already valid BESM-6 FP; `,A+X, 0` forces normalization. **DoubleToInt**: runtime `b/dtoi` (extract exponent, shift mantissa by 104−exp via `ASN`). UInt variants handle sign bit specially. | L |
| 27 | Label, Jump, JumpIfZero, JumpIfNotZero | **Label**: buffer; prepend to next emitted instruction. **Jump**: `,UJ, target`. **JumpIfZero**: `6\|7 ,XTA, cond` (sets ω=Logical) / `,UZA, target`. **JumpIfNotZero**: same with `,U1A,`. No `NTR` needed; `XTA` sets logical ω mode automatically. | S |
| 28 | FunCall and Return | **Call** (N args): load arg0 with `XTA`; push args 1..N−1 with `6\|7 ,XTS, off` (XTS pushes A then loads next); `14 ,VTM, -N`; `,CALL, fun`. When N=0: just `,CALL, fun`, no need for `14, VTM,`. Result in A; store with `6\|7 ,ATX, dst`. **Callee prologue**: `,ITS, 13` / `,CALL, b/save` / `15 ,UTM, L`. **Epilogue**: load result; `,UJ, b/ret`. No need to declare externals, as instruction `,CALL,` declares them automatically. | L |
| 29 | AddPtr, CopyToOffset, CopyFromOffset | **AddPtr**: for power-of-2 scale k, `,ASN, (64-k)B` on index; `,NTR, 001B` + `6\|7 ,A+X, ptr`. Non-power: call `b/imul`. **CopyToOffset/FromOffset**: `7 ,MTJ, 1` + `1 ,UTM, base_off` + `1 ,UTM, field_off`; then `1 ,ATX\|XTA, 0` for write/read. | M |

---

### Phase C — Static Data

| # | Task | Description | Effort |
|---|------|-------------|--------|
| 30 | Static variables | `StaticVariable`: emit `,ENTRY, name` if global; emit one word per `StaticInit`: `IntInit`/`CharInit` → `,INT,`; `DoubleInit` → `,REAL,`; `ZeroInit` → `,BSS, words`; `StringInit` → one `,INT, char_code` per character (1 word/char); `PointerInit` → `,OCT, addr`. | M |
| 31 | Static constants | `StaticConstant`: same as StaticVariable for a single init word. Madlen has no hardware write-protection; distinguish by convention (comment). | S |
| 32 | String constants and text encoding | Simple first implementation: one word per character, ASCII value as `,INT, code`; null terminator as `,INT, 0`. Later optimization: pack 8 characters per word in 6-bit GOST encoding with a `char`→GOST mapping table. | M |

---

### Phase D — Runtime Support Library

| # | Task | Description | Effort |
|---|------|-------------|--------|
| 33 | Runtime helpers (`backend/besm6/runtime.mad`) | Implement in Madlen using the standard C calling convention: `b/save` and `b/ret` (context save/restore per calling convention); `b/idiv` (truncated signed divide); `b/imod` (remainder); `b/imul` (for non-constant-scale use); `b/udiv`, `b/umod`, `b/ucmp` (unsigned variants); `b/dtoi` (double-to-integer). Also define constants: `__one : ,INT, 1`; `__allones : ,OCT, 7777777777777777`; truncation masks `__mask8 : ,INT, 255`, `__mask16 : ,INT, 65535`. No `__zero` needed — use `,XTA, 0`. | L |
| 34 | Program entry point (`backend/besm6/startup.mad`) | Emit the `main` subprogram that calls the C-compiled `b/main`: set r14=0, `,CALL, b/main`, then `,STOP,`. Declare `b/main` external with `,SUBP,`. Link `startup.mad` first so the Dubna loader finds the `main` entry point. | M |

---

### Phase F — Integration and Testing

| # | Task | Description | Effort |
|---|------|-------------|--------|
| 38 | Tests | Unit tests for the Madlen emitter (`madlen_tests.cpp`) and frame allocator (`frame_tests.cpp`) already pass. Remaining: **Integration** (`backend/besm6/codegen_tests.cpp`): hand-crafted `Tac_Program` values through the full pipeline compared against golden `.mad` files in `backend/besm6/testdata/`; programs: identity function, recursive factorial, simple loop, struct field copy. **Smoke**: `./parse tests/smoke.c \| ./lower \| ./genbesm` and verify the `.mad` is well-formed. | L |
