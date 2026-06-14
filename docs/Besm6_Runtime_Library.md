## Compiler-Support Routines

These routines are emitted automatically by the compiler. They implement the C calling
convention and the operators that have no direct BESM-6 instruction equivalents.

For a detailed description of the calling convention, see
[Besm6_Calling_Conventions.md](Besm6_Calling_Conventions.md).
For the integer storage model, see
[Besm6_Data_Representation.md](Besm6_Data_Representation.md).

### Integer model

C integers are stored as raw words with the exponent field (bits 48–42) = 0:

- **Signed `int`**: 41-bit sign-magnitude; bit 41 = sign, bits 40:1 = magnitude.
  Range −2⁴⁰ … 2⁴⁰−1.
- **Unsigned `int`**: full 48-bit unsigned value. Range 0 … 2⁴⁸−1.

`b/save` leaves the AU mode register **R = 7** (binary `000111`):

- Bits 5–3 = `001`: **Logical** ω mode — after logical operations (XOR, AND, OR, loads),
  `UZA`/`U1A` test whether A = 0 / A ≠ 0.
- Bit 2 = 1: suppress rounding after normalization.
- Bit 1 = 1: suppress normalization after arithmetic.

After subtraction instructions (`A-X`, `X-A`, `A+X`), the hardware automatically switches
R to **Additive** ω mode (bits 5–3 = `100`): `UZA` then branches when A ≥ 0 (bit 41 = 0),
`U1A` when A < 0 (bit 41 = 1).

The arithmetic helpers (`b/mul`, `b/div`, `b/mod`, and their unsigned counterparts)
borrow the FP unit by temporarily converting operands to **INT-format** (exponent field
set to `0150B` = 104 decimal), which places the mantissa where `A*X` / `A/X` expect it.
This is a transient representation used only inside those helpers; it is never the
storage format for C values.

---

### Runtime Helper Convention

All arithmetic, comparison, and conversion helpers use a lightweight calling convention
distinct from the full C ABI:

- **First operand `a`**: at the stack top — r15 points one word past it, so `a` is at
  `mem[r15−1]`.
- **Second operand `b`**: in the accumulator (A).
- **Result**: left in A; r15 is not modified (the compiler adjusts the stack after the
  call).
- **Register contract**: helpers may freely modify scratch index registers (including r14)
  but must preserve r6 and r7.
- **Invocation**: a plain `13 ,UJ, b/xxx` jump — not the full `ITS`/`VJM`/`b/save`
  protocol. The return address is in r13 on entry to the helper.

`b/save`, `b/save0`, and `b/ret` follow the full C ABI described below; they are separate
from the helper convention.

---

### `b/save` — [b_save.madlen](../backend/besm6/libc/b_save.madlen)

Called on entry to every C function that has **one or more parameters**.

The compiler emits:

```
   ,its, 13         ; push return-to-caller address (in r13) onto the stack
13 ,vjm, b/save     ; call b/save; r13 ← address of the first instruction of the function body
```

**Source walkthrough:**

```
15 ,j+m, 14         ; r15 += r14  (r14 = −N; rewinds r15 past the N argument slots)
   ,its, 7          ; push r7 (caller's auto-variable pointer)
   ,its, 6          ; push r6 (caller's parameter pointer)
   ,its, 5          ; push r5 (caller's scratch register)
   ,its,            ; push A  (= last argument argN, passed in the accumulator)
14 ,mtj, 6          ; r6 = r14  (set parameter pointer; r6+i addresses param[i])
15 ,mtj, 7          ; r7 = r15  (set auto-variable pointer; r7+j addresses local[j])
   ,ntr, 7          ; R = 7: Logical ω + suppress normalization + suppress rounding
13 ,uj,             ; jump to r13 (the function body's first instruction)
```

**After `b/save`:**

- r6 = parameter pointer; `r6 + i` addresses the i-th argument.
- r7 = auto-variable pointer; `r7 + j` addresses the j-th local variable.
- The stack holds the saved r7, r6, r5 from the caller, plus argN.
- R = 7 (integer arithmetic mode).

---

### `b/save0` — [b_save0.madlen](../backend/besm6/libc/b_save0.madlen)

Called on entry to every C function with **no parameters**.

The compiler emits the same `ITS`/`VJM` prologue as for `b/save`. Because no arguments
were pushed, `b/save0` synthesises a valid r6 without the `j+m` adjustment:

```
15 ,mtj, 14         ; r14 = r15  (save current stack top into r14)
14 ,utm, -1         ; r14 -= 1   (one below the current stack top)
   ,its, 7          ; push r7
   ,its, 6          ; push r6
   ,its, 5          ; push r5
   ,its,            ; push A     (no meaningful last argument; frame stays uniform)
14 ,mtj, 6          ; r6 = r14   (parameter pointer set to the synthesised base)
15 ,mtj, 7          ; r7 = r15
   ,ntr, 7          ; R = 7
13 ,uj,             ; continue to function body
```

The synthesised r14/r6 value makes the frame layout identical to a one-argument call,
so `b/ret` can use the same unwind calculation regardless of parameter count.

---

### `b/ret` — [b_ret.madlen](../backend/besm6/libc/b_ret.madlen)

Called at every exit point of a C function. The return value must be in A before
jumping to `b/ret`.

```
 6 ,mtj, 14         ; r14 = r6  (save parameter-base index for stack unwind)
 7 ,mtj, 15         ; r15 = r7  (reset stack pointer to the saved-register block)
 7 ,stx, -5         ; locate the saved-register block relative to r7
   ,sti, 5          ; pop saved r5
   ,sti, 6          ; pop saved r6  (caller's parameter pointer)
   ,sti, 7          ; pop saved r7  (caller's auto-variable pointer)
   ,sti, 13         ; pop saved return address into r13
14 ,mtj, 15         ; r15 = r14  (restore caller's pre-argument stack level)
13 ,uj,             ; return to caller
```

**Actions:** restores the caller's r5, r6, r7, and r13; unwinds r15 to the level it had
before any arguments were pushed; then jumps to r13.

---

### `b/true` — [b_true.madlen](../backend/besm6/libc/b_true.madlen)

A single word containing the raw integer 1:

```
,log, 1
```

BESM-6 has no load-immediate instruction for arbitrary integer values. All relational and
logical helpers load the "true" result (1) with `xta b/true` rather than constructing it
inline.

---

### Signed Integer Arithmetic

Operands are raw 48-bit words with exponent field = 0. The signed helpers convert them to
**INT-format** by ORing in the base exponent `=:64` (which sets the bits of the 7-bit
exponent field to the value `0150B` = 104 decimal). This allows the hardware FP multiply
and divide units to operate on the integer values. After the operation, the product or
quotient exponent is corrected with `a+x, =:64`, and the exponent field is stripped with
`aax, =37 7777 7777 7777` — a 42-bit mask (14 octal digits) that zeroes bits 48–42 and
leaves the 41-bit signed result (sign bit 41 + 40-bit magnitude) in A.

#### `b/mul` — [b_mul.madlen](../backend/besm6/libc/b_mul.madlen)

Computes `a * b` (signed, low 41-bit result).

```
14 ,base,*          ; set up local basing in r14 for the literal pool
   ,aox, =:64       ; A |= INT-exponent  →  b in INT-format
15 ,stx,            ; exchange A with mem[r15−1]: A ← a (raw), stacks INT-form b
   ,aox, =:64       ; A |= INT-exponent  →  a in INT-format
   ,ntr, 2          ; R = 2: enable normalization (required for FP multiply)
15 ,a*x, 1          ; A ← INT-form a × INT-form b  (FP multiply; reads stacked INT-form b)
   ,ntr, 3          ; R = 3: restore suppress-normalization + suppress-rounding
   ,a+x, =:64       ; correct product exponent (subtract the doubled INT-exponent)
   ,aax, =37 7777 7777 7777  ; mask to 41-bit signed result (strip exponent field)
13 ,uj,             ; return
```

`b/mul` is **signed only**: the INT-format bridge interprets bit 48 as the operand's sign,
so it is valid only where each operand fits the 41-bit signed range. Unsigned multiply over
the full 48-bit range uses `b/umul` (see *Unsigned Integer Arithmetic* below).

#### `b/div` — [b_div.madlen](../backend/besm6/libc/b_div.madlen)

Computes `a / b` (signed, truncated toward zero — C11 §6.5.5).

The hardware FP divide leaves the quotient as a two's-complement mantissa, so masking off
the fraction rounds toward −∞ (floors). To get C truncation, the helper divides the
**absolute values** and reapplies the sign:

1. Convert `b` to INT-format (`aox, =:64`) and record the sign word `a ^ b` (its bit 41 is
   `sign(a) ^ sign(b)`).
2. `avx` against each INT-format operand takes its absolute value and normalizes it (the
   FP divisor must be normalized).
3. `a/x` divides `|a| / |b|`; `a+x, =:64` corrects the exponent and `aax, =37 7777 7777
   7777` masks to the 41-bit result. Because the operands are non-negative, this truncates
   toward zero.
4. `avx` against the saved sign word negates the quotient when the operand signs differ.
5. A final `aox` (OR with `mem[0] = 0`, leaving the accumulator unchanged) restores the
   **logical ω-mode** that callers expect on return — the additive ω-mode left by the
   sign-reapply `avx` would otherwise invert the next `uza`/`u1a` test in the caller (e.g.
   printf's decimal-digit loop).

#### `b/mod` — [b_mod.madlen](../backend/besm6/libc/b_mod.madlen)

Computes `a % b` (signed remainder; result takes the sign of the **dividend** — C11
§6.5.5).

Uses the same absolute-value approach as `b/div` via the identity
`|r| = |a| − (|a| ÷ |b|) · |b|`, then reapplies the sign of `a`:

Like `b/div`, it is **push-based**: the dividend `a` arrives on the stack (its raw word
doubles as the sign word), and the helper pushes the normalized moduli `|b|` and `|a|`,
addressing them by negative offset and dropping all three with a final `utm` (net pop of one
word).

1. Convert each operand to INT-format (`aox, =:64`) and `avx` it against its own pushed word
   to form the normalized modulus — `|b|` then `|a|` — leaving both on the stack.
2. `a/x` gives the truncated magnitude `|q| = |a| ÷ |b|` (the exponent-correction `a+x,
   =:64` under R = 3 drops the fraction; no separate mask is needed mid-computation).
3. `a*x` forms `|q| · |b|`; `x-a` against the still-saved `|a|` yields `|r|`.
4. `a+x, =:64` + `aax, =377777 77777777` extract `|r|` as a raw integer.
5. `avx` against the raw dividend `a` reapplies its sign, then the trailing `aox`
   restores logical ω-mode (as in `b/div`).

---

### Unsigned Integer Arithmetic

The signed helpers' INT-format trick mishandles unsigned operands whose top bit (bit 48)
is set: the FP unit interprets bit 48 as the number's sign, producing incorrect results.
This affects multiply and divide (`b/umul`, `b/udiv`, `b/umod`).

Add and subtract fail for a related reason: signed `A+X`/`A-X` work only because raw
41-bit integers keep the exponent field (bits 48–42) = 0, so the additive unit adds the
mantissas directly. Full 48-bit unsigned values carry data in that field, which the
additive unit misreads as an exponent — so unsigned add/subtract need software helpers
(`b/uadd`, `b/usub`) that perform true 48-bit modular arithmetic.

Separate helpers are therefore required for the full 48-bit unsigned range.

#### `b/uadd` — [b_uadd.madlen](../backend/besm6/libc/b_uadd.madlen) — `a + b` (unsigned)

Receives two 48-bit unsigned values (`a` at `mem[r15−1]`, `b` in A). Returns the 48-bit
modular sum in A. Adds the two operands in 24-bit half-words with explicit carry propagation
from the low half into the high half, so the exponent-field bits participate as plain value
bits. Overflow wraps modulo 2⁴⁸.

#### `b/usub` — [b_usub.madlen](../backend/besm6/libc/b_usub.madlen) — `a − b` (unsigned)

Returns the 48-bit modular difference `a − b` in A. Negates `b` (complement plus one) and
adds it to `a`, handling the bit-48 carry explicitly so the exponent-field bits participate
as plain value bits. Underflow wraps modulo 2⁴⁸.

#### `b/umul` — [b_umul.madlen](../backend/besm6/libc/b_umul.madlen) — `a * b` (unsigned)

Returns the low 48 bits of the unsigned product `a * b` in A. Adapted from the two-path
`u_mul_u` routine of a sibling 64-bit/52-bit machine. The `A*X` instruction natively forms
the full 81-bit product mantissa across the A:Y pair (`A` = product bits 80:41, `Y` = bits
40:1), so the low 48 bits are `(A << 40)[48:41] | (Y & mask40)`. The helper branches on
operand magnitude — `(a|b) >> 40` — because an operand `≥ 2⁴⁰` has bits in 48:41 that the FP
unit misreads as sign/exponent and must therefore be split:

- **Short way** (both operands `< 2⁴⁰`, the common case): a *single* 40×40→low-48 multiply
  followed by the repack above. No subroutine call, no splitting.
- **Long way** (some operand `≥ 2⁴⁰`): split each operand at the 40-bit mantissa boundary,
  `a = aH·2⁴⁰ + aL`, `b = bH·2⁴⁰ + bL` (with `aL,bL < 2⁴⁰`, `aH,bH < 2⁸`), and assemble

  ```
  result = (aL·bL) + 2⁴⁰ · ((aL·bH + aH·bL) mod 2⁸)        mod 2⁴⁸
  ```

  The `aH·bH·2⁸⁰` term and all but the low 8 bits of each cross product vanish mod 2⁴⁸.

The routine uses no named temporaries and no subroutine call: the four split parts and the
partial products all live on the stack, addressed by negative offset from `r15`, and the
per-product low-bit extraction (run the hardware FP multiply on the clean, sign-bit-free
operands, then repack the low 48 bits from the A:Y pair) is inlined at each of the three
multiply sites. The two cross-product addends are each `< 2⁸`, so their sum never carries
past bit 48 and a plain `ARX` add suffices.

#### `b/udiv` — [b_udiv.madlen](../backend/besm6/libc/b_udiv.madlen) — `a / b` (unsigned) — **to be implemented**

Receives two 48-bit unsigned values (`a` at `mem[r15−1]`, `b` in A). Returns the
unsigned quotient in A, truncated toward zero.

Intended algorithm: a software restoring long-division loop operating on the full 48-bit
representation, or normalisation of the value as a non-negative FP mantissa followed by
FP division. Division by zero has implementation-defined behaviour.

#### `b/umod` — [b_umod.madlen](../backend/besm6/libc/b_umod.madlen) — `a % b` (unsigned) — **to be implemented**

Returns the unsigned remainder `a − (a÷b)·b` using `b/udiv`. Both operands and the
result span the full 48-bit unsigned range.

---

### Signed Relational and Logical Operators

Each helper receives `a` at `mem[r15−1]` and `b` in A. It returns 1 (loaded from
`b/true`) if the condition holds, 0 otherwise.

All seven routines follow the same two-branch template:

1. Perform a comparison operation (subtraction or XOR) that sets the ω mode.
2. Branch to the `true` path on the appropriate ω condition.
3. Fall-through: `xta,` loads 0 from address 0 (BESM-6 guarantees `mem[0] = 0`).
4. `true` path: `xta b/true` loads 1; return.

Inside each source file, `b/true` is declared as a `,subp,` alias that shares the
constant from [b_true.madlen](../backend/besm6/libc/b_true.madlen).

#### Branch condition details

Two instruction classes are used, each setting a different ω mode automatically:

**`AEX` (bitwise XOR, opcode 012) → Logical ω mode (bits 5–3 = `001`):**
After `AEX`, `UZA` branches when A = 0 (all bits zero); `U1A` when A ≠ 0.

**Subtraction (`A-X`, `X-A`) → Additive ω mode (bits 5–3 = `100`):**
After subtraction, `UZA` branches when A ≥ 0 (bit 41 = 0); `U1A` when A < 0 (bit 41 = 1).

`aex` with no explicit address reads from `mem[0] = 0`, so A = b XOR 0 = b; the result
tests whether b itself is zero. `15 ,aex,` reads `mem[r15−1]` = a (via pre-decrement on
r15=017), computing A = b XOR a.

`15 ,x-a,` computes A = mem[r15−1] − A = a − b (pre-decrement on r15, X operand minus A).
`15 ,a-x,` computes A = A − mem[r15−1] = b − a.

#### Operator table

| Routine | Source | C op | Key instruction | ω mode | Branch taken when |
|---------|--------|------|-----------------|--------|-------------------|
| `b/not` | [b_not.madlen](../backend/besm6/libc/b_not.madlen) | `!b` | `aex` → A = b XOR 0 = b | Logical | `uza`: A = 0 (b = 0) |
| `b/eq` | [b_eq.madlen](../backend/besm6/libc/b_eq.madlen) | `a == b` | `15 ,aex,` → A = b XOR a | Logical | `uza`: A = 0 (a = b) |
| `b/ne` | [b_ne.madlen](../backend/besm6/libc/b_ne.madlen) | `a != b` | `15 ,aex,` → A = b XOR a | Logical | `u1a`: A ≠ 0 (a ≠ b) |
| `b/lt` | [b_lt.madlen](../backend/besm6/libc/b_lt.madlen) | `a < b` | `15 ,x-a,` → A = a − b | Additive | `u1a`: A < 0 (a < b) |
| `b/le` | [b_le.madlen](../backend/besm6/libc/b_le.madlen) | `a <= b` | `15 ,a-x,` → A = b − a | Additive | `uza`: A ≥ 0 (b ≥ a) |
| `b/gt` | [b_gt.madlen](../backend/besm6/libc/b_gt.madlen) | `a > b` | `15 ,a-x,` → A = b − a | Additive | `u1a`: A < 0 (b < a) |
| `b/ge` | [b_ge.madlen](../backend/besm6/libc/b_ge.madlen) | `a >= b` | `15 ,x-a,` → A = a − b | Additive | `uza`: A ≥ 0 (a ≥ b) |

**Why pairs share an instruction:** `b/lt`/`b/ge` both compute `a − b` and test opposite
ω conditions (`U1A` vs `UZA`). `b/le`/`b/gt` both compute `b − a` and test opposite
conditions. `b/eq`/`b/ne` both XOR the operands and test A = 0 vs A ≠ 0.

**Limitation for unsigned operands:** the Additive sign test (bit 41 = sign) is valid only
for 41-bit signed integers. If either operand spans the full 48-bit unsigned range (bit 48
set), the subtraction sign test gives the wrong answer. `b/eq` and `b/ne` are
signedness-independent (XOR tests bitwise equality) and are reused for unsigned equality.
The four unsigned ordering helpers below handle the remaining cases.

---

### Unsigned Relational Operators

The subtraction sign-bit test used by `b/lt`, `b/le`, `b/gt`, `b/ge` is only valid within
the 41-bit signed range. For unsigned values in the full 48-bit range, a different
comparison is needed — for example, carry/borrow detection via `ARX` (cyclic add, opcode
013), or normalising both values as non-negative FP numbers and comparing exponents.

#### `b/ult` — [b_ult.madlen](../backend/besm6/libc/b_ult.madlen) — `a < b` (unsigned) — **to be implemented**

#### `b/ule` — [b_ule.madlen](../backend/besm6/libc/b_ule.madlen) — `a <= b` (unsigned) — **to be implemented**

#### `b/ugt` — [b_ugt.madlen](../backend/besm6/libc/b_ugt.madlen) — `a > b` (unsigned) — **to be implemented**

#### `b/uge` — [b_uge.madlen](../backend/besm6/libc/b_uge.madlen) — `a >= b` (unsigned) — **to be implemented**

Each receives the two 48-bit unsigned operands in the standard helper convention
(`a` at `mem[r15−1]`, `b` in A) and returns 0 or 1 in A.

---

### Floating-Point Relational Operators

The four FP orderings mirror their signed-integer counterparts (`a` at `mem[r15−1]`, `b` in
A; result 0/1 in A) but the operands are native 48-bit floating-point words. The signed
helpers subtract the operands as raw integers, which is wrong for FP — a native FP word is
not monotonic when read as a two's-complement integer. The FP helpers instead bracket the
subtract with `,ntr,` (R := 0, full FP mode) so the result is normalized and rounded: the
Additive sign then reflects the mathematical difference, and equal operands produce an exact
zero (so the `≥`/`≤` equality edge tests correctly). Before returning, each path restores
`R := 7` with `,ntr, 7` — the integer mode `b/save` leaves in place — so the caller's
following integer ops behave; the `,ntr, 7` must come *after* the conditional branch, since
`NTR` overwrites the ω flag that `U1A`/`UZA` test. Operands are already FP (valid exponents),
so — unlike `b/div`/`b/mul` — no INT-format bridge is needed.

| Routine | Source | Operation | Subtraction | Group | True condition |
|---------|--------|-----------|-------------|-------|----------------|
| `b/flt` | [b_flt.madlen](../backend/besm6/libc/b_flt.madlen) | `a < b` | `15 ,x-a,` → A = a − b | Additive | `u1a`: A < 0 (a < b) |
| `b/fle` | [b_fle.madlen](../backend/besm6/libc/b_fle.madlen) | `a <= b` | `15 ,a-x,` → A = b − a | Additive | `uza`: A ≥ 0 (b ≥ a) |
| `b/fgt` | [b_fgt.madlen](../backend/besm6/libc/b_fgt.madlen) | `a > b` | `15 ,a-x,` → A = b − a | Additive | `u1a`: A < 0 (b < a) |
| `b/fge` | [b_fge.madlen](../backend/besm6/libc/b_fge.madlen) | `a >= b` | `15 ,x-a,` → A = a − b | Additive | `uza`: A ≥ 0 (a ≥ b) |

Floating-point `==` and `!=` are pure bit equality (`AEX` + `UZA`/`U1A`), which is
type-independent, so they reuse `b/eq` and `b/ne` rather than dedicated FP helpers.

---

### Type Conversion Helpers

These routines convert between the native BESM-6 floating-point format (`float` ≡
`double`, a single 48-bit word with 7-bit exponent and 40-bit mantissa) and C integer
types. INT_TO_DOUBLE and UINT_TO_DOUBLE are short inline sequences (set the INT-format
exponent, then `NTR` to normalise) and do not need a helper call. DOUBLE_TO_INT and
DOUBLE_TO_UINT require extracting the mantissa at the correct shift, which is more
involved.

#### `b/dtoi` — [b_dtoi.madlen](../backend/besm6/libc/b_dtoi.madlen) — `double` → signed `int` — **to be implemented**

Receives the 48-bit native FP value in A (no first operand on the stack). Returns the
truncated 41-bit signed integer in A.

Algorithm: extract the 7-bit exponent field (bits 48–42); compute the right-shift as
`104 − exp` (where 104 = `0150B` is the INT-format base exponent for 1.0); shift the
40-bit mantissa right by that amount using `ASX` or `ASN`; apply the FP sign bit. Values
outside [−2⁴⁰, 2⁴⁰−1] have implementation-defined behaviour (C11 §6.3.1.4).

`float` and `double` use the same 48-bit format on BESM-6, so `b/dtoi` serves both
`(int)f` and `(int)d`.

#### `b/dtou` — [b_dtou.madlen](../backend/besm6/libc/b_dtou.madlen) — `double` → unsigned `int` — **to be implemented**

Same mantissa-shift algorithm as `b/dtoi` but without sign handling; returns the full
48-bit unsigned result in A. Values outside [0, 2⁴⁸−1] have implementation-defined
behaviour.

---

### I/O Routines

#### `b/tout` — [b_tout.madlen](../backend/besm6/libc/b_tout.madlen)

Writes a line to stdout via BESM-6 extracode 71 (Dubna monitor print-line service).

The caller places the KOI7-encoded string address in A before jumping to `b/tout`
(no arguments on the stack; this is not a C ABI call).

```madlen
   ,ati, 12          ; r12 ← A  (string word-address → index register 12)
   ,utc, info        ; unconditional transfer to the extracode dispatch block
   ,*71,             ; extracode 71: write line to stdout
13 ,uj,              ; return to caller

info: 12 ,040,       ; extracode control word: opcode 040, address modifier = r12
         ,   ,       ; second word of the extracode parameter block (padding)
```

`ATI` stores A into an index register. `UTC` is an unconditional transfer that sets up the
extracode parameter base at the address `info`. `*71` is the Dubna system call that outputs
the string whose word address is recorded in the `info` control word via r12.
