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

## x86_64 Backend

The backend consumes binary TAC produced by `lower` and emits GNU AT&T assembly (`.s` files)
for the System V AMD64 ABI. Assemble and link with:

```
./parse prog.c | ./lower - prog.tac && ./genx86 prog.tac prog.s
cc -no-pie prog.s -o prog
```

On macOS, C symbol names carry an underscore prefix (`_main`), section names differ
(`__TEXT,__text`, `__DATA,__data`, `__TEXT,__const`), and `.align N` means 2^N bytes (log2)
rather than N bytes (Linux GAS). Control via a compile-time `PLATFORM_MACOS` macro.

### Calling convention summary (System V AMD64 ABI)

| Register(s) | Role |
|-------------|------|
| rdi, rsi, rdx, rcx, r8, r9 | Integer/pointer arguments 1–6 (in order) |
| xmm0–xmm7 | Floating-point arguments 1–8 (in order) |
| rax | Integer/pointer return value |
| xmm0 | Double return value |
| rbx, rbp, r12–r15 | Callee-saved — must be preserved across calls |
| rax, rcx, rdx, rsi, rdi, r8–r11 | Caller-saved — may be clobbered by a call |
| rsp | Stack pointer — must be 16-byte-aligned at every `callq` |

Stack grows downward. At function entry (after `callq` pushes the return address), RSP is
8-byte aligned; the prologue pushes RBP (8 bytes), making RSP 16-byte aligned again before any
further adjustments.

### Stack frame layout

```
rbp + 8*(2+k)   stack argument k  (7th integer arg = rbp+16, 8th = rbp+24, …)
rbp + 8         return address     (pushed by callq)
rbp + 0         saved rbp          (pushed by prologue)
rbp - 8         spilled param 0    (register args copied here in prologue)
rbp - 16        spilled param 1
   …
rbp - N         last local / temp
```

All TAC variables — params and temporaries alike — occupy one 8-byte slot on the stack.
Register arguments (params 0–5 for integers, 0–7 for doubles) are copied to their assigned
slots at the start of the function prologue. Stack-passed arguments (integer params 6+) already
reside above RBP and are referenced via positive RBP offsets.

Frame size is padded to the nearest multiple of 16 after accounting for callee-saved register
saves (each is an additional 8-byte push). This ensures RSP is 16-byte aligned before any
`callq` issued by the function body.

---

### Phase A — Infrastructure

| # | Task | Description | Effort |
|---|------|-------------|--------|
| 12 | Backend directory and build | Create `backend/x86/` with `CMakeLists.txt` producing the `genx86` executable (reads binary TAC, writes GNU AT&T `.s`). Wire into the top-level `CMakeLists.txt` and `Makefile` alongside `parse` and `lower`. | S |
| 13 | Type ABI (`backend/x86/abi.h`) | Define byte sizes and natural alignments per System V AMD64 ABI: `char`/`uchar`=1, `short`/`ushort`=2, `int`/`uint`=4, `long`/`ulong`/`long long`/`ulong long`/pointer=8, `double`=8. All TAC variables occupy an 8-byte stack slot regardless of declared width (sub-word narrowing happens explicitly via Truncate/ZeroExtend/SignExtend). Provide `x86_sizeof(Tac_TypeKind)` and `x86_alignof(Tac_TypeKind)`. | S |
| 14 | Register name tables (`backend/x86/abi.h`) | Define arrays of register names by size suffix: `REG_QWORD[]` = {"rax","rcx","rdx","rsi","rdi","r8","r9","r10","r11","rbx","r12","r13","r14","r15"}, and the 32/16/8-bit counterparts (eax/ecx/…, ax/cx/…, al/cl/…). Define `ARG_REGS_INT[]` = {rdi,rsi,rdx,rcx,r8,r9}, `ARG_REGS_XMM[]` = {xmm0,…,xmm7}, `CALLEE_SAVED[]` = {rbx,r12,r13,r14,r15}. | S |
| 15 | GNU AT&T assembly emitter (`backend/x86/asm.c/h`) | `AsmCtx` context struct plus API: `asm_section(name)`, `asm_globl(sym)`, `asm_align(bytes)`, `asm_label(name)`, `asm_instr2(op, src, dst)`, `asm_instr1(op, operand)`, `asm_instr0(op)`, `asm_comment(text)`, `asm_fresh_label(prefix)`. Operand helpers: `asm_reg(name)` → `"%rax"`; `asm_imm(n)` → `"$42"`; `asm_mem_rbp(off)` → `"-16(%rbp)"`; `asm_mem_reg(base)` → `"(%rax)"`; `asm_rip(sym)` → `"_sym(%rip)"`. Data directives: `asm_byte`, `asm_long`, `asm_quad`, `asm_double_hex`, `asm_string`, `asm_zero(bytes)`. All output goes to a `FILE*` member of `AsmCtx`. | L |
| 16 | Frame allocator and type map (`backend/x86/frame.c/h`) | Walk `Tac_TopLevel` params and instruction body in two passes. **Pass 1 (type inference)**: For each TAC variable name, infer `FrameKind` ∈ {INT_SIGNED, INT_UNSIGNED, DOUBLE}. Rules: a variable set by `IntToDouble`/`UIntToDouble` → DOUBLE; set by `ZeroExtend` → INT_UNSIGNED; set by `SignExtend`/`DoubleToInt` → INT_SIGNED; set by `DoubleToUInt` → INT_UNSIGNED; constant kind maps directly. Unresolved → INT_SIGNED. **Pass 2 (layout)**: Assign each unique name an 8-byte slot at `rbp - offset`, growing downward. Params 0–5 (integer) and 0–7 (double) get negative-rbp slots; params beyond those limits get positive-rbp offsets (already on stack). Track which callee-saved registers are used (needed for prologue/epilogue). Provide `frame_build`, `frame_lookup(name, *offset, *kind)`, `frame_size`, `frame_used_callee_regs`, `frame_free`. | M |

---

### Phase B — Instruction Selection

All integer operations use 64-bit registers (rax, rcx, r10 as scratch); sub-word arithmetic is
performed at full 64-bit width and narrowed only when Truncate/ZeroExtend/SignExtend instructions
say so. Double operations use SSE2 scalar instructions on xmm0/xmm1.

The per-instruction pattern is: load src operand(s) from stack → operate in registers → store
result to stack. One memory operand is allowed on most ALU instructions (e.g., `addq
src(%rbp), %rax`) to reduce load/store count.

| # | Task | Description | Effort |
|---|------|-------------|--------|
| 17 | Copy and GetAddress | **Copy (integer)**: `movq src(%rbp), %rax` / `movq %rax, dst(%rbp)`. **Copy (double)**: `movsd src(%rbp), %xmm0` / `movsd %xmm0, dst(%rbp)`. **GetAddress (local)**: `leaq src(%rbp), %rax` / `movq %rax, dst(%rbp)`. **GetAddress (global/static)**: `leaq _sym(%rip), %rax` / `movq %rax, dst(%rbp)`. Distinguish local from global by looking up the name in the frame map; if absent, treat as global. | M |
| 18 | Load and Store (pointer dereference) | **Load**: `movq ptr(%rbp), %rax` / `movq (%rax), %rcx` / `movq %rcx, dst(%rbp)`. Use `movsd` for double. **Store**: `movq dst_ptr(%rbp), %rax` / `movq src(%rbp), %rcx` / `movq %rcx, (%rax)`. Use `movsd` for double. Sub-word loads/stores (for Truncate/ZeroExtend contexts) addressed in task 28. | S |
| 19 | Integer Add and Subtract | **Add**: `movq s1(%rbp), %rax` / `addq s2(%rbp), %rax` / `movq %rax, dst(%rbp)`. **Subtract**: same with `subq`. Immediate constant src2: emit `addq $N, %rax` directly. | S |
| 20 | Integer Multiply | `movq s1(%rbp), %rax` / `imulq s2(%rbp), %rax` (two-operand form; result in rax, high bits discarded) / `movq %rax, dst(%rbp)`. Constant multiplier: `imulq $N, s1(%rbp), %rax`. | S |
| 21 | Integer Divide and Remainder | **Signed divide**: `movq dividend(%rbp), %rax` / `cqo` (sign-extend RAX → RDX:RAX) / `idivq divisor(%rbp)` / quotient in rax, remainder in rdx. **Unsigned divide**: `movq dividend(%rbp), %rax` / `xorq %rdx, %rdx` / `divq divisor(%rbp)`. Store rax for Divide, rdx for Remainder. Use `frame_lookup` signedness to select signed vs unsigned variant. Constant divisor: move to a scratch register first (IDIV does not accept immediates). | M |
| 22 | Integer Negate, Complement, Not | **Negate** (unary minus): `movq src(%rbp), %rax` / `negq %rax` / store. **Complement** (bitwise NOT): `movq src(%rbp), %rax` / `notq %rax` / store. **Not** (logical NOT): `movq src(%rbp), %rax` / `testq %rax, %rax` / `sete %al` / `movzbq %al, %rax` / store. | S |
| 23 | Bitwise And, Or, Xor | Load s1 → rax; `andq|orq|xorq s2(%rbp), %rax`; store. Immediate operand: `andq $mask, %rax`. | S |
| 24 | LeftShift and RightShift | Shift count must be in CL or an immediate. Load src1 → rax; load src2 → rcx; `salq %cl, %rax` (left shift); `sarq %cl, %rax` (signed right shift) or `shrq %cl, %rax` (unsigned). Use `frame_lookup` signedness of src1 to choose SAR vs SHR. For constant count: `salq $N, %rax`. Store result. | S |
| 25 | Integer comparisons | General pattern: `movq s1(%rbp), %rax` / `cmpq s2(%rbp), %rax` / SETcc `%al` / `movzbq %al, %rax` / `movq %rax, dst(%rbp)`. SETcc selection by operator and signedness from `frame_lookup`: Equal→SETE, NotEqual→SETNE; signed: LessThan→SETL, LessOrEqual→SETLE, GreaterThan→SETG, GreaterOrEqual→SETGE; unsigned: LessThan→SETB, LessOrEqual→SETBE, GreaterThan→SETA, GreaterOrEqual→SETAE. Signedness is that of src1. | S |
| 26 | Double arithmetic | **Add**: `movsd s1(%rbp), %xmm0` / `addsd s2(%rbp), %xmm0` / `movsd %xmm0, dst(%rbp)`. Similarly `subsd`, `mulsd`, `divsd`. **Negate**: load 0.0 into xmm1 (`xorpd %xmm1, %xmm1`), then `subsd src(%rbp), %xmm1` (0 − src), `movsd %xmm1, dst(%rbp)`. | M |
| 27 | Double comparisons | `movsd s1(%rbp), %xmm0` / `ucomisd s2(%rbp), %xmm0` / SETcc `%al` / `movzbq %al, %rax` / store. UCOMISD sets CF and ZF: ZF=1 if equal; CF=1 if s1 < s2 (unordered or below). SETcc selection: Equal→SETE, NotEqual→SETNE, LessThan→SETB, LessOrEqual→SETBE, GreaterThan→SETA, GreaterOrEqual→SETAE. Handle NaN: UCOMISD sets PF on unordered; for == and != emit `setnp`/`setp` guard if NaN-safe behavior is required (defer to a follow-up task). | M |
| 28 | Type conversions | **Truncate** (wide→narrow, e.g. 64→8): load src → rax; `movq %rax, dst(%rbp)` (upper bits already irrelevant in 8-byte slot; caller narrows via MOVZX/MOVSX when reading). **SignExtend**: `movq src(%rbp), %rax` / `movsbq %al, %rax` (8→64) or `movswq %ax, %rax` (16→64) or `movslq %eax, %rax` (32→64) / store. **ZeroExtend**: `movzbq %al, %rax` (8→64) / `movzwq %ax, %rax` (16→64) / `movl %eax, %eax` (32→64; upper 32 bits zeroed automatically by movl) / store. **IntToDouble**: `movq src(%rbp), %rax` / `cvtsi2sdq %rax, %xmm0` / `movsd %xmm0, dst(%rbp)`. **DoubleToInt**: `movsd src(%rbp), %xmm0` / `cvttsd2siq %xmm0, %rax` (truncate-toward-zero per C) / store. **UIntToDouble**: `movl src(%rbp), %eax` (upper 32 bits zero-extended by movl) / `cvtsi2sdq %rax, %xmm0` / store. **DoubleToUInt**: `movsd src(%rbp), %xmm0` / `cvttsd2siq %xmm0, %rax` / store (wraps correctly for values in [0, 2^63−1]; values in [2^63, 2^64) need a two-step bias correction — defer to a follow-up task). | L |
| 29 | Label, Jump, JumpIfZero, JumpIfNotZero | **Label**: emit `name:`. **Jump**: `jmp target`. **JumpIfZero (integer)**: `movq cond(%rbp), %rax` / `testq %rax, %rax` / `je target`. **JumpIfNotZero (integer)**: same with `jne`. **JumpIfZero (double)**: `movsd cond(%rbp), %xmm0` / `xorpd %xmm1, %xmm1` / `ucomisd %xmm1, %xmm0` / `je target`. Distinguish integer vs double using `frame_lookup kind`. | S |
| 30 | FunCall and Return | **Callee prologue**: `pushq %rbp` / `movq %rsp, %rbp` / save callee-saved regs that `frame_used_callee_regs` reports (each: `pushq %rXX`) / `subq $frame_size, %rsp` / spill integer args: `movq %rdi, p0(%rbp)`, `movq %rsi, p1(%rbp)`, …; spill double args: `movsd %xmm0, pd0(%rbp)`, …. **Callee epilogue** (shared label `.Lfun.ret`): load return value → `%rax` or `%xmm0` based on type / restore callee-saved regs in reverse / `movq %rbp, %rsp` / `popq %rbp` / `ret`. **Return instruction**: if src is non-null, load it; then `jmp .Lfun.ret`. **Caller side of FunCall**: load args into ARG_REGS_INT / ARG_REGS_XMM (first 6/8); push excess args right-to-left (`pushq`); if total stack-push count is odd, emit `subq $8, %rsp` alignment pad first; `callq _fun_name`; clean up with `addq $N, %rsp`. Store return value from `%rax` or `%xmm0` to dst. For variadic calls, set `%al` = count of double args in XMM registers before `callq`. | L |
| 31 | AddPtr, CopyToOffset, CopyFromOffset | **AddPtr**: load ptr → rax; load index → rcx; if scale ∈ {1,2,4,8}, emit `leaq (%rax,%rcx,scale), %rax`; otherwise `imulq $scale, %rcx` / `addq %rcx, %rax`; store. **CopyToOffset**: `leaq base(%rbp), %rax` (if local) or `leaq _sym(%rip), %rax` (if global) / `addq $offset, %rax` / load src → rcx / `movq %rcx, (%rax)`. **CopyFromOffset**: compute address → rax / `movq (%rax), %rcx` / store to dst. Handle double fields via MOVSD. | M |

---

### Phase C — Static Data

| # | Task | Description | Effort |
|---|------|-------------|--------|
| 32 | Static variables | `StaticVariable`: emit `.globl _name` if global. Initialized data goes in `.data`; zero-only data goes in `.bss`. For each `StaticInit` in the init list: `I8/U8` → `.byte N`; `I32/U32` → `.long N`; `I64/U64` → `.quad N`; `DoubleInit` → `.quad 0xHHHHHHHHHHHHHHHH` (bit-cast via `memcpy`); `ZeroInit N` → `.zero N`; `StringInit null_terminated` → `.asciz "…"`, otherwise `.ascii "…"`; `PointerInit name` → `.quad _name`. Emit `.align N` before each symbol using `x86_alignof` (log2 value on macOS, byte count on Linux). | M |
| 33 | Static constants | `StaticConstant`: emit in `.section .rodata` (Linux) or `.section __TEXT,__const` (macOS). No `.globl`. Same data directive rules as task 32. Double constants in `.rodata` are referenced from code via `_sym(%rip)` (RIP-relative). | S |
| 34 | String literals | `StringInit` with `null_terminated=true` → `.asciz "…"` (GAS appends `\0`). Without null → `.ascii "…"`. Escape non-printable bytes as `\NNN`. For string constants used as static initializers, generate a private label (`L.str.N`) in `.rodata` (or `__TEXT,__cstring` on macOS) and reference it via `.quad L.str.N` in the parent static variable. | S |

---

### Phase D — ABI and Stack Details

| # | Task | Description | Effort |
|---|------|-------------|--------|
| 35 | Stack alignment and callee-saved register management | After the prologue, RSP must be 16-byte aligned before any `callq`. Callee-saved register pushes occur after `pushq %rbp` and before `subq $frame_size, %rsp`. Let S = number of callee-saved pushes; the total stack consumed before the `subq` is `8*(1+S)` bytes (including saved rbp). Pad `frame_size` so that `8*(1+S) + frame_size` is a multiple of 16. If the function makes no calls (leaf function) and the red zone (128 bytes below RSP) covers all locals, omit the `subq` and use RSP-relative addressing directly. | M |
| 36 | Stack argument passing for calls with more than 6 integer or 8 double args | Integer args 7+ and double args 9+ are pushed right-to-left before `callq`. Each push is 8 bytes. If the total number of such pushes is odd, emit a pre-push `subq $8, %rsp` alignment pad. After `callq`, clean up with `addq $total_pushed, %rsp` (including pad). Ensure the frame allocator assigns positive-rbp offsets for parameters that arrive this way in the callee (7th param at `rbp+16`, 8th at `rbp+24`, etc.). | M |

---

### Phase E — Integration and Testing

| # | Task | Description | Effort |
|---|------|-------------|--------|
| 37 | `genx86` executable (`backend/x86/main.c`) | Read binary TAC from a file or stdin (`-`); write GNU AT&T assembly to a `.s` file or stdout (`--stdout`). Support `-D` for instruction-selection trace (print each TAC instruction before its emitted code). Full pipeline: `./parse prog.c \| ./lower - prog.tac && ./genx86 prog.tac prog.s && cc -no-pie prog.s -o prog`. | M |
| 38 | Codegen driver (`backend/x86/codegen.c`) | `codegen_program(Tac_Program*, AsmCtx*)` iterates `decls` and dispatches to: `codegen_function` (emit `.globl` + label + prologue + per-instruction dispatch via `codegen_instr` + epilogue + `.size` on Linux); `codegen_static_var`; `codegen_static_const`. `codegen_instr(Tac_Instruction*, FrameMap*, AsmCtx*)` switches on `kind` and calls the appropriate helper from Phase B. | M |
| 39 | Tests | **Unit** (`backend/x86/emitter_tests.cpp`): each `asm_emit_*` call produces the expected AT&T text; `frame_build` assigns correct `rbp - offset` slots and correct `FrameKind` for each variable. **Integration** (`backend/x86/codegen_tests.cpp`): hand-craft `Tac_Program` values, run through the full pipeline, assemble with `as`, link with `cc -no-pie`, execute and compare stdout; test programs: identity function (int, double), iterative factorial, pointer write-back, struct field copy. Golden `.s` files in `backend/x86/testdata/`. **Smoke** (`make x86-smoke`): `./parse tests/smoke.c \| ./lower \| ./genx86 --stdout` and verify the output assembles and links without error. | L |
