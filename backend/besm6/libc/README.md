# C Runtime Library for BESM-6

Runtime library for C programs targeting the BESM-6 mainframe.

For basing, use `r14`. No other functions must be called, and no extracodes.

The higher-level routines are written in C and compiled by this project's own
toolchain (`parse → lower → genbesm`) into Madlen; the low-level helpers remain
hand-written Madlen.  The original B-language sources (`*.b`) are kept for
historical reference but are no longer built.

## I/O Functions

| Function | Source | Description |
|----------|--------|-------------|
| `getch()` | [getch.c](getch.c) | Read one character from stdin; returns `0` at EOF |
| `putch(ch)` | [putch.c](putch.c) | Append a multi-char word to the output buffer (skips leading zero bytes) |
| `putbyte(b)` | [putbyte.c](putbyte.c) | Append one byte to the output buffer; flushes on newline or full buffer |
| `flush()` | [flush.c](flush.c) | Flush the output line buffer to stdout (or drum when `fout` is set) |
| `printf(fmt, ...)` | [printf.c](printf.c) | Formatted output: `%d` decimal, `%o` octal, `%c` character, `%s` string |
| `print_d(n)` | [print_d.c](print_d.c) | Print a signed decimal integer |
| `print_o(n)` | [print_o.c](print_o.c) | Print an octal integer |
| `b/tout(buf)` | [b_tout.madlen](b_tout.madlen) | Low-level: write a line buffer directly to stdout via extracode `*71` |

## String / Character Functions

| Function | Source | Description |
|----------|--------|-------------|
| `char(str, i)` | [char.madlen](char.madlen) | Return byte `i` of string `str` (left-to-right, 0-based) |
| `lchar(str, i, ch)` | [lchar.madlen](lchar.madlen) | Set byte `i` of string `str` to `ch`; return `ch` |

## Program Control

| Function | Source | Description |
|----------|--------|-------------|
| `exit()` | [exit.madlen](exit.madlen) | Terminate the program (extracode `*74`) |

## Compiler-Support Routines

These are internal helpers emitted by the compiler; they are not called directly from C source.

### Calling Convention

| Routine | Source | Description |
|---------|--------|-------------|
| `b/save` | [b_save.madlen](b_save.madlen) | Save registers on function entry (1+ parameters) |
| `b/save0` | [b_save0.madlen](b_save0.madlen) | Save registers on function entry (0 parameters) |
| `b/ret` | [b_ret.madlen](b_ret.madlen) | Restore registers and return from a C function |
| `b/true` | [b_true.madlen](b_true.madlen) | Constant `1` (logical true value) |

### Arithmetic Operators

| Routine | Source | C operator | Description |
|---------|--------|-----------|-------------|
| `b/mul` | [b_mul.madlen](b_mul.madlen) | `*` | Integer multiply |
| `b/div` | [b_div.madlen](b_div.madlen) | `/` | Integer divide |
| `b/mod` | [b_mod.madlen](b_mod.madlen) | `%` | Integer modulo |

### Relational and Logical Operators

| Routine | Source | C operator | Description |
|---------|--------|-----------|-------------|
| `b/not` | [b_not.madlen](b_not.madlen) | `!` | Logical NOT |
| `b/eq` | [b_eq.madlen](b_eq.madlen) | `==` | Equal |
| `b/ne` | [b_ne.madlen](b_ne.madlen) | `!=` | Not equal |
| `b/lt` | [b_lt.madlen](b_lt.madlen) | `<` | Less than |
| `b/le` | [b_le.madlen](b_le.madlen) | `<=` | Less than or equal |
| `b/gt` | [b_gt.madlen](b_gt.madlen) | `>` | Greater than |
| `b/ge` | [b_ge.madlen](b_ge.madlen) | `>=` | Greater than or equal |

The integer orderings above subtract the operands as raw words. For floating-point operands
the four orderings use the FP variants below, which bracket the subtract with `NTR` (full FP
mode) so the additive sign reflects the mathematical difference. Floating-point `==` / `!=`
are pure bit equality, so they reuse `b/eq` / `b/ne`.

| Routine | Source | C operator | Description |
|---------|--------|-----------|-------------|
| `b/flt` | [b_flt.madlen](b_flt.madlen) | `<` | Less than (floating-point) |
| `b/fle` | [b_fle.madlen](b_fle.madlen) | `<=` | Less than or equal (floating-point) |
| `b/fgt` | [b_fgt.madlen](b_fgt.madlen) | `>` | Greater than (floating-point) |
| `b/fge` | [b_fge.madlen](b_fge.madlen) | `>=` | Greater than or equal (floating-point) |
