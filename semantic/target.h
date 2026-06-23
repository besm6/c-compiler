#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//
// Target descriptor: size and alignment of primitive scalar types for one
// supported architecture.  All values are in bytes (C addressable units,
// i.e. in the same unit as sizeof() returns).  sizeof(char) == 1 always
// and is not stored here; bool/schar/uchar are likewise always 1 byte.
// sizeof(enum) == sizeof(int) by convention.
//
typedef struct {
    const char *name;
    size_t short_size, short_align;
    size_t int_size, int_align;
    size_t long_size, long_align;
    size_t llong_size, llong_align; // long long / unsigned long long
    size_t float_size, float_align;
    size_t double_size, double_align;
    size_t ldouble_size, ldouble_align; // long double
    size_t pointer_size, pointer_align;
    // Signed integer value width in bits (two's-complement, sign bit included).
    // Normally <type>_size*8, but BESM-6 signed ints are 41-bit inside a 48-bit
    // word.  The unsigned value width is always the full storage <type>_size*8.
    int short_bits, int_bits, long_bits, llong_bits;
    // Signedness of plain `char` (target-defined in C): 1 = signed, 0 = unsigned.
    // `signed char` and `unsigned char` are unaffected (always signed/unsigned).
    int char_signed;
} Target;

// Active target.  Defaults to x86_64.  Set this before calling any
// pipeline function (typecheck_decl, translate, etc.).
extern const Target *target_config;

// Look up a target by name (e.g. "x86_64", "besm6").
// Returns NULL if the name is not recognised.
const Target *target_lookup(const char *name);

// Print all known target names to stderr, one per line.
void target_list(void);

#ifdef __cplusplus
}
#endif
