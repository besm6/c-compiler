# TODO

Work plan ordered by recommended implementation sequence.
Effort: S = half day, M = 1–2 days, L = 3–5 days, XL = 1–2 weeks.

---

## Frontend (Completed)

| # | Task | Location | Status |
|---|------|----------|--------|
| 1 | Scanner | `scanner/` | Done |
| 2 | Parser | `parser/` | Done |
| 3 | AST node types | `ast/ast.h`, `ast/ast.asdl` | Done |
| 4 | AST binary I/O | `ast/`, `libutil/wio` | Done |
| 5 | AST YAML / Graphviz output | `ast/` | Done |
| 6 | AST clone, compare | `ast/` | Done |
| 7 | Symbol tables | `semantic/symtab.c`, `structtab.c`, `typetab.c` | Done |
| 8 | Type checking | `semantic/typecheck.c` | Done |
| 9 | Loop labeling | `semantic/label_loops.c` | Done |
| 10 | Constant conversion | `semantic/const_convert.c` | Done |
| 11 | AST → TAC lowering | `translator/translate.c`, `expr.c`, `stmt.c` | Done |

---

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
| r13 | Return address — set by `13 ,VJM, fun` on each call |
| r14 | Negative argument count — set by caller |
| r15 | Stack pointer — grows toward higher addresses |
| r6 | Parameter pointer — set by `c/save`; `r6+i` = param[i] |
| r7 | Auto-variable pointer — set by `c/save`; `r7+j` = local[j] |
| r1–r7 | Callee-saved |

Caller pushes arguments in direct order with `XTA`/`XTS` (last arg stays in A), sets
`14 ,VTM, -N`, then calls `13 ,VJM, fun`.  Every callee starts with `,ITS, 13` (push last
arg, load r13) then `13 ,VJM, c/save` (saves r13/r7/r6, sets r6 and r7).  Return value goes
in A; epilogue is `,UJ, c/ret`.  Reading address 0 yields 0 — an architectural guarantee of
the BESM-6/Dubna system; no `__zero` constant is needed.

---

### Phase A — Infrastructure

| # | Task | Description | Effort |
|---|------|-------------|--------|
| 12 | Backend directory and build | Create `backend/besm6/` with `CMakeLists.txt` producing the `emit` executable (reads binary TAC, writes Madlen `.mad`). Wire into top-level `CMakeLists.txt` and `Makefile` alongside `parse` and `lower`. | S |
| 13 | Type ABI (`backend/besm6/abi.h`) | All C types map to one BESM-6 word (48 bits). Integers use INT-format encoding: exponent field (bits 48–42) = 104 decimal = 0150 octal, mantissa (bits 40–1) = 40-bit two's-complement value; range ±(2^40−1). Pointers are raw 15-bit words in bits 15–1. Provide `codegen_sizeof()` and `codegen_alignof()`. | M |
| 14 | Calling convention constants (`backend/besm6/abi.h`) | Define `REG_SP=15`, `REG_RET=13`, `REG_CNT=14`, `REG_PAR=6`, `REG_AUTO=7`. Document that `c/save` and `c/ret` are external runtime routines declared via `,CALL,` in every subprogram. | S |
| 15 | Madlen emitter (`backend/besm6/madlen.c/h`) | `MadCtx` context struct plus API: `mad_emit_name`, `mad_emit_end`, `mad_emit_rel`, `mad_emit_call`, `mad_emit_entry`, `mad_emit_instr(label, m_reg, mnemonic, address)`, `mad_emit_const_int/real/oct/log`, `mad_emit_bss`, `mad_emit_equ`, `mad_fresh_label`. Formatting: label at column 1, mnemonic between mandatory commas, address after second comma. | L |
| 16 | Frame allocator (`backend/besm6/frame.c/h`) | Walk `Tac_TopLevel` params and instruction body. Map each TAC param name to (REG_PAR, offset i); map each TAC temporary to (REG_AUTO, offset j). Provide `frame_build`, `frame_lookup(name, *reg, *offset)`, `frame_num_autos`, `frame_free`. | M |

---

### Phase B — Instruction Selection

All TAC variables live in the frame (params via r6, autos via r7).  A is the sole computation
register.  The pattern is: load from frame → operate → store to frame.

| # | Task | Description | Effort |
|---|------|-------------|--------|
| 17 | Copy, Load, Store, GetAddress | **Copy**: `6\|7 ,XTA, src_off` + `6\|7 ,ATX, dst_off`. **Load** (dereference): load pointer, `,ATI, 1`, then `1 ,XTA, 0`. **Store** (write-through): load pointer, `,ATI, 1`, load src, `1 ,ATX, 0`. **GetAddress**: `7 ,MTJ, 1` + `1 ,UTM, off` + `,ITA, 1` + store. | M |
| 18 | Integer Add, Subtract | Integers are in INT format. `NTR` must be placed immediately before the arithmetic instruction — `XTA` clears bit 1 of R when it sets ω mode, so `,NTR, 001B` (norm_disable) must follow the last load. Sequence: `6\|7 ,XTA, src1` / `,NTR, 001B` / `6\|7 ,A+X, src2` / `6\|7 ,ATX, dst`. Subtract uses `,A-X,`. | M |
| 19 | Integer Multiply | `A*X` on two INT-format operands gives FP exponent 104+104−64 = 144. Correct with `,E-N, 150B` (E −= 40; 144−40 = 104). Sequence: `6\|7 ,XTA, src1` / `6\|7 ,A*X, src2` / `,E-N, 150B` / `6\|7 ,ATX, dst`. | M |
| 20 | Integer Divide and Remainder | FP divide on INT operands yields exponent 64 (fractional); integer truncation requires a runtime routine. Implement `c/idiv` and `c/imod` in `backend/besm6/runtime.mad`: pop dividend and divisor, extract signs, FP-divide absolute values, adjust exponent with `,E+N, 150B` (E += 40), apply sign. `c/imod` = a − (a÷b)×b. | L |
| 21 | Integer Negate, Complement, Not | **Negate**: `6\|7 ,XTA, src` / `,NTR, 001B` / `,X-A, 0` (0−A; mem[0]=0 architecturally) / `6\|7 ,ATX, dst`. **Complement** (bitwise NOT): `6\|7 ,XTA, src` / `,AEX, __allones` / `6\|7 ,ATX, dst`. **Not** (logical): `6\|7 ,XTA, src`; in logical ω mode test with `UZA`/branch; load 0 or `__one`. | S |
| 22 | Bitwise And, Or, Xor | Direct: `6\|7 ,XTA, s1` / `6\|7 ,AAX\|AOX\|AEX, s2` / `6\|7 ,ATX, dst`. No `NTR` needed; bitwise instructions do not involve normalization. | S |
| 23 | LeftShift, RightShift | `ASN N` shifts by N−64 bits (left shift by k: N = 64−k; right shift by k: N = 64+k). For constant shift counts, emit `,ASN, (64±k)B` directly. For variable counts, build an ASX control word whose exponent field encodes the shift. Arithmetic right shift: follow logical shift with sign-fill via `,AOX,` and a sign-extension mask. | M |
| 24 | Integer comparisons | All comparisons produce INT-format 0 or 1. **Equal**: `6\|7 ,AEX, src2` (XOR; A=0 iff equal); test with `UZA`; load `__one` or 0. **LessThan** (signed): `,NTR, 001B` / `6\|7 ,A-X, src2`; ω=Additive; `U1A` branches if A[41] set (negative). GreaterThan: swap operands. LessOrEqual: invert branch. Unsigned: `c/ucmp` runtime helper. | M |
| 25 | Floating-point arithmetic | FP Add/Subtract/Multiply/Divide map directly to `A+X`, `A-X`, `A*X`, `A/X`. No `NTR` needed. FP comparisons: `A-X` sets ω=Additive; `U1A` branches if A<0. FP negate: `,X-A, 0` (mem[0] = 0.0 in FP). | M |
| 26 | Type conversions | **Truncate**: `6\|7 ,XTA, src` / `,AAX, __maskN` (AND with N-bit mask). **ZeroExtend**: same with target-width mask. **SignExtend**: mask, test sign bit, OR fill if negative. **IntToDouble**: INT word is already valid BESM-6 FP; `,A+X, 0` forces normalization. **DoubleToInt**: runtime `c/dtoi` (extract exponent, shift mantissa by 104−exp via `ASN`). UInt variants handle sign bit specially. | L |
| 27 | Label, Jump, JumpIfZero, JumpIfNotZero | **Label**: buffer; prepend to next emitted instruction. **Jump**: `,UJ, target`. **JumpIfZero**: `6\|7 ,XTA, cond` (sets ω=Logical) / `,UZA, target`. **JumpIfNotZero**: same with `,U1A,`. No `NTR` needed; `XTA` sets logical ω mode automatically. | S |
| 28 | FunCall and Return | **Call** (N args): load arg0 with `XTA`; push args 1..N−1 with `6\|7 ,XTS, off` (XTS pushes A then loads next); `14 ,VTM, -N`; `13 ,VJM, fun`. N=0: `14 ,VTM, 0` + `13 ,VJM, fun`. Result in A; store with `6\|7 ,ATX, dst`. **Callee prologue**: `,ITS, 13` / `13 ,VJM, c/save` / `15 ,UTM, L`. **Epilogue**: load result; `,UJ, c/ret`. Declare externals with `,CALL, name`. | L |
| 29 | AddPtr, CopyToOffset, CopyFromOffset | **AddPtr**: for power-of-2 scale k, `,ASN, (64-k)B` on index; `,NTR, 001B` + `6\|7 ,A+X, ptr`. Non-power: call `c/imul`. **CopyToOffset/FromOffset**: `7 ,MTJ, 1` + `1 ,UTM, base_off` + `1 ,UTM, field_off`; then `1 ,ATX\|XTA, 0` for write/read. | M |

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
| 33 | Runtime helpers (`backend/besm6/runtime.mad`) | Implement in Madlen using the standard C calling convention: `c/save` and `c/ret` (context save/restore per calling convention); `c/idiv` (truncated signed divide); `c/imod` (remainder); `c/imul` (for non-constant-scale use); `c/udiv`, `c/umod`, `c/ucmp` (unsigned variants); `c/dtoi` (double-to-integer). Also define constants: `__one : ,INT, 1`; `__allones : ,OCT, 7777777777777777`; truncation masks `__mask8 : ,INT, 255`, `__mask16 : ,INT, 65535`. No `__zero` needed — use `,XTA, 0`. | L |
| 34 | Program entry point (`backend/besm6/startup.mad`) | Emit the `main` subprogram that calls the C-compiled `c/main`: set r14=0, `13 ,VJM, c/main`, then `,STOP,`. Declare `c/main` external with `,CALL,`. Link `startup.mad` first so the Dubna loader finds the `main` entry point. | M |

---

### Phase E — Addressing and Basing

| # | Task | Description | Effort |
|---|------|-------------|--------|
| 35 | Relocatable basing (`REL`) | Format-1 instructions have a 12-bit address offset (max 4095). The `,REL,` directive tells Madlen to add a basing descriptor; the Dubna loader picks a free index register, loads the subprogram's base address, and rewrites internal Format-1 references as `M[base]+offset`. Emit `,REL,` as the second line of every subprogram (after `,NAME,`). This is fully compatible with r6/r7 frame access, which is already index-register-relative. | M |

---

### Phase F — Integration and Testing

| # | Task | Description | Effort |
|---|------|-------------|--------|
| 36 | `emit` executable (`backend/besm6/main.c`) | Read binary TAC (file or stdin via `-`); write Madlen to `.mad` file or stdout via `--stdout`. Support `-D` for instruction-selection trace. Full pipeline: `./parse prog.c \| ./lower - prog.tac && ./emit prog.tac`. | M |
| 37 | Codegen driver (`backend/besm6/codegen.c`) | `codegen_program(Tac_Program*, MadCtx*)` iterates decls and dispatches to: `codegen_function` (NAME + REL + CALL decls + frame alloc + prologue + per-instruction dispatch + epilogue + END); `codegen_static_var`; `codegen_static_const`. `codegen_instr(Tac_Instruction*, FrameMap*, MadCtx*)` switches on `kind`. | M |
| 38 | Tests | **Unit** (`backend/besm6/emitter_tests.cpp`): each `mad_emit_*` produces correct Madlen text; `frame_build` assigns correct r6/r7 offsets. **Integration** (`backend/besm6/codegen_tests.cpp`): hand-crafted `Tac_Program` values through the full pipeline, compared against golden `.mad` files in `backend/besm6/testdata/`; programs: identity function, recursive factorial, simple loop, struct field copy. **Smoke** (`make besm6-smoke`): `./parse tests/smoke.c \| ./lower \| ./emit` and verify the `.mad` is well-formed. | L |
