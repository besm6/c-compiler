#include "target.h"

#include <stdio.h>
#include <string.h>

// clang-format off
static const Target targets[] = {
    // name
    //            short_size  short_align
    //            int_size    int_align
    //            long_size   long_align
    //            llong_size  llong_align
    //            float_size  float_align
    //            double_size double_align
    //            ldouble_size ldouble_align
    //            pointer_size pointer_align

    { "avr",
      2, 1,   // short
      2, 1,   // int
      4, 1,   // long
      8, 1,   // long long
      4, 1,   // float
      4, 1,   // double  (same as float in avr-gcc default mode)
      4, 1,   // long double (same as float)
      2, 1 }, // pointer (16-bit data address)

    { "msp430",
      2, 2,   // short
      2, 2,   // int
      4, 2,   // long
      8, 2,   // long long
      4, 2,   // float
      8, 2,   // double
      8, 2,   // long double (same as double)
      2, 2 }, // pointer (16-bit, 4 bytes on MSP430X)

    { "arm32",
      2, 2,   // short
      4, 4,   // int
      4, 4,   // long (ILP32: same size as int)
      8, 8,   // long long
      4, 4,   // float
      8, 8,   // double
      8, 8,   // long double (same as double on ARM EABI)
      4, 4 }, // pointer

    { "aarch64",
      2, 2,   // short
      4, 4,   // int
      8, 8,   // long (LP64)
      8, 8,   // long long
      4, 4,   // float
      8, 8,   // double
     16,16,   // long double (IEEE 754 binary128)
      8, 8 }, // pointer

    { "x86_64",
      2, 2,   // short
      4, 4,   // int
      8, 8,   // long (LP64)
      8, 8,   // long long
      4, 4,   // float
      8, 8,   // double
     16,16,   // long double (x87 80-bit value in 16-byte slot)
      8, 8 }, // pointer

    { "riscv32",
      2, 2,   // short
      4, 4,   // int
      4, 4,   // long (ILP32: same size as int)
      8, 8,   // long long
      4, 4,   // float
      8, 8,   // double
     16,16,   // long double (IEEE 754 binary128, software)
      4, 4 }, // pointer

    { "riscv64",
      2, 2,   // short
      4, 4,   // int
      8, 8,   // long (LP64)
      8, 8,   // long long
      4, 4,   // float
      8, 8,   // double
     16,16,   // long double (IEEE 754 binary128, software)
      8, 8 }, // pointer

    { "mmix",
      2, 2,   // short
      4, 4,   // int
      8, 8,   // long (LP64)
      8, 8,   // long long
      4, 4,   // float
      8, 8,   // double
      8, 8,   // long double (same as double; no wider FP hardware)
      8, 8 }, // pointer

    // BESM-6: 48-bit word-oriented machine.
    // sizeof() values are in 8-bit bytes (CHAR_BIT = 8).
    // One machine word = 6 bytes.  Two-word types (long long, long double)
    // are 12 bytes.  All alignments are 1 word = 6 bytes.
    { "besm6",
      6, 6,   // short   (1 word)
      6, 6,   // int     (1 word)
      6, 6,   // long    (1 word)
     12, 6,   // long long  (2 words, 1-word aligned)
      6, 6,   // float   (1 word, BESM-6 native FP)
      6, 6,   // double  (1 word, same as float)
     12, 6,   // long double (2 words, 1-word aligned)
      6, 6 }, // pointer (1 word, 15-bit word address)
};
// clang-format on

#define NUM_TARGETS ((int)(sizeof(targets) / sizeof(targets[0])))

// Index of the default target (x86_64).
#define DEFAULT_TARGET_INDEX 4

const Target *target_config = &targets[DEFAULT_TARGET_INDEX];

const Target *target_lookup(const char *name)
{
    for (int i = 0; i < NUM_TARGETS; i++) {
        if (strcmp(targets[i].name, name) == 0) {
            return &targets[i];
        }
    }
    return NULL;
}

void target_list(void)
{
    for (int i = 0; i < NUM_TARGETS; i++) {
        fprintf(stderr, "  %s\n", targets[i].name);
    }
}
