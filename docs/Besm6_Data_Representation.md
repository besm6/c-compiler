# Data Representation on BESM-6

This article describes how every basic C scalar type — integers, floating-point numbers,
characters, booleans, and pointers — is stored in memory on the BESM-6. The intended
audience is a programmer who knows C and binary arithmetic but has not worked with a
word-oriented machine before.

---

## 1. Hardware Background

The BESM-6 is a Soviet mainframe computer developed in the 1960s. Its memory model
differs fundamentally from every modern platform:

- **The addressable unit is one 48-bit word.** There are no byte-level load or store
  instructions. Reading less than one full word requires loading the containing word and
  then isolating the desired bits with shift and mask operations in software.
- **The address space is 32,768 words** (2¹⁵). An address is a word index, not a byte
  offset. One address increment steps by 48 bits, not by 8.
- **`CHAR_BIT` is 8.** A `char` holds 8 bits of data. Six chars fit in one 48-bit word.
  From a C programmer's perspective: `sizeof(char) == 1` and `sizeof(int) == 6`
  (six char-units — one 48-bit word).
- **No IEEE 754.** The BESM-6 has its own native floating-point format, described below.

### Bit-numbering convention

Throughout this document and in BESM-6 hardware documentation, bits are numbered
**right-to-left starting from 1**. Bit 1 is the least-significant bit (LSB); bit 48 is
the most-significant bit (MSB).

```
      MSB                                         LSB
Bit:  48  47  46  45  44  43  42  41  40 ...   2   1
     ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
     │   │   │   │   │   │   │   │   │   │   │   │   │
     └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘
```

---

## 2. The Universal 48-bit Word Layout

The BESM-6 hardware arithmetic unit defines one canonical layout for a 48-bit word:
a 7-bit exponent field, a 1-bit sign, and a 40-bit two's-complement mantissa:

```
Bit:  48     42  41 40                       1
     ┌─────────┬───┬──────────────────────────┐
     │Exponent │ S │ Mantissa (2's complement)│
     └─────────┴───┴──────────────────────────┘
```

This format is shared — with variations — by both floating-point values and integers:

| Type class | Bits 48–42 | Bit 41 | Bits 40–1 |
|------------|-----------|--------|-----------|
| Integer    | Zero      | Sign   | Two's complement value |
| Float      | Exponent (biased 64) | Sign | Two's complement mantissa |

For integers, bits 48–42 are always zero and carry no meaning. For floats, they hold a
biased binary exponent. Both share the same sign-bit position and the same two's-complement
encoding in bits 40–1.

> **Historical note.** Other BESM-6 compilers (Fortran, Algol, Pascal, Madlen) store
> integers with the exponent field set to 40+64 = 104, so that a single normalization
> instruction converts an integer to a valid floating-point number. This C compiler leaves
> bits 48–42 at zero, which simplifies bitwise operations and shifts: the integer is just a
> raw two's-complement number without any embedded exponent to maintain.

---

## 3. Boolean (`bool`)

A boolean value occupies one 48-bit word. Only bit 1 carries the value; bits 2–48 are
always zero.

```
Bit:  48                                       2  1
     ┌──────────────────────────────────────────┬───┐
     │                  Zero                    │ V │
     └──────────────────────────────────────────┴───┘
```

`false` is stored as all-zero; `true` has bit 1 set. The representation is consistent
with integer arithmetic: a boolean `true` compares equal to the integer 1.

---

## 4. Character Types

### Plain `char` and `unsigned char`

On BESM-6, plain `char` is unsigned by default. A standalone `char` variable occupies one
full word, using only the 8 least-significant bits (byte #5 in big-endian word order).
Bits 48–9 are zero.

```
Bit:  48                               9 8          1
     ┌──────────────────────────────────┬────────────┐
     │              Zero                │ char value │
     └──────────────────────────────────┴────────────┘
```

Range: 0 to 255.

The byte lives in position "byte #5" because the BESM-6 uses big-endian byte ordering
within a word — the most significant byte is byte #0, the least significant is byte #5.

### `signed char`

`signed char` uses the same one-word storage. The hardware interprets the 8-bit field
as a two's-complement value. Range: −128 to 127.

### Arrays of `char`

Six 8-bit characters pack into one 48-bit word, arranged left-to-right (big-endian):

```
Bit:  48   41 40   33 32   25 24   17 16    9 8     1
     ┌───────┬───────┬───────┬───────┬───────┬───────┐
     │byte #0│byte #1│byte #2│byte #3│byte #4│byte #5│
     └───────┴───────┴───────┴───────┴───────┴───────┘
```

An array of N characters occupies ⌈N/6⌉ words. The packed layout is six times more
memory-efficient than storing each character in its own word, but it requires
**fat pointers** (described in Section 8) to address individual bytes.

---

## 5. Integer Types

### `short`, `int`, and `long`

All three types map to the same 48-bit word representation. The hardware has no 16-bit
or 32-bit integer format; the short and long keywords are aliases for `int` on BESM-6.

```
Bit:  48     42  41 40                       1
     ┌─────────┬───┬──────────────────────────┐
     │  Zero   │ S │  Value (2's complement)  │
     └─────────┴───┴──────────────────────────┘
```

- **Bits 48–42** (7 bits): Always zero.
- **Bit 41**: Sign. 0 = positive or zero, 1 = negative.
- **Bits 40–1** (40 bits): Two's complement magnitude.

The sign bit and the 40-bit magnitude together form a 41-bit two's-complement integer.

| Constant | Value |
|----------|-------|
| `INT_MIN` | −2⁴⁰ = −1,099,511,627,776 |
| `INT_MAX` | 2⁴⁰ − 1 = 1,099,511,627,775 |

`sizeof(short) == sizeof(int) == sizeof(long) == 6` (six char-units, one 48-bit word).

### `unsigned`, `unsigned short`, and `unsigned long`

Unsigned variants use all 48 bits as a non-negative integer. There is no reserved sign
bit; the full word represents a value from 0 to 2⁴⁸ − 1.

| Constant | Value |
|----------|-------|
| `UINT_MAX` | 2⁴⁸ − 1 = 281,474,976,710,655 |

### Integer constants and literal suffixes

The width rules above apply to integer *constants* as well. At code emission a signed
`int`/`long` constant is masked to its 41 value bits, while an unsigned constant keeps all
48 bits (see `const_lit_name` in `backend/besm6/codegen.c`). Two consequences:

- **A signed literal that needs more than 41 bits silently loses its top 7 bits.** For
  example `0xFFFFFFFFFFFF` (a signed value of 2⁴⁸ − 1) is masked to its low 41 bits. This is
  expected: signed `int`/`long` *is* a 41-bit type on BESM-6, so the constant simply does not
  fit. To keep a value wider than 41 bits, give it an unsigned type.

- **A `U` suffix alone is enough to get a full 48-bit unsigned constant — the `L` is not
  required.** Writing `0xFFFFFFFFFFFFU` yields the full 48-bit value; you do not need to add
  `L` (`0xFFFFFFFFFFFFUL`). A wide unsigned literal automatically takes whichever unsigned
  type is wide enough to hold all 48 bits, so the value reaches code generation intact and is
  emitted as the full 16-octal-digit word.

### `long long`

`long long` occupies two consecutive words and provides 81 bits of two's-complement
precision. Software routines implement all arithmetic.

**Word 0 (high word):**
```
Bit:  48     42  41 40                       1
     ┌─────────┬───┬──────────────────────────┐
     │  Zero   │ S │   High 40 value bits     │
     └─────────┴───┴──────────────────────────┘
```
- Bits 48–42: Zero.
- Bit 41: Sign of the entire 80-bit value (0 = positive, 1 = negative).
- Bits 40–1: Upper 40 bits of the two's-complement representation.

**Word 1 (low word):**
```
Bit:  48     42  41 40                       1
     ┌─────────┬───┬──────────────────────────┐
     │  Zero   │   │   Low 40 value bits      │
     └─────────┴───┴──────────────────────────┘
```
- Bits 48–41: Unused (ignored by arithmetic routines).
- Bits 40–1: Lower 40 bits of the two's-complement representation.

The 81 data bits (1 sign + 40 high + 40 low) give the range −2⁸⁰ to 2⁸⁰ − 1.

`sizeof(long long) == 12` (twelve char-units, two words).

### `unsigned long long`

Same two-word layout as `long long`, but all 96 bits are used as a non-negative integer.
Range: 0 to 2⁹⁶ − 1.

---

## 6. Floating-Point Types

### `float` and `double` — the BESM-6 native format

Both `float` and `double` map to the same 48-bit native floating-point format. The
BESM-6 hardware provides no IEEE 754 arithmetic; there is no way to distinguish a 32-bit
float from a 64-bit double at the hardware level.

```
Bit:  48     42  41 40                       1
     ┌─────────┬───┬──────────────────────────┐
     │Exponent │ S │ Mantissa (2's complement)│
     └─────────┴───┴──────────────────────────┘
```

- **Bits 48–42** (7 bits): Binary exponent, biased by 64. A stored exponent value of 64
  represents 2⁰; value 65 represents 2¹; value 1 represents 2⁻⁶³; value 127 represents 2⁶³.
- **Bit 41**: Sign. 0 = positive or zero, 1 = negative.
- **Bits 40–1** (40 bits): Mantissa in two's complement, representing the fractional value
  `0.b₄₀b₃₉…b₁` in binary.

**Value formula:**

```
value = (0.mantissa − sign) × 2^(exponent − 64)
```

For a positive number (`sign = 0`): `value = 0.mantissa × 2^(exponent − 64)`, where the
mantissa fraction lies in [0, 1).

For a negative number (`sign = 1`): `value = (0.mantissa − 1) × 2^(exponent − 64)`, where
`(0.mantissa − 1)` is the two's-complement negative fraction in [−1, 0).

**Normalized form.** A normalized number has bits 41 and 40 different: the top bit of the
mantissa disagrees with the sign. Consequently:
- A normalized positive number has bit 40 = 1: mantissa ∈ [0.5, 1.0).
- A normalized negative number has bit 40 = 0: effective fractional value ∈ [−1.0, −0.5).

This gives approximately 12 significant decimal digits of precision (40 bits × log₁₀ 2 ≈ 12.04).

**Comparison with IEEE 754:**

| Property | BESM-6 | IEEE binary32 | IEEE binary64 |
|----------|--------|---------------|---------------|
| Total bits | 48 | 32 | 64 |
| Exponent bits | 7 (bias 64) | 8 (bias 127) | 11 (bias 1023) |
| Mantissa bits | 40 | 23 | 52 |
| Sign convention | Two's complement | Sign-magnitude | Sign-magnitude |
| Decimal digits | ~12 | ~7 | ~15 |
| NaN / Infinity | No | Yes | Yes |
| Denormals | No | Yes | Yes |
| Exponent range | 2⁻⁶³ to 2⁶³ | 2⁻¹²⁶ to 2¹²⁷ | 2⁻¹⁰²² to 2¹⁰²³ |

The absence of NaN, infinity, and denormals means that overflow, division by zero, and
underflow do not produce the special values familiar from IEEE 754; instead they typically
produce incorrect results silently, or the hardware raises a fault condition.

`sizeof(float) == sizeof(double) == 6`.

### `long double`

`long double` occupies two consecutive words, extending the 40-bit mantissa with an
additional 40 bits. Also 7-bit exponent is extended with an additional 7 bits.
Software routines implement all arithmetic, using the hardware Y (RMR — younger bits)
register convention for intermediate double-width results.

**Word 0 (high word):**
```
Bit:  48     42  41 40                       1
     ┌─────────┬───┬──────────────────────────┐
     │Exp. MSB │ S │   Mantissa, bits 80–41   │
     └─────────┴───┴──────────────────────────┘
```
- Bits 48–42: Upper 7 bits of exponent.
- Bit 41: Sign.
- Bits 40–1: Upper 40 bits of the mantissa.

**Word 1 (low word):**
```
Bit:  48     42  41 40                       1
     ┌─────────┬───┬──────────────────────────┐
     │Exp. LSB │   │   Mantissa, bits 40–1    │
     └─────────┴───┴──────────────────────────┘
```
- Bits 48–42: Lower 7 bits of exponent.
- Bit 41: Unused (ignored).
- Bits 40–1: Lower 40 bits of the mantissa.

The combined 80-bit mantissa gives approximately 24 significant decimal digits
(80 × log₁₀ 2 ≈ 24.08).

The combined 14-bit exponent is biased by 8192.

`sizeof(long double) == 12`.

---

## 7. Pointers

### Regular pointer

A pointer to any word-sized type (`int*`, `double*`, `long*`, `struct Foo*`, etc.)
is a plain 48-bit word with the 15-bit word address in the lower bits and the upper
bits set to zero.

```
Bit:  48                          16 15             1
     ┌──────────────────────────────┬────────────────┐
     │          Zero                │  Word address  │
     └──────────────────────────────┴────────────────┘
```

Pointer arithmetic on a single-word type (`int*`, `float*`, etc.) increments the address
by 1 (one word). Pointer arithmetic on two-word types (`long long*`, `long double*`)
increments by 2.

Regular pointers can hold 2¹⁵ = 32,768 distinct addresses, spanning the entire BESM-6
address space.

### Fat pointer (`char*`, `void*`)

Characters in an array are packed six per word. To address an individual character, a
pointer must carry both a word address and a sub-word byte offset. This combination is
called a **fat pointer**.

```
Bit:  48 47  45 44  42            16 15             1
     ┌──┬──────┬──────┬─────────────┬────────────────┐
     │ 1│Offset│  0   │    Zero     │  Word address  │
     └──┴──────┴──────┴─────────────┴────────────────┘
```

- **Bit 48**: Always 1. This distinguishes a fat pointer from a regular pointer (where
  bits 48–16 are zero).
- **Bits 47–45** (3 bits): Byte offset within the word. Encodes which of the six bytes
  the pointer addresses, as a shift distance in bytes:

  | Field value (bits 47–45) | Shift (bytes) | Byte position |
  |--------------------------|---------------|----------------|
  | 5 (binary 101) | 5 | Byte #0 — MSB (bits 48–41) |
  | 4 (binary 100) | 4 | Byte #1 (bits 40–33) |
  | 3 (binary 011) | 3 | Byte #2 (bits 32–25) |
  | 2 (binary 010) | 2 | Byte #3 (bits 24–17) |
  | 1 (binary 001) | 1 | Byte #4 (bits 16–9) |
  | 0 (binary 000) | 0 | Byte #5 — LSB (bits 8–1) |

- **Bits 44–42**: Always zero.
- **Bits 41–16**: Always zero.
- **Bits 15–1**: 15-bit word address.

**Why this encoding?** The BESM-6 `ASX` instruction shifts the accumulator right by the
number of positions given in the exponent field (bits 48–42) of its memory operand, minus
64. With the fat pointer layout, that field equals `64 + offset × 8`, giving a shift of
`offset × 8` bits — exactly what is needed to move the target byte into the lowest 8 bits
of the accumulator. Byte extraction thus compiles to four instructions:

```
    WTC ptr        ; load word address from lower 15 bits of pointer into M register
    XTA            ; A = mem[M]   (load the containing word)
    ASX ptr        ; A >>= offset × 8  (right-shift by amount encoded in fat pointer)
    AAX =0377      ; A &= 0xFF         (mask to lowest 8 bits)
```

#### Address of a standalone `char`

A standalone `char` variable always occupies byte #5 (the least significant byte) of
its word. Taking its address produces a fat pointer with offset = 0:

```
Bit:  48 47  45 44  42            16 15             1
     ┌──┬──────┬──────┬─────────────┬────────────────┐
     │ 1│0 0 0 │  0   │    Zero     │  Word address  │
     └──┴──────┴──────┴─────────────┴────────────────┘
```

#### Casting `int*` to `char*`

The C standard requires that casting an integer pointer to `char*` produces a pointer to
the first byte of the integer's representation. On BESM-6 the first (most significant)
byte of a word is byte #0, so the conversion sets offset = 5:

```
Bit:  48 47  45 44  42            16 15             1
     ┌──┬──────┬──────┬─────────────┬────────────────┐
     │ 1│1 0 1 │  0   │    Zero     │  Word address  │
     └──┴──────┴──────┴─────────────┴────────────────┘
```

#### Casting `char*` to `int*`

Clearing the fat marker and offset converts a fat pointer back to a regular word pointer.
The byte offset is discarded; the resulting `int*` addresses the word that contains the
byte the original `char*` was pointing to.

#### Casting between `char*` and `void*`

The C standard requires that `char*` and `void*` have identical representations. The
compiler leaves the bit pattern unchanged on conversions between these two types.

#### Incrementing a `char*`

Incrementing a fat pointer decreases the offset by 1 (moving from a more significant
byte to a less significant one). When the offset wraps from 0 to 5 — that is, when
stepping past byte #5 — the word address is simultaneously incremented by 1, advancing
to the next word.

---

## 8. Type Summary

| Type | Size | Alignment | Bits used | Notes |
|------|------|-----------|-----------|-------|
| `bool` | 1w | 1w | 1 | Lower bit; upper 47 bits zero |
| `char` | 1w | 1w | 8 | Unsigned by default; fat pointers for `char*` |
| `signed char` | 1w | 1w | 8 | Two's complement; range −128…127 |
| `unsigned char` | 1w | 1w | 8 | Range 0…255 |
| `short` | 1w | 1w | 41 | Same as `int` |
| `int` | 1w | 1w | 41 | 1 sign bit + 40 value bits |
| `long` | 1w | 1w | 41 | Same as `int` |
| `unsigned short` | 1w | 1w | 48 | Same as `unsigned int` |
| `unsigned int` | 1w | 1w | 48 | Full 48-bit unsigned |
| `unsigned long` | 1w | 1w | 48 | Same as `unsigned int` |
| `long long` | 2w | 2w | 80 | Sign + 40 high + 40 low bits; software arithmetic |
| `unsigned long long` | 2w | 2w | 80 | Software arithmetic |
| `float` | 1w | 1w | 48 | BESM-6 native FP; same as `double` |
| `double` | 1w | 1w | 48 | BESM-6 native FP; ~12 decimal digits |
| `long double` | 2w | 2w | 80+8 | 80-bit mantissa; ~24 decimal digits; software arithmetic |
| pointer | 1w | 1w | 15 | Word address in bits 15–1 |
| `char*`, `void*` | 1w | 1w | 3+15 | Fat pointer: 3-bit offset in bits 47–45 |

Sizes and alignments in words (1 word = 48 bits). `sizeof` values in char-units:
`sizeof(char) == 1`, single-word types have `sizeof == 6`, two-word types have `sizeof == 12`.

---

## 9. C Programmer's Reference

### `limits.h`

| Macro | Value | Notes |
|-------|-------|-------|
| `CHAR_BIT` | 8 | Bits per char |
| `SCHAR_MIN` | −128 | |
| `SCHAR_MAX` | 127 | |
| `UCHAR_MAX` | 255 | |
| `SHRT_MIN` | −2⁴⁰ | Same as `INT_MIN` |
| `SHRT_MAX` | 2⁴⁰ − 1 | Same as `INT_MAX` |
| `INT_MIN` | −1,099,511,627,776 | −2⁴⁰ |
| `INT_MAX` | 1,099,511,627,775 | 2⁴⁰ − 1 |
| `UINT_MAX` | 281,474,976,710,655 | 2⁴⁸ − 1 |
| `LLONG_MIN` | −2⁸⁰ | Two-word signed |
| `LLONG_MAX` | 2⁸⁰ − 1 | Two-word signed |
| `ULLONG_MAX` | 2⁹⁶ − 1 | Two-word unsigned |

For how integer **constants** are sized and why a `U` suffix alone keeps all 48 bits, see
[Integer constants and literal suffixes](#integer-constants-and-literal-suffixes) in Section 5.

### `sizeof` in char-units

```c
sizeof(char)        == 1
sizeof(short)       == 6
sizeof(int)         == 6
sizeof(long)        == 6
sizeof(long long)   == 12
sizeof(float)       == 6
sizeof(double)      == 6
sizeof(long double) == 12
sizeof(void *)      == 6
sizeof(char *)      == 6   /* fat pointer fits in one word */
```

Every type is aligned to its own size. There is no struct padding for single-word members
since all of them have the same size and alignment.

### Pointer size and address space

`sizeof(void *) == 6` (one word). The 15-bit word address limits the usable address space
to 32,768 words = 32,768 × 6 logical bytes = 196,608 bytes.

String and memory operations (`memcpy`, `strlen`, etc.) work on fat pointers and process
up to six logical bytes per word in the common case.
