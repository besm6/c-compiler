# BESM-6 standard library headers

C11 standard-library headers for programs compiled with this toolchain for the
BESM-6 target.  Types and macros match the BESM-6 data model
([docs/Besm6_Data_Representation.md](../../../docs/Besm6_Data_Representation.md)).

For the full narrative reference — the role of each header, the functions it
declares, how the headers relate, and the target specifics — see
[docs/Standard_Include_Files.md](../../../docs/Standard_Include_Files.md).

## Usage — the compiler has no preprocessor

The `parse` front end consumes already-preprocessed C (it understands `#` line
markers only, not `#include`/`#define`, and has no `-I` flag).  Run an external
preprocessor first, then feed the result to `parse`:

```sh
cc -E -nostdinc -I libc/besm6/include prog.c prog.i
./build/parse prog.i prog.ast
./build/lower  prog.ast prog.tac
./build/genbesm prog.tac prog.mad
```

Use the compiler's preprocessor (`cc -E`), not a standalone `cpp`: a traditional
`cpp` (e.g. Apple's `/usr/bin/cpp`) only honors a `#` directive in column 1, so
indented `#include` lines silently fail to expand.  `-nostdinc` keeps the host's
system headers out so only these BESM-6 headers are seen.  No `-P` is needed —
`parse` consumes the `# line` markers and they keep the original line numbers.

## Freestanding subset (C11 §4)

A freestanding implementation needs only these — types and macros, no runtime:

`float.h` · `iso646.h` · `limits.h` · `stdalign.h` · `stdarg.h` · `stdbool.h` ·
`stddef.h` · `stdint.h` · `stdnoreturn.h`

`stdarg.h` is fully functional: the BESM-6 ABI puts every argument in one word,
so `va_list` is a word pointer and `va_arg` steps one word (the same walk
`madlen/doprnt.c` performs).

## Hosted subset

All other headers, **except** `complex.h`, `stdatomic.h`, `threads.h` (and
`tgmath.h`'s complex half), which are intentionally not provided.

`besm6.h` and `malloc.h` belong to neither subset: they are BESM-6 extensions,
not C11 headers.  `besm6.h` declares the compiler intrinsics that give C direct
access to the machine instructions it cannot otherwise express — see
[docs/Besm6_Intrinsics.md](../../../docs/Besm6_Intrinsics.md).

## What is implemented vs. planned

A routine listed below has a real implementation in the runtime library; everything
else is a declaration awaiting an implementation (marked `TODO` in the header). The
runtime builds in two forms — the Madlen `libc.bin` (Dubna monitor) and the Unix
`libc.a` (`b6as`/`b6ld`/`b6sim`). Both carry the same set **except** the dynamic
allocator, which is in the Unix `libc.a` only (it depends on the b6ld/b6sim memory map).

| Header | Implemented routines |
|---|---|
| `stdio.h` | `printf`, `sprintf`, `snprintf`, `puts`, `putchar`, plus the BESM-6 console primitives `putbyte`, `putch`, `getch`, `flush` |
| `stdlib.h` | `exit`, `atoi`; **Unix `libc.a` only:** `malloc`, `calloc`, `realloc`, `free` |
| `string.h` | `strlen`, `strcpy`, `strncpy`, `strcat`, `strncat`, `strcmp`, `strncmp`, `strchr`, `strrchr`, `strstr`, `strtok`, `strerror`, `memcpy`, `memmove`, `memset`, `memcmp`, `memchr` |
| `math.h` | `modf`, `fabs`, `fmin`, `fmax`, `fma`, `frexp`, `ldexp` |
| `stdarg.h` | `va_start`, `va_arg`, `va_end`, `va_copy` (word-pointer `va_list`) |
| `besm6.h` | none — the `__besm6_*` intrinsics are no library routines but machine instructions the back end inlines, and that lowering is not implemented yet (tasks I2–I5 in [backend/besm6/TODO.md](../../../backend/besm6/TODO.md)); today a call to one compiles into an ordinary external call to a symbol that does not exist |
| all others | — (declarations only, for now) |

The `printf`/`sprintf`/`snprintf` prototypes use the ordinary ISO variadic form.
The libc *definitions* still read their arguments through an internal `int args`
slot, but on the BESM-6 ABI the first variadic argument lands in that slot, so an
ISO call is binary-compatible (this is how the existing run-tests call printf).

## BESM-6 type ceilings to keep in mind

- Every scalar is one 48-bit word; signed integers are 41-bit (`INT_MAX` =
  2^40−1), unsigned are 48-bit (`UINT_MAX` = 2^48−1).
- No `int16_t`/`int32_t`/`int64_t` (no types of those widths); `intmax_t` is only
  41-bit signed.  Only `int8_t`/`uint8_t` exact-width types exist.
- `float` == `double` == `long double`: one native floating format, ~12 decimal
  digits, **no infinities, NaNs, or denormals** — so `<math.h>` omits `INFINITY`
  and `NAN`, and `<tgmath.h>` collapses to the single double-typed functions.
- Plain `char` is unsigned; `char*`/`void*` are fat pointers carrying an in-word
  byte offset.
