Here is a summary of the key design decisions and how RISC-V differs from the x86_64 and AArch64 schemas.

**The register model is flat and orthogonal.** `xreg` is a simple 32-variant enum `X0`–`X31`; `freg` is `F0`–`F31`. There are no sub-register variants (no W/H/B views), no special-purpose register union (no XZR/SP ambiguity). `X0` is physically present in the type — it is the zero register by convention, not by exclusion — which is the correct model for an ISA where `ADDI X0, X0, 0` is the canonical NOP and `JAL X0, label` is an unconditional jump.

**No condition flags, no condition codes.** Unlike x86 (EFLAGS) and AArch64 (NZCV), RISC-V has no flag register at all. The six branch instructions (`BEQ`, `BNE`, `BLT`, `BGE`, `BLTU`, `BGEU`) each take two source registers and perform the comparison inline. The schema reflects this directly: `branch_instr` carries `(xreg rs1, xreg rs2, label lbl)` with no cc field.

**Explicit rounding mode on every FP arithmetic instruction.** The `round_mode` type (`RNE`, `RTZ`, `RDN`, `RUP`, `RMM`, `DYN`) is a field of every FP arithmetic constructor, including the four-register fused multiply-add family. This is architecturally mandated — the hardware encodes the 3-bit `rm` field in the instruction word itself. `DYN` means the hardware reads `FCSR.frm` at runtime, which is what compilers typically emit when the language doesn't guarantee a specific rounding mode.

**F and D are separate extension groups.** The file splits single-precision (`fp_single_*`) and double-precision (`fp_double_*`) into separate sub-types. Precision conversion (`FCVT.S.D` / `FCVT.D.S`) lives in its own `fp_prec_cvt_instr` group. This makes it easy for a backend that supports only F (no D) to exclude the entire `IFPDouble*` family from the instr wrapper.

**FCLASS and comparison results go to integer registers.** `FEQ_S`, `FLT_S`, `FLE_S`, and `FCLASS_S` all write to an `xreg` destination — this is a direct ISA property of RISC-V (unlike x86 where FP comparisons set EFLAGS or SSE comparison masks stay in XMM). The schema captures this by typing `rd` as `xreg` in these instructions.

**FSGNJ, FSGNJN, FSGNJX unify four common pseudo-instructions.** Setting `rs1 = rs2` in `FSGNJ_S` gives `FMV.S` (copy); in `FSGNJN_S` gives `FNEG.S`; in `FSGNJX_S` gives `FABS.S`. These aliases are noted in comments rather than represented as additional constructors, keeping the variant count compact.

**Atomic ordering is a single enum, not two booleans.** RISC-V atomics have independent `aq` and `rl` bits, giving four combinations. Modelling them as `amo_order = AMO_Relaxed | AMO_Acquire | AMO_Release | AMO_AcqRel` rather than `bool aq, bool rl` avoids the illegal fifth state and matches how code generators reason about memory ordering (which maps directly to C++ `memory_order` semantics).

**Memory addressing is a single form.** `mem_addr = MemAddr(xreg base, int offset)` — that's it. No scaled index, no SIB, no pre/post-index. This is RISC-V's radical simplicity: all addressing is base + 12-bit signed offset. The encoder validates the range.

**`cf_terminator` uses three structural categories.** Rather than overloading the `cf_instr` type as in the AArch64 schema, the RISC-V block terminator is `CFBranch | CFJump | CFTrap`. This reflects a meaningful distinction: `CFBranch` has two successors (taken / not-taken), `CFJump` has one (or unknown for `JALR` indirect), and `CFTrap` (`ECALL`, `EBREAK`, `MRET`, `SRET`) causes a privilege-mode transition and has no ordinary successor.

**Six calling conventions.** The ABI name encodes both integer width (`ILP32` for RV32, `LP64` for RV64) and the FP passing convention (none / `F` / `D`). The `func` type carries separate callee-saved lists for integer (`s0`–`s11`) and FP registers (`fs0`–`fs11`), along with a `target_abi` field on the module that sets the module-wide default.
