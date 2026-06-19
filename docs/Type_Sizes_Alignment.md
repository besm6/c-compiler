# Type Sizes and Alignment

## 1. Introduction

The C11 standard deliberately leaves the sizes of most integer types unspecified so
that each implementation can pick the sizes that are most efficient on its target
hardware. A `long` is 32 bits on a 32-bit ARM but 64 bits on a 64-bit x86. An `int`
is 16 bits on an 8-bit AVR but 32 bits on every 32-bit-and-above platform. A compiler
that targets multiple CPU families must track these differences explicitly.

### C11 minimum guarantees

The standard only guarantees minimum precisions (in bits):

| Type        | Minimum precision |
|-------------|-------------------|
| `char`      | 8                 |
| `short`     | 16                |
| `int`       | 16                |
| `long`      | 32                |
| `long long` | 64                |
| `float`     | IEEE 754 binary32 |
| `double`    | IEEE 754 binary64 |

The standard also guarantees `sizeof(char) == 1` by definition, where "1" means one
*addressable unit*, not necessarily one octet. On unusual hardware the addressable unit
may be wider than 8 bits (see Section 4 on BESM-6).

### Data models

The combination of sizes chosen for `int`, `long`, and pointers defines a *data model*.
The common models on byte-addressed machines are:

| Model | `int`  | `long` | `long long` | pointer | Typical platform |
|-------|--------|--------|-------------|---------|------------------|
| LP16  | 16-bit | 32-bit | 64-bit      | 16-bit  | AVR, MSP430      |
| ILP32 | 32-bit | 32-bit | 64-bit      | 32-bit  | ARM32, RISC-V 32 |
| LP64  | 32-bit | 64-bit | 64-bit      | 64-bit  | x86_64 (Linux/macOS), AArch64, RISC-V 64, MMIX |
| LLP64 | 32-bit | 32-bit | 64-bit      | 64-bit  | x86_64 (Windows) |

BESM-6 does not fit any of these models cleanly because it is not byte-addressed;
it is described separately in Section 3.9 and in Section 4.

---

## 2. Natural Alignment

A datum of size *N* bytes is *naturally aligned* when its address is a multiple of *N*.
For example, a 4-byte `int` is naturally aligned at addresses 0, 4, 8, 12, …

### Why alignment matters

- **Fault**: Some CPUs (many ARM Cortex-M, MIPS, SPARC) raise a hardware exception
  on a misaligned load or store.
- **Performance**: x86_64 and AArch64 accept misaligned accesses but may require two
  cache-line fetches instead of one, doubling memory latency.
- **Atomicity**: C11 `_Atomic` operations are only guaranteed lock-free when naturally
  aligned.

### Struct padding

When the compiler lays out a struct, it inserts padding bytes between members and after
the last member so that every member is naturally aligned and the struct's total size is
a multiple of its strictest member's alignment. The `offsetof` macro (from `<stddef.h>`)
returns the byte offset of each member including any padding.

```c
struct Example {      // on x86_64
    char   a;         // offset 0, size 1
    // 3 bytes padding
    int    b;         // offset 4, size 4
    double c;         // offset 8, size 8
};                    // total size 16, alignment 8
```

Compilers provide `__attribute__((packed))` (GCC/Clang) to suppress padding, but the
resulting code is slower and may fault on strict-alignment CPUs.

---

## 3. Architecture Tables

Each table lists sizes and alignments for the primitive scalar types. All values are in
bytes unless otherwise noted. "Natural" alignment means alignment equals size.

### 3.1 AVR (8-bit Harvard)

The AVR is an 8-bit RISC microcontroller with a Harvard architecture (separate
program and data memories). Data memory is byte-addressable SRAM; all alignments are 1
because the CPU reads one byte at a time and has no alignment restrictions. Multi-byte
values are accessed with multiple byte-load instructions.

The C standard requires `int` ≥ 16 bits; on AVR `int` is 16 bits (2 bytes). There is no
hardware floating-point unit. By default avr-gcc makes `double` the same size as `float`
(4 bytes) to save code space; newer toolchain versions optionally support 8-byte `double`.

Data pointers are 16 bits (2 bytes) matching the maximum SRAM address space of 64 KiB.

| Type          | Size | Alignment | Notes |
|---------------|------|-----------|-------|
| `bool`        | 1    | 1         | |
| `char`        | 1    | 1         | Unsigned by default |
| `short`       | 2    | 1         | |
| `int`         | 2    | 1         | |
| `long`        | 4    | 1         | |
| `long long`   | 8    | 1         | |
| `float`       | 4    | 1         | Software FP |
| `double`      | 4    | 1         | Same as `float` in avr-gcc default mode |
| `long double` | 4    | 1         | Same as `float` |
| pointer       | 2    | 1         | 16-bit data address |

### 3.2 MSP430 (16-bit von Neumann)

The MSP430 is a 16-bit RISC microcontroller with a unified (von Neumann) address space.
Word instructions require 2-byte alignment; multi-byte types are therefore aligned to 2.
The extended MSP430X variant adds a 20-bit address bus; on MSP430X the pointer widens
to 4 bytes (with the upper 4 bits carrying the extended address).

| Type          | Size | Alignment | Notes |
|---------------|------|-----------|-------|
| `bool`        | 1    | 1         | |
| `char`        | 1    | 1         | |
| `short`       | 2    | 2         | |
| `int`         | 2    | 2         | |
| `long`        | 4    | 2         | |
| `long long`   | 8    | 2         | |
| `float`       | 4    | 2         | Software FP |
| `double`      | 8    | 2         | Software FP |
| `long double` | 8    | 2         | Same as `double` |
| pointer       | 2    | 2         | 4 bytes on MSP430X |

### 3.3 ARM32 (32-bit, ARM EABI)

ARM32 follows the 32-bit ARM Embedded Application Binary Interface (EABI, document
IHI0042). All types are naturally aligned. The `long double` type maps to the same
representation as `double` (64-bit IEEE 754) because the ARM EABI does not define an
extended-precision format; hardware with VFP/NEON provides 32-bit and 64-bit FP only.

| Type          | Size | Alignment | Notes |
|---------------|------|-----------|-------|
| `bool`        | 1    | 1         | |
| `char`        | 1    | 1         | Unsigned by default on ARM |
| `short`       | 2    | 2         | |
| `int`         | 4    | 4         | |
| `long`        | 4    | 4         | ILP32: same size as `int` |
| `long long`   | 8    | 8         | |
| `float`       | 4    | 4         | IEEE 754 binary32 |
| `double`      | 8    | 8         | IEEE 754 binary64 |
| `long double` | 8    | 8         | Same as `double` on ARM EABI |
| pointer       | 4    | 4         | |

### 3.4 AArch64 (64-bit ARM)

AArch64 follows the 64-bit ARM Procedure Call Standard (AAPCS64, document IHI0055).
The LP64 data model is used: `long` and pointers are 64 bits. `long double` is the
full IEEE 754 binary128 quad-precision format (16 bytes).

| Type          | Size | Alignment | Notes |
|---------------|------|-----------|-------|
| `bool`        | 1    | 1         | |
| `char`        | 1    | 1         | Unsigned by default |
| `short`       | 2    | 2         | |
| `int`         | 4    | 4         | |
| `long`        | 8    | 8         | LP64 |
| `long long`   | 8    | 8         | |
| `float`       | 4    | 4         | IEEE 754 binary32 |
| `double`      | 8    | 8         | IEEE 754 binary64 |
| `long double` | 16   | 16        | IEEE 754 binary128 |
| pointer       | 8    | 8         | |

### 3.5 x86_64 (64-bit, System V ABI — Linux/macOS)

The System V AMD64 ABI uses the LP64 data model. The x87 FPU supports 80-bit extended
precision; `long double` uses this format but is stored in a 16-byte slot on the stack
(with 6 bytes of padding after the 10-byte value) to maintain 16-byte stack alignment.

Note: the Microsoft x64 ABI (Windows) uses the LLP64 model where `long` = 4 bytes.
The table below covers the System V ABI only.

| Type          | Size | Alignment | Notes |
|---------------|------|-----------|-------|
| `bool`        | 1    | 1         | |
| `char`        | 1    | 1         | |
| `short`       | 2    | 2         | |
| `int`         | 4    | 4         | |
| `long`        | 8    | 8         | LP64 |
| `long long`   | 8    | 8         | |
| `float`       | 4    | 4         | IEEE 754 binary32 |
| `double`      | 8    | 8         | IEEE 754 binary64 |
| `long double` | 16   | 16        | x87 80-bit value in a 16-byte slot |
| pointer       | 8    | 8         | |

### 3.6 RISC-V 32 (32-bit, ilp32 ABI)

RISC-V is an open-standard ISA designed at UC Berkeley. The 32-bit variant (RV32I)
uses the ilp32 ABI defined in the RISC-V ELF psABI document. It follows the ILP32
data model — the same sizes as ARM32. RISC-V is little-endian. The base ISA has no
floating-point; the F and D standard extensions add hardware binary32 and binary64 FP.
`long double` is 128-bit (IEEE 754 binary128) implemented entirely in software; no
RISC-V extension provides hardware quad-precision.

| Type          | Size | Alignment | Notes |
|---------------|------|-----------|-------|
| `bool`        | 1    | 1         | |
| `char`        | 1    | 1         | |
| `short`       | 2    | 2         | |
| `int`         | 4    | 4         | |
| `long`        | 4    | 4         | ILP32: same size as `int` |
| `long long`   | 8    | 8         | |
| `float`       | 4    | 4         | IEEE 754 binary32 (F extension) |
| `double`      | 8    | 8         | IEEE 754 binary64 (D extension) |
| `long double` | 16   | 16        | IEEE 754 binary128, software only |
| pointer       | 4    | 4         | |

### 3.7 RISC-V 64 (64-bit, lp64 ABI)

The 64-bit RISC-V variant (RV64I) uses the lp64 ABI. It follows the LP64 data model —
the same integer sizes as x86_64 and AArch64. Like RV32, `long double` is 128-bit IEEE
754 binary128 in software. RISC-V is little-endian.

| Type          | Size | Alignment | Notes |
|---------------|------|-----------|-------|
| `bool`        | 1    | 1         | |
| `char`        | 1    | 1         | |
| `short`       | 2    | 2         | |
| `int`         | 4    | 4         | |
| `long`        | 8    | 8         | LP64 |
| `long long`   | 8    | 8         | |
| `float`       | 4    | 4         | IEEE 754 binary32 (F extension) |
| `double`      | 8    | 8         | IEEE 754 binary64 (D extension) |
| `long double` | 16   | 16        | IEEE 754 binary128, software only |
| pointer       | 8    | 8         | |

### 3.8 MMIX (64-bit, Knuth)

MMIX is Donald Knuth's 64-bit RISC architecture, described in *The Art of Computer
Programming* fascicle 1 and implemented in the MMIXware simulator. It uses a 2⁶⁴-byte
byte-addressed virtual address space and is big-endian. MMIX has 256 general-purpose
64-bit registers and provides native load/store instructions for bytes (LDBU/STBU),
wydes (16-bit: LDWU/STW), tetras (32-bit: LDTU/STT), and octas (64-bit: LDO/STO).

MMIX follows the LP64 model for integers. The FPU handles IEEE 754 binary32 and binary64
natively; there is no hardware quad-precision. The GCC MMIX port therefore maps
`long double` to the same 8-byte representation as `double` (`LONG_DOUBLE_TYPE_SIZE=64`),
the same choice made by ARM32 — unlike AArch64, x86_64, and both RISC-V variants which
all use 16-byte `long double`.

| Type          | Size | Alignment | Notes |
|---------------|------|-----------|-------|
| `bool`        | 1    | 1         | |
| `char`        | 1    | 1         | |
| `short`       | 2    | 2         | |
| `int`         | 4    | 4         | |
| `long`        | 8    | 8         | LP64 |
| `long long`   | 8    | 8         | |
| `float`       | 4    | 4         | IEEE 754 binary32 |
| `double`      | 8    | 8         | IEEE 754 binary64 |
| `long double` | 8    | 8         | Same as `double`; no wider FP hardware |
| pointer       | 8    | 8         | |

### 3.9 BESM-6 (48-bit word-oriented)

The BESM-6 is a 48-bit Soviet mainframe from the 1960s. Its memory model differs
fundamentally from all architectures above: see Section 4 for a detailed explanation.
The minimum addressable unit is one 48-bit *word*; there is no hardware byte access.

The address space is 32,768 words (15-bit word address). Addresses increment per word,
not per byte. For a C compiler the consequences are:

- `CHAR_BIT` = 48 (one addressable unit = 48 bits).
- Pointer arithmetic on any scalar type advances the address by 1 (one word) — every
  scalar type, including `long long` and `long double`, is a single word on BESM-6.
- "Fat pointers" are used for char* and void*, with a sub-word byte offset in MSB.
- Conventionally `sizeof(char) == 1` and `sizeof(type) == 6` for every type that occupies one word.

The BESM-6 has its own 48-bit floating-point format (7-bit base-2 exponent, 40-bit
two's-complement mantissa). `float` and `double` both map to this native format.
There is no IEEE 754 hardware.

| Type          | Size | Alignment | Bits used | Notes |
|---------------|------|-----------|-----------|-------|
| `bool`        | 1w   | 1w        | 1         | Lower bit; upper 47 bits zero |
| `char`        | 1w   | 1w        | 8         | Fat pointers |
| `short`       | 1w   | 1w        | 48        | Same as `int` |
| `int`         | 1w   | 1w        | 48        | Full word |
| `long`        | 1w   | 1w        | 48        | Same as `int` |
| `long long`   | 1w   | 1w        | 48        | Same as `long` |
| `float`       | 1w   | 1w        | 48        | BESM-6 native FP format |
| `double`      | 1w   | 1w        | 48        | Same as `float` (no wider FP hardware) |
| `long double` | 1w   | 1w        | 48        | Same as `double` (no wider FP hardware) |
| pointer       | 1w   | 1w        | 15        | Word address in lower 15 bits |

On BESM-6, a `char` variable wastes 40 bits per allocation.
The packed representation (6 chars per word, using a sub-word byte offset baked into the pointer)
trades memory efficiency for pointer complexity; that scheme requires "fat pointers"
carrying both a word address and a 3-bit intra-word offset.

---

## 4. Word-Oriented vs. Byte-Addressed Machines

### Byte-addressed machines (AVR, MSP430, ARM32, AArch64, x86_64, RISC-V 32, RISC-V 64, MMIX)

On a byte-addressed machine every byte has a unique address. A 4-byte `int` stored at
address 100 occupies bytes 100, 101, 102, 103. The next `int` in an array starts at
address 104.

Pointer arithmetic follows `sizeof`: incrementing a `T*` adds `sizeof(T)` to the
numeric address. For example:

```c
int  *p = (int *)100;  p++;  // p is now 104
char *q = (char *)100; q++;  // q is now 101
```

Because each byte is individually addressable, the compiler can read or write any single
byte with a single load/store instruction. Sub-byte types (bit-fields) require
read-modify-write, but sub-word types like `char` and `short` are efficiently supported.

### Word-addressed machines (BESM-6)

On the BESM-6, the address space is a flat array of 48-bit words. An address is a
*word index*, not a byte index. There is no instruction to load or store a single byte
or a sub-word portion of memory directly. Reading less than one full word requires
loading the containing word and then masking/shifting the desired bits in software.

The C standard defines `sizeof(char) == 1` to mean one addressable unit, and requires
that `char` is at least 8 bits. On BESM-6 the addressable unit is a 48-bit word, though
`CHAR_BIT == 8` and a `char` object occupies one full word.
All of the following are true simultaneously:

```
sizeof(char)   == 1
sizeof(short)  == 6
sizeof(int)    == 6
sizeof(long)   == 6
sizeof(float)  == 6
sizeof(double) == 6
sizeof(void*)  == 6
```

Pointer arithmetic works differently for regular pointers (word address) and fat pointers
(word address and intra-word offset).
Incrementing a regular `T*` adds 1 to the numeric address, so
a pointer increment advances by exactly 1 word:

```c
int  *p = (int *)100;   // p is 100
p++;                    // p is 101  (one word forward)
```

Incrementing a fat `char*` increases the intra-word offset, and occasionally adds 1 to
the word address, so a pointer increment advances by exactly 1 word:

```c
char *q = (char *)100;  // q is 100 + ((64 + 40) << 41)  (fat pointer)
q++;                    // q is 100 + ((64 + 32) << 41)  (one byte forward)
```

A single `char` variable on BESM-6 occupies full word, and wastes 40 of its 48 bits.

### Packed character arrays on BESM-6

The BESM-6 hardware can pack six 8-bit characters per 48-bit word. An array of `char`
represents a string as a sequence of words containing packed chars.
For the pointer `char *p` to address individual characters within a packed
word, it needs more information than a plain 15-bit word address.
The compiler uses a **fat pointers** approach: a `char *` is a pair `(word_addr, bit_offset)` where
`bit_offset` in the most significant bits of the pointer selects the character within the word.

String operations (`memcpy`, `strlen`, etc.) implemented for BESM-6 work on fat pointers.

### Synthesizing sub-word access

When the compiler must extract or insert a value narrower than a word (e.g., to
implement a `short` bit-field or to sign-extend a 32-bit `int` loaded from a word),
it emits bit-shift and mask instructions:

- **Read word** by bits [15:1] of the pointer:
  ```
  WTC ptr          ; use word address in lower 15 bits
  XTA              ; get word
  ```
- **Select** required byte by a sub-word bit offset:
  ```
  ASX ptr          ; shift right by offset in MSB
  AAX =0377        ; mask the required byte
  ```

---

## 5. Summary Comparison

The table below compares sizes across all architectures. Byte-addressed architectures
show sizes in bytes; BESM-6 shows sizes in words (1 word = 48 bits = 6 bytes).

| Type          | AVR | MSP430 | ARM32 | RV32 | AArch64 | x86_64 | RV64 | MMIX | BESM-6 |
|---------------|-----|--------|-------|------|---------|--------|------|------|--------|
| `bool`        | 1   | 1      | 1     | 1    | 1       | 1      | 1    | 1    | 1 word |
| `char`        | 1   | 1      | 1     | 1    | 1       | 1      | 1    | 1    | 1 word |
| `short`       | 2   | 2      | 2     | 2    | 2       | 2      | 2    | 2    | 1 word |
| `int`         | 2   | 2      | 4     | 4    | 4       | 4      | 4    | 4    | 1 word |
| `long`        | 4   | 4      | 4     | 4    | 8       | 8      | 8    | 8    | 1 word |
| `long long`   | 8   | 8      | 8     | 8    | 8       | 8      | 8    | 8    | 1 word |
| `float`       | 4   | 4      | 4     | 4    | 4       | 4      | 4    | 4    | 1 word |
| `double`      | 4   | 8      | 8     | 8    | 8       | 8      | 8    | 8    | 1 word |
| `long double` | 4   | 8      | 8     | 16   | 16      | 16     | 16   | 8    | 1 word |
| pointer       | 2   | 2      | 4     | 4    | 8       | 8      | 8    | 8    | 1 word |

And alignment values (same units as sizes):

| Type          | AVR | MSP430 | ARM32 | RV32 | AArch64 | x86_64 | RV64 | MMIX | BESM-6 |
|---------------|-----|--------|-------|------|---------|--------|------|------|--------|
| `bool`        | 1   | 1      | 1     | 1    | 1       | 1      | 1    | 1    | 1 word |
| `char`        | 1   | 1      | 1     | 1    | 1       | 1      | 1    | 1    | 1 word |
| `short`       | 1   | 2      | 2     | 2    | 2       | 2      | 2    | 2    | 1 word |
| `int`         | 1   | 2      | 4     | 4    | 4       | 4      | 4    | 4    | 1 word |
| `long`        | 1   | 2      | 4     | 4    | 8       | 8      | 8    | 8    | 1 word |
| `long long`   | 1   | 2      | 8     | 8    | 8       | 8      | 8    | 8    | 1 word |
| `float`       | 1   | 2      | 4     | 4    | 4       | 4      | 4    | 4    | 1 word |
| `double`      | 1   | 2      | 8     | 8    | 8       | 8      | 8    | 8    | 1 word |
| `long double` | 1   | 2      | 8     | 16   | 16      | 16     | 16   | 8    | 1 word |
| pointer       | 1   | 2      | 4     | 4    | 8       | 8      | 8    | 8    | 1 word |

Notable observations:

- **AVR** aligns everything to 1 byte — no alignment requirements at all.
- **MSP430** aligns everything to 2 bytes once the type is 2 bytes or wider.
- **ARM32 and RISC-V 32** share the ILP32 model (`long` = 4) with identical type sizes.
- **AArch64, x86_64, RISC-V 64, and MMIX** all use LP64 integers (`long` = pointer = 8).
- **x86_64** `long double` is a 10-byte (80-bit) value stored in a 16-byte slot.
- **AArch64 and RISC-V 64** `long double` is a true 16-byte (128-bit) IEEE 754 quad-precision value.
- **MMIX** maps `long double` to 8 bytes (same as `double`) despite being a 64-bit machine,
  matching ARM32 — the FPU is 64-bit only.
- **BESM-6** collapses every scalar type — `char`, `short`, `int`, `long`, `long long`,
  `float`, `double`, `long double`, and pointers — to the same size (1 word). No scalar
  type is two words.

---

## 6. Implications for This Compiler

The current implementation in `semantic/type_utils.c` hard-codes sizes and alignments
matching the x86_64 / LP64 model (bool=1, short=2, int=4, long/long long/pointer=8,
long double=16). This is correct for x86_64 and AArch64 and is used throughout the
TAC lowering stage.

When adding backends for other architectures, the following changes will be required:

1. **Target descriptor**: Introduce a `TargetABI` struct containing arrays of sizes and
   alignments keyed by `TypeKind`. Pass it through the compilation pipeline.

2. **`get_size()` / `get_alignment()`** in `semantic/type_utils.c`: Replace the
   hard-coded switch with a lookup in the active `TargetABI`.

3. **TAC layer**: TAC currently stores sizes as `size_t` byte counts derived from the
   host-model values. For BESM-6, "bytes" become "words"; the TAC consumer (the backend)
   must interpret them in the correct unit. Byte arrays are packed as 6 bytes per word.

4. **BESM-6 backend specifics**:
   - `char` and `short` loads must emit mask/shift sequences to extract the narrow value
     from its containing word.
   - Regular pointer comparisons and arithmetic work identically to int arithmetic (both are
     one-word quantities).
   - Fat pointer comparisons and arithmetic use special implementation.
   - `sizeof` works as usual: 1 for `char` and 6 for word-sized types.
   - String and memory-copy operations should process one word (6 logical bytes) per
     iteration for efficiency when packed strings are used.
