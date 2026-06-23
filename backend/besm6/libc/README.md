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
| `printf(fmt, ...)` | [printf.c](printf.c) | Full-featured formatted output to stdout (see below) |
| `sprintf(buf, fmt, ...)` | [sprintf.c](sprintf.c) | Formatted output into an unbounded caller buffer; returns the length |
| `snprintf(buf, size, fmt, ...)` | [snprintf.c](snprintf.c) | Bounded variant: writes at most `size-1` chars plus NUL; returns the would-be length |
| `__doprnt(...)` | [doprnt.c](doprnt.c) | Shared formatting engine behind printf/sprintf/snprintf (not called directly) |
| `b/tout(buf)` | [b_tout.madlen](b_tout.madlen) | Low-level: write a line buffer directly to stdout via extracode `*71` |

### `printf` family

Conversions: `%d` `%i` `%u` `%o` `%x` `%X` `%c` `%s` `%p` `%f` `%e` `%g` `%%`, with the
flags `-` `0` `+` space `#`, a field width (including `*`) and a precision (including `.*`).
The length modifiers `l` / `h` are accepted and ignored (`long` == `int`, `short` == `int`).

BESM-6 specifics:

- **Output folds to upper case.**  The stdout buffer is KOI7, which folds letters onto their
  upper-case codes.  Consequently all letters print upper case, and `%x`/`%X`, `%e`/`%E`,
  `%g`/`%G` produce identical output.
- **Floating point** uses the native 48-bit format (~12 significant decimal digits); helpers
  live in [modf.c](modf.c) (`modf`).  Very small `%g` values follow the simplified
  kernel-printf algorithm.
- **No string literals in the library.**  Per-module string constants share the `_strN`
  namespace, so a literal inside a runtime routine would collide with the caller's own string
  constants at link time; routines build any fixed text (e.g. `(NULL)`) in a local buffer.

### `<string.h>` (C11 §7.24)

All routines treat `char*` / `void*` as fat byte cursors, so they cross word
boundaries transparently.

| Function | Source | Description |
|----------|--------|-------------|
| `strlen(s)` | [strlen.c](strlen.c) | Length of the NUL-terminated string `s` |
| `strcpy(dest, src)` | [strcpy.c](strcpy.c) | Copy `src` (incl. NUL) into `dest` |
| `strncpy(dest, src, n)` | [strncpy.c](strncpy.c) | Copy ≤ `n` bytes; pad with NUL if `src` is shorter |
| `strcat(dest, src)` | [strcat.c](strcat.c) | Append `src` to `dest` |
| `strncat(dest, src, n)` | [strncat.c](strncat.c) | Append ≤ `n` bytes of `src`; always NUL-terminates |
| `strcmp(s1, s2)` | [strcmp.c](strcmp.c) | Compare strings (unsigned char), `<0`/`0`/`>0` |
| `strncmp(s1, s2, n)` | [strncmp.c](strncmp.c) | Compare ≤ `n` bytes |
| `strchr(s, c)` | [strchr.c](strchr.c) | First occurrence of `c` (incl. the NUL when `c==0`) |
| `strrchr(s, c)` | [strrchr.c](strrchr.c) | Last occurrence of `c` |
| `strstr(hay, needle)` | [strstr.c](strstr.c) | First occurrence of substring `needle` |
| `strtok(str, delim)` | [strtok.c](strtok.c) | Tokenize on `delim`; non-reentrant (file-scope state) |
| `strerror(errnum)` | [strerror.c](strerror.c) | Static message for an `<errno.h>` code (UPPERCASE) |
| `memcpy(dest, src, n)` | [memcpy.c](memcpy.c) | Copy `n` bytes; regions must not overlap |
| `memmove(dest, src, n)` | [memmove.c](memmove.c) | Copy `n` bytes, overlap-safe |
| `memset(s, c, n)` | [memset.c](memset.c) | Fill `n` bytes with `c` |
| `memcmp(s1, s2, n)` | [memcmp.c](memcmp.c) | Compare `n` bytes (unsigned char) |
| `memchr(s, c, n)` | [memchr.c](memchr.c) | First `c` within the first `n` bytes |

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
