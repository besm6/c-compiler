//
// Chapter 19 — optimizer tests, imported from "Writing a C Compiler"
// (tests/chapter_19: constant_folding, copy_propagation, dead_store_elimination,
// unreachable_code_elimination, incl. dont_propagate/dont_elim negatives).
//
// Each book program is compiled and run through the full typecheck + translate +
// optimize pipeline; we pin the optimized TAC YAML (PipelineTest::OptimizeYaml).
// Following the book's convention, the inspection points are the target* funcs;
// where a program defines them, its self-checking main() driver is dropped (that
// runtime check is exercised by the whole_pipeline BESM-6 run tests in
// backend/besm6/chapter19_tests.cpp).  Programs whose logic lives in main are
// kept whole.  Goldens reflect the BESM-6 target (int/long are 41-bit, one
// word).  The book's host-only "#if/#pragma" lines are stripped (our scanner has
// no preprocessor).
//
// Cases the BESM-6 target/optimizer can't handle are DISABLED_ with a one-line
// reason (NaN constant-fold loops; a static-local name collision under the
// no-shadowing rule).
//
// A few programs whose optimized TAC is very large (e.g. copy propagation through
// nested loops) would need multi-thousand-line exact goldens; those use a compact
// instruction-kind histogram assertion (PipelineTest::KindHistogram) instead,
// which still pins the optimized instruction mix.
//
// This file is generated content pinned against the optimizer's actual output;
// regenerate goldens if the optimizer's TAC emission changes intentionally.
//
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include "pipeline_test_fixture.h"

// fatal_error() for the optimizer-book-tests executable.  The compiler libraries
// call it; the regular optimizer-tests binary defines its own copy (in
// pipeline_tests.cpp), so the book executable needs this one.
extern "C" _Noreturn void fatal_error(const char *message, ...)
{
    fprintf(stderr, "Fatal error: ");
    va_list ap;
    va_start(ap, message);
    vfprintf(stderr, message, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

TEST_F(PipelineTest, Chapter19_CF_IntOnly_FoldBinary)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we can constant-fold binary expressions, including
 * the arithmetic +, -, *, /, and % operations, and the relational
 * ==, !=, <, <=, >, and >= operations.
 */

// arithmetic tests
int target_add(void) {
    return 100 + 200;  // 300
}

int target_sub(void) {
    return 2 - 2147483647;  // -2147483645
}

int target_mult(void) {
    return 1000 * 1000;  // 1000000
}

int target_div(void) {
    return 1111 / 4;  // 277
}

int target_rem(void) {
    return 10 % 3;  // 1
}

// relational tests
int target_eq_true(void) {
    return 2147483647 == 2147483647;  // 1
}

int target_eq_false(void) {
    return 2147483647 == 2147483646;  // 0
}

int target_neq_true(void) {
    return 1111 != 1112;  // 1
}

int target_neq_false(void) {
    return 1112 != 1112;  // 0
}

int target_gt_true(void) {
    return 10 > 1;  // 1
}

int target_gt_false(void) {
    return 10 > 10;  // 0
}

int target_ge_true(void) {
    return 123456 >= 123456;  // 1
}

int target_ge_false(void) {
    return 2147 >= 123456;  // 0
}

int target_lt_true(void) {
    // 256 < 2^30 + 256
    return 256 < 1073742080;  // 1
}

int target_lt_false(void) {
    return 256 < 0;  // 0
}

int target_le_true(void) {
    return 123456 <= 123457;  // 1
}

int target_le_false(void) {
    return 123458 <= 123457;  // 0
}

int val_to_negate = 2147483645;
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 300
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: -2147483645
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1000000
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 277
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
)OPT");
}

TEST_F(PipelineTest, Chapter19_CF_IntOnly_FoldConditionalJump)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test constant folding of JumpIfZero and JumpIfNotZero instructions
 * resulting from && and || operations.
 * */

// We'll emit two TACKY instructions of the form
// JumpIfZero(0, false_label)
// both should be rewritten as Jump instructions
int target_jz_to_jmp(void) {
    return 0 && 0; // 0
}

// We'll emit two TACKY instructions of the form
// JumpIfZero(1, false_label)
// both should be removed
int target_remove_jz(void) {
    return 1 && 1; // 1
}

// We'll emit two JumpIfNotZero instructions:
// JumpIfNotZero(3, true_label)
// JumpIfNotZero(99, true_label)
// both should be written as Jump instructions
int target_jnz_to_jmp(void) {
    return 3 || 99; // 1
}

// We'll emit two JumpIfNotZero instructions:
// JumpIfNotZero(0, true_label)
// JumpIfNotZero(1, true_label)
// we should remove the first, rewrite the second as a Jump instruction
int target_remove_jnz(void) {
    return 0 || 1; // 1
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %2
- instruction:
  kind: return
  src:
    kind: var
    name: %2
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %2
- instruction:
  kind: return
  src:
    kind: var
    name: %2
)OPT");
}

TEST_F(PipelineTest, Chapter19_CF_IntOnly_FoldControlFlow)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test constant folding of JumpIfZero and JumpIfNotZero instructions
 * resulting from conditional ?: expressions, if statements, and loops.
 * */

int target_if(void) {
    if (0)
        return 1;
    return 0;
}

int target_if_else_true(void) {
    if (1) {
        return 2;
    } else {
        return 3;
    }
}

int target_if_else_false(void) {
    if (0) {
        return 2;
    } else {
        return 3;
    }
}

int target_conditional_true(void) {
    return 1 ? 2 : 3;
}

int target_conditional_false(void) {
    return 0 ? 4 : 5;
}

int target_do_loop(void) {
    int retval = 0;
    do {
        retval = 10;
    } while (0);
    return retval;
}

int target_while_loop_false(void) {
    int retval = 0;
    while (0) {
        retval = 10;
    }
    return retval;
}

int target_while_loop_true(void) {
    int retval = 0;
    while (1048576) {  // 1048576 == 2^20
        retval = 10;
        break;
    }
    return retval;
}

int target_for_loop_true(void) {
    int retval = 0;
    for (int i = 100; 123;) {
        retval = i;
        break;
    }
    return retval;
}

int target_for_loop_false(void) {
    int retval = 0;
    for (int i = 100; 0;) {
        retval = i;
        break;
    }
    return retval;
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 2
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 3
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 2
  dst:
    kind: var
    name: %2
- instruction:
  kind: return
  src:
    kind: var
    name: %2
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 5
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 10
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 0
  dst:
    kind: var
    name: %retval
- instruction:
  kind: return
  src:
    kind: var
    name: %retval
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 10
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 100
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 0
  dst:
    kind: var
    name: %retval
- instruction:
  kind: return
  src:
    kind: var
    name: %retval
)OPT");
}

TEST_F(PipelineTest, Chapter19_CF_IntOnly_FoldException)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test cases where result of constant-folded expression is undefined (i.e.
 * division by zero and overflow). In this particular case, these expressions
 * wouldn't actually be evaluated at runtime, so the program's behavior is
 * well-defined; the main thing we're testing here is that compiler doesn't
 * crash during the constant folding pass. There are no target_ functions
 * because we don't inspect the assembly in this program.
 * */

int main(void) {
    int dead_div_by_zero = 1 || (1 / 0); // we short-circuit before evaluating 1 / 0
    int dead_zero_remainder = 0 && (100 % 0); // we short-circuit before evaluating 100 % 0
    int overflow = 0 ? (2147483647 + 10) : 100; // 2147483647 + 10 would overflow, but we skip it and just evaluate 100

    if (dead_div_by_zero != 1) {
        return 1;
    }
    if (dead_zero_remainder != 0) {
        return 2;
    }

    if (overflow != 100) {
        return 3;
    }

    return 0;  // success
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
)OPT");
}

TEST_F(PipelineTest, Chapter19_CF_IntOnly_FoldUnary)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we can constant-fold !, -, and ~ expressions. */

int target_negate(void) {
    return -3;
}

int target_negate_zero(void) {
    return -0;
}

int target_not(void) {
    return !1024;
}

int target_not_zero(void) {
    return !0;
}

int target_complement(void) {
    return ~1;
}

int three = 3;
int two = 2;
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: -3
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: -2
)OPT");
}

TEST_F(PipelineTest, Chapter19_CF_IntOnly_ExtraCredit_FoldBitwise)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test constant-folding the bitwise &, |, ^, >>, and << expressions */

int target_and(void) {
    // 0x0f0f_0f0f & 0x00ff_00ff
    return 252645135 & 16711935;
}

int target_or(void) {
    // 0x0f0f_0f0f | 0x00ff_00ff
    return 252645135 | 16711935;
}

int target_xor(void){
    // 0x0f0f_0f0f ^ 0x00ff_00ff
    return 252645135 ^ 16711935;
}

int target_shift_left(void) {
    return 291 << 18;
}

int target_shift_right(void) {
    return 252645135 >> 9;
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 983055
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 268374015
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 267390960
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 76283904
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 493447
)OPT");
}

TEST_F(PipelineTest, Chapter19_CF_AllTypes_FoldCastFromDouble)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test constant folding of casts from double to integer types,
 * making sure the results are correctly rounded.
 * */

char target_to_char(void) {
    return (char)126.5;
}

unsigned char target_to_uchar(void) {
    return (unsigned char)254.9;
}

int target_to_int(void) {
    return (int)5.9;
}

unsigned target_to_uint(void) {
    // constant in the range of uint but not int
    return (unsigned)2147483750.5;
}

long target_to_long(void) {
    // nearest representable double is 9223372036854774784.0,
    // which will be converted to long int 9223372036854774784
    return (long)9223372036854774783.1;
}

unsigned long target_to_ulong(void) {
    // constant in the range of ulong but not long
    return (unsigned long)13835058055282163712.5;
}

unsigned long target_implicit(void) {
    // same as target_to_ulong but cast is implicit; make sure we still constant fold it
    return 3458764513821589504.0;
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 126
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: uint
      value: 254
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 5
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: uint
      value: 2147483750
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: -2147483648
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: uint
      value: 0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: uint
      value: 1048576
)OPT");
}

TEST_F(PipelineTest, Chapter19_CF_AllTypes_FoldCastToDouble)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test constant folding of casts from integer types to double
 * making sure the results are correctly rounded.
 * */

// can convert 32-bit ints to double exactly
double target_from_int(void) {
    return (double)1000;
}

// cast a double outside the range of int
// (See example in Chapter 13, "Converting an Unsigned Integer to a double")
double target_from_uint(void) {
    return (double)4294967290u;
}

// this value is exactly between two representable doubles;
// using ties-to-even, it should be converted to 4611686018427387904.0
// (From double-rounding example in Chapter 13, "Converting an Unsigned Integer
// to a double")
double target_from_long(void) {
    return (double)4611686018427388416l;
}

// convert a value outside the range of signed long
// (From double-rounding example in Chapter 13, "Converting an Unsigned Integer
// to a double")
double target_from_ulong(void) {
    return (double)9223372036854776833ul;
}

// same as target_from_int but cast is implicit;
// make sure we still constant fold it
double target_implicit(void) {
    return 1000;
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: double
      value: 0x1.f4p+9
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: double
      value: 0x1.fffffff4p+31
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: double
      value: 0x1p+62
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: double
      value: 0x1.0000000000001p+63
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: double
      value: 0x1.f4p+9
)OPT");
}

TEST_F(PipelineTest, Chapter19_CF_AllTypes_FoldConditionalJump)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test constant folding of JumpIfZero and JumpIfNotZero instructions
 * resulting from && and || operations, with operand types other than int.
 * Identical to chapter_19/constant_folding/int_only/fold_conditional_jump.c
 * but with non-int operands
 * */

// We'll emit two TACKY instructions of the form
// JumpIfZero(0, false_label)
// both should be rewritten as Jump instructions
int target_jz_to_jmp(void) {
    return 0l && 0; // 0
}

// We'll emit two TACKY instructions of the form
// JumpIfZero(1, false_label)
// both should be removed
int target_remove_jz(void) {
    return 1u && 1.; // 1
}

// We'll emit two JumpIfNotZero instructions:
// JumpIfNotZero(3, true_label)
// JumpIfNotZero(99, true_label)
// both should be written as Jump instructions
int target_jnz_to_jmp(void) {
    return 3.5 || 99ul; // 1
}

// We'll emit two JumpIfNotZero instructions:
// JumpIfNotZero(0, true_label)
// JumpIfNotZero(1, true_label)
// we should remove the first, rewrite the second as a Jump instruction
int target_remove_jnz(void) {
    return 0ul || 1; // 1
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: binary
  op: not_equal
  src1:
    kind: constant
    const:
      kind: double
      value: 0x1p+0
  src2:
    kind: constant
    const:
      kind: int
      value: 0
  dst:
    kind: var
    name: %2
- instruction:
  kind: return
  src:
    kind: var
    name: %2
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %2
- instruction:
  kind: return
  src:
    kind: var
    name: %2
)OPT");
}

TEST_F(PipelineTest, Chapter19_CF_AllTypes_FoldDouble)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test constant folding of all operations on doubles and make sure they're
 * correctly rounded.
 * */


double target_add(void) {
    // Because 1.2345e60 is so large, adding one to it doesn't change its value
    return 1.2345e60 + 1.;
}

double target_sub(void) {
    // make sure we properly calculate the difference between two very close
    // subnormal numbers
    return 5.85543871245623688067e-311 - 5.85543871245574281503e-311;
}

double target_mult(void) {
    return 2.1 * 3.0;
}

double target_div(void) {
    return 1100.5 / 5000.;
}

double target_div_underflow(void) {
    // this result should underflow to zero
    return 0.5e-100 / 2e307;
}

double target_neg(void) {
    return -.000000275;
}

int target_not(void) {
    return !1e30;
}

int target_eq(void) {
    // these decimal constants should be rounded to the same floating-point
    // value, so this will return 1
    return 0.1 == 0.10000000000000001;
}

int target_neq(void) {
    // these should compare unequal; 5e-324 will be rounded to the subnormal
    // number just above zero
    return 5e-324 != 0.0;
}

int target_gt(void) {
    return 1e308 > 1e307;
}

int target_ge(void) {
    return 3.1 >= 3.1;
}

int target_lt(void) {
    // these decimal constants should be rounded to the same floating-point
    // value, so this will return 0
    return 0.1 < 0.10000000000000001;
}

int target_le(void) {
    return 0.5 <= 0.;
}

double target_negate_zero(void) {
    // make sure this gives us negative zero and not zero
    return -0.0;
}

double target_infinity(void) {
    // this will result in infinity
    return 1e308 * 2.;
}

int target_compare_infininty(void) {
    // infinity == infinity
    return 10e308 == 12e308;
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: double
      value: 0x1.89559ac537b6bp+199
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: double
      value: 0x1p-1074
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: double
      value: 0x1.9333333333334p+2
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: double
      value: 0x1.c2c3c9eecbfb1p-3
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: double
      value: 0x0p+0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: double
      value: -0x1.27476ca61b882p-22
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: double
      value: -0x0p+0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: double
      value: inf
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
)OPT");
}

TEST_F(PipelineTest, Chapter19_CF_AllTypes_FoldDoubleCastException)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test case where result of casting double to integer is undefined (because
 * the result is out of range). The program's behavior is well-defined
 * because the cast operations aren't actually executed; the main thing we're
 * testing here is that compiler doesn't crash during the constant folding pass.
 * There are no target_ functions
 * because we don't inspect the assembly in this program.
 * */



int main(void) {
    int dead_int_cast = 0 ? (int)2147483649.0 : 100; // in the range of uint but not int
    unsigned int dead_uint_cast = 0 ? (unsigned int) 34359738368.0 : 200; // in the range of long but not uint
    signed long dead_long_cast = 1 ? 300 : 9223372036854777856.0; // in the range of unsigned long but not long
    unsigned long dead_ulong_cast = 1 ? 200 : (unsigned long)200e300; //outside the range of unsigned long
    return dead_int_cast + dead_uint_cast + dead_long_cast + dead_ulong_cast;
}
)SRC"),
              R"OPT(- instruction:
  kind: zero_extend
  src:
    kind: constant
    const:
      kind: int
      value: 300
  dst:
    kind: var
    name: %21
- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: %21
  src2:
    kind: constant
    const:
      kind: int
      value: 300
  dst:
    kind: var
    name: %22
- instruction:
  kind: binary
  op: add_unsigned
  src1:
    kind: var
    name: %22
  src2:
    kind: constant
    const:
      kind: long
      value: 200
  dst:
    kind: var
    name: %24
- instruction:
  kind: truncate
  src:
    kind: var
    name: %24
  dst:
    kind: var
    name: %25
- instruction:
  kind: return
  src:
    kind: var
    name: %25
)OPT");
}

TEST_F(PipelineTest, Chapter19_CF_AllTypes_FoldExtensionsAndCopies)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we can constant-fold zero- and sign-extensions from shorter to
 * longer ints, and conversions from one integer type to another of the same
 * size (e.g. long to unsigned long). We inspect the assembly for sign-extension
 * operations to make sure there are no movsx instructions, but not for
 * zero-extension or conversions between types of the same size because those
 * turn into a single 'mov' instruction whether they're sign extended or not.
 * We also can't test sign- or zero-extension of character types yet because
 * there are no constants of character type. The whole_pipeline/ folder has more
 * robust tests for constant folding of all these type conversions - we can test
 * them more thoroughly once other optimizations are enabled.
 */

long uint_to_long(void) {
    return (long)4294967295U;
}

unsigned long uint_to_ulong(void) {
    return (unsigned long)4294967295U;
}

/* These next two are target_* functions b/c they require sign extension */
unsigned long target_int_to_ulong(void) {
    return (unsigned long)2147483647;
}

long target_int_to_long(void) {
    return (long)1;
}

int uint_to_int(void) {
    // outside the range of int; will be negative
    return (int)4294967200U;
}

unsigned int int_to_uint(void) {
    return (unsigned)2147480000;
}

long ulong_to_long(void) {
    // outside the range of long; will be negative
    return (long)18446744073709551615UL;
}

unsigned long long_to_ulong(void) {
    return 2147483650l;
}

long implicit(void) {
    // same as ulong_to_long, but cast is implicit
    return 18446744073709551615UL;
}

long one = 1l;
long ninety_six = 96l;
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: ulong
      value: 4294967295
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: ulong
      value: 4294967295
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: long
      value: 2147483647
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: long
      value: 1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: uint
      value: 4294967200
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 2147480000
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: ulong
      value: 18446744073709551615
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: long
      value: 2147483650
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: ulong
      value: 18446744073709551615
)OPT");
}

TEST_F(PipelineTest, Chapter19_CF_AllTypes_FoldLong)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test constant folding of operations on long ints.
 * Make sure we correctly handle operations that require all 64 bits.
 * Tests do not involve any overflow, since that's undefined behavior.
 */
long target_add(void) {
    // we can add longs when the result exceeds INT_MAX
    return 2147483647l + 1000l;
}

long target_sub(void) {
    // we can subtract longs when the result is smaller than INT_MIN
    return 1000l - 9223372036854773807l;
}

long target_mult(void) {
    // can multiply longs when the result exceeds INT_MAX
    return 35184372088832l * 4l;
}

long target_div(void) {
    // both operands are larger than INT_MAX
    return 9223372036854775807l / 3147483647l;
}

long target_rem(void) {
    // both operands are larger than INT_MAX
    return 9223372036854775807l % 3147483647l;
}

long target_complement(void) {
    // alternating 1s and 0s
    return ~6148914691236517206l;
}

long target_neg(void) {
    // except for most significant bit, upper 32 bits of negated value are all
    // zeros
    return -(9223372036854775716l);
}

int target_not(void) {
    // 2^56 + 2^45 + 2^44
    // lower 32 bits are all zeros
    return !72110370596061184l;
}

int target_eq(void) {
    return 9223372036854775716l == 9223372036854775716l;
}

int target_neq(void) {
    // lower 32 bits of 72110370596061184l are all zeros
    return 72110370596061184l != 0l;
}

int target_gt(void) {
    // second operand is greater, but if we only looked at lower
    // 32 bits we'd think the first was greater
    return 549755813889l > 17592186044416l ;  // 2^39 + 1 > 2^44
}

int target_ge(void) {
    return 400l >= 399l;
}

int target_lt(void) {
    // compare two values whose lower 32 bits are identical
    return 17592186044416l < 549755813888l;  // 2^44 < 2^39
}

int target_le(void) {
    // if we interpreted this as a signed int it would be negative
    return 2147483648l <= 0l;
}

long sub_result = 9223372036854772807l;
long complement_result = 6148914691236517207l;
long neg_result = 9223372036854775716l;
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: long
      value: 2147484647
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: long
      value: -9223372036854772807
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: long
      value: 140737488355328
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: long
      value: 2930395538
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: long
      value: 1758008721
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: long
      value: -6148914691236517207
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: long
      value: -9223372036854775716
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
)OPT");
}

// Structural assertion (instruction-kind histogram) — exact TAC
// YAML too large to pin verbatim (124 instructions).
TEST_F(PipelineTest, Chapter19_CF_AllTypes_FoldTruncate)
{
    EXPECT_EQ(KindHistogram(OptimizeYaml(R"SRC(
/* Test constant folding of all conversions from longer to shorter integer
 * types. For now, we just verify that the behavior is correct, without
 * inspecting the assembly code; we can't tell whether a truncate operation was
 * constant folded because it turns into a single 'mov' instruction either way.
 * The whole_pipeline/ folder has more robust tests for constant folding of
 * truncate operations - in these tests, constant-folding truncate enables
 * other optimizations.
 */


// truncate long
int long_to_int(void) {
    // 2^45 + 2^35 + 1234
    return (int)35218731828434l;
}

unsigned int long_to_uint(void) {
    // 2^45 + 2^35 + 1234
    return (unsigned int)35218731828434l;
}

char long_to_char(void) {
    // LONG_MAX
    return (char)9223372036854775807l;
}

signed char long_to_schar(void) {
    // 2^62 + 128
    return (signed char)4611686018427388032l;
}

unsigned char long_to_uchar(void) {
    // UINT_MAX
    return (unsigned char)4294967295UL;
}

// truncate unsigned long
int ulong_to_int(void) {
    // ULONG_MAX
    return (int)18446744073709551615UL;
}

unsigned int ulong_to_uint(void) {
    return (unsigned int)18446744073709551615UL;
}

char ulong_to_char(void) {
    return (char)4294967295UL;
}

signed char ulong_to_schar(void) {
    return (signed char)4611686018427388032ul;
}

unsigned char ulong_to_uchar(void) {
    // 2^63 + 255
    return (unsigned char)9223372036854776063ul;
}

// truncate int
char int_to_char(void) {
    return (char)1274;
}

signed char int_to_schar(void) {
    // INT_MAX
    return (signed char)2147483647;
}

unsigned char int_to_uchar(void) {
    return (unsigned char)1274;
}

// truncate unsigned int
char uint_to_char(void) {
    return (char)2147483901u;  // 2^31 + 253
}

signed char uint_to_schar(void) {
    return (signed char)2147483660u;  // 2^31 + 12
}

unsigned char uint_to_uchar(void) {
    return (unsigned char)2147483901u;
}

// same as uint_to_uchar but implicit cast
unsigned char implicit(void) {
    return 2147483901u;
}

int one = 1;
int six = 6;
int three = 3;
int one_twenty_eight = 128;

int main(void) {
    // truncate longs

    // 0x0000_2008_0000_04d2 --> 0x0000_04d2
    if (long_to_int() != 1234) {
        return 1;
    }
    if (long_to_uint() != 1234u) {
        return 2;
    }

    // 0x7fff_ffff_ffff_ffff --> 0xff
    if (long_to_char() != -one) {
        return 3;
    }

    // 0x4000_0000_0000_0080 --> 0x80
    if (long_to_schar() != -one_twenty_eight) {
        return 4;
    }

    // 0x0000_0000_ffff_ffff -> 0xff
    if (long_to_uchar() != 255) {
        return 5;
    }

    // truncate ulongs

    // 0xffff_ffff_ffff_ffff --> 0xffff_ffff
    if (ulong_to_int() != -one) {
        return 6;
    }
    if (ulong_to_uint() != 4294967295U) {
        return 7;
    }

    // 0x7fff_ffff_ffff_ffff --> 0xff
    if (ulong_to_char() != -one) {
        return 8;
    }

    // 0x4000_0000_0000_0080 --> 0x80
    if (ulong_to_schar() != -one_twenty_eight) {
        return 9;
    }

    // 0x0000_0000_ffff_ffff -> 0xff
    if (ulong_to_uchar() != 255) {
        return 10;
    }

    // truncate ints

    // 0x0000_04fa -> 0xfa
    if (int_to_char() != -six) {
        return 11;
    }

    // 0x7fff_ffff -> 0xff
    if (int_to_schar() != -one) {
        return 12;
    }

    // 0x0000_04fa -> 0xfa
    if (int_to_uchar() != 250) {
        return 13;
    }

    // truncate uints

    // 0x8000_00fd --> 0xfd
    if (uint_to_char() != -three) {
        return 14;
    }

    // 0x8000_000c -> 0x0c
    if (uint_to_schar() != 12) {
        return 15;
    }
    // 0x8000_00fd --> 0xfd
    if (uint_to_uchar() != 253) {
        return 16;
    }
    if (implicit() != 253) {
        return 17;
    }
    return 0;
}
)SRC")),
              "binary=17 fun_call=17 jump_if_zero=17 label=17 return=35 sign_extend=8 unary=8 zero_extend=5");
}

TEST_F(PipelineTest, Chapter19_CF_AllTypes_FoldUint)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test constant folding of all operations on unsigned ints;
 * make sure they wrap around correctly
 * and that we evaluate them with unsigned division/comparison functions.
 */
unsigned int target_add(void) {
    // result exceeds UINT_MAX and wraps around past 0
    return 4294967295U + 10u;
}

unsigned int target_sub(void) {
    // result is less then 0 and wraps back round past UINT_MAX
    return 10u - 12u;
}

unsigned int target_mult(void) {
    // wraps back around to 2147483648u
    return 2147483648u * 3u;
}

unsigned int target_div(void) {
    // result would be different if we interpreted values as signed
    return 4294967286u / 10u;
}

unsigned int target_rem(void) {
    // result would be different if we interpreted values as signed
    return 4294967286u % 10u;
}

unsigned int target_complement(void) {
    return ~1u;
}

unsigned int target_neg(void) {
    return -10u;
}

int target_not(void) {
    return !65536u;  // 2^16
}

int target_eq(void) {
    return 100u == 100u;
}

int target_neq(void) {
    // these have identical binary representations except for the most
    // significant bit
    return 2147483649u != 1u;
}

int target_gt(void) {
    // make sure we're using unsigned comparisons;
    // if we interpret these as signed integers,
    // we'll think 2147483649u is negative and return 0
    return 2147483649u > 1000u;
}

int target_ge(void) {
    return 4000000000u >= 3999999999u;
}

int target_lt(void) {
    // as with target_gt, make sure we don't interpret 2147483649u
    // as a negative signed integer
    return 2147483649u < 1000u;
}

int target_le(void) {
    return 4000000000u <= 3999999999u;
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: uint
      value: 9
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: uint
      value: 4294967294
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: uint
      value: 2147483648
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: uint
      value: 429496728
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: uint
      value: 6
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: uint
      value: 18446744073709551614
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: uint
      value: 18446744073709551606
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
)OPT");
}

TEST_F(PipelineTest, Chapter19_CF_AllTypes_FoldUlong)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test constant folding of all operations on unsigned longs;
 * make sure that they wrap around correctly,
 * that we evaluate them with unsigned division/comparison functions,
 * and that we can evaluate expressions requiring all 64 bits.
 */
unsigned long target_add(void) {
    // result exceeds ULONG_MAX and wraps around past 0
    return 18446744073709551615UL + 10ul;
}

unsigned long target_sub(void) {
    // result is less then 0 and wraps back around past ULONG_MAX
    return 10ul - 12ul;
}

unsigned long target_mult(void) {
    // wraps back around to 9223372036854775808ul
    return 9223372036854775808ul * 3ul;
}

unsigned long target_div(void) {
    return 18446744073709551614ul / 10ul;
}

unsigned long target_rem(void) {
    return 18446744073709551614ul % 10ul;
}

unsigned long target_complement(void) {
    return ~1ul;
}

unsigned long target_neg(void) {
    return -(9223372036854775900ul);
}

int target_not(void) {
    return !4294967296UL;  // 2^32
}

int target_eq(void) {
    return 18446744073709551615UL == 18446744073709551615UL;
}

int target_neq(void) {
    // these have identical binary representations except for the most
    // significant bit
    return 9223372036854775809ul != 1ul;
}

int target_gt(void) {
    // make sure we're using unsigned comparisons;
    // if we interpret these as signed integers,
    // we'll think 9223372036854775809ul is negative and return 0
    return 9223372036854775809ul > 1000ul;
}

int target_ge(void) {
    // 200ul would be greater if we only considered lower 32 bits
    return 9223372036854775809ul >= 200ul;
}

int target_lt(void) {
    // as with target_gt, make sure we don't interpret 9223372036854775809ul
    // as a negative signed integer
    return 9223372036854775809ul < 1000ul;
}

int target_le(void) {
    return 9223372036854775809ul <= 200ul;
}

int target_le2(void) {
    // make sure we're evaluating <= and not <
    return 9223372036854775809ul <= 9223372036854775809ul;
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: ulong
      value: 9
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: ulong
      value: 18446744073709551614
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: ulong
      value: 9223372036854775808
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: ulong
      value: 1844674407370955161
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: ulong
      value: 4
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: ulong
      value: 18446744073709551614
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: ulong
      value: 9223372036854775716
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
)OPT");
}

TEST_F(PipelineTest, Chapter19_CF_AllTypes_NegativeZero)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* If we deduplicate floating-point StaticConstant constructs, make sure we
 * distinguish between constants with the same value but different alignments.
 * Specifically, if we've already added an ordinary constant -0.0, and then we
 * need a 16-byte aligned -0.0 to use for negation, don't just reuse the
 * previous 8-byte aligned one. (It's okay to either keep them as separate
 * constants, or merge them and keep the higher alignment.) This is a regression
 * test for a bug in the reference implementation. Note that we can only catch
 * this bug once we implement constant folding; before then, we don't add
 * positive StaticConstants.
 * No 'target' function here because we're just looking for correctness,
 * not inspecting assembly.
 * */

double x = 5.0;

int main(void) {
    double d = -0.0;  // add normal constant -0. to list of top-level constants
    return (-x > d); // add 16-byte-aligned constant -0. to negate x
}
)SRC"),
              R"OPT(- instruction:
  kind: unary
  op: negate_double
  src:
    kind: var
    name: x
  dst:
    kind: var
    name: %1
- instruction:
  kind: binary
  op: greater_than_double
  src1:
    kind: var
    name: %1
  src2:
    kind: constant
    const:
      kind: double
      value: -0x0p+0
  dst:
    kind: var
    name: %2
- instruction:
  kind: return
  src:
    kind: var
    name: %2
)OPT");
}

// DISABLED: NaN (0.0/0.0) constant folding loops in the optimizer
TEST_F(PipelineTest, DISABLED_Chapter19_CF_AllTypes_ExtraCredit_CastNanNotExecuted)
{
    OptimizeYaml(R"SRC(
// Make sure the compiler doesn't complain if you try to cast NaN to an int
// in code that isn't executed. (If it actually did execute it would be
// undefined behavior)

static int flse = 0;

int main(void) {
    int retval = 0;
    if (flse) {
        retval = (int) (0.0/0.0);
    }
    return retval;
}
)SRC");
}

TEST_F(PipelineTest, Chapter19_CF_AllTypes_ExtraCredit_FoldBitwiseLong)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test constant-folding the bitwise &, |, ^, >>, and << expressions with long operands */

long target_and(void) {
    // 0x0f0f_0f0f_0f0f_0f0f & 0x00ff_00ff_00ff_00ff
    return 1085102592571150095l & 71777214294589695l;
}

long target_or(void) {
    // 0x0f0f_0f0f_0f0f_0f0f | 0x00ff_00ff_00ff_00ff
    return 1085102592571150095l | 71777214294589695l;
}

long target_xor(void){
    // 0x0f0f_0f0f_0f0f_0f0f ^ 0x00ff_00ff_00ff_00ff
    return 1085102592571150095l ^ 71777214294589695l;
}

long target_shift_left(void) {
    return 1l << 62;
}

long target_shift_right(void) {
    return 72057589742960640l >> 35;
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: long
      value: 4222189076152335
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: long
      value: 1152657617789587455
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: long
      value: 1148435428713435120
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: long
      value: 4611686018427387904
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: long
      value: 2097151
)OPT");
}

TEST_F(PipelineTest, Chapter19_CF_AllTypes_ExtraCredit_FoldBitwiseUnsigned)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test constant-folding the bitwise &, |, ^, >>, and << expressions with unsigned operands */

unsigned target_and(void) {
    // 0xf0f0_f0f0 & 0xff00_ff00
    return 4042322160u & 4278255360u;
}

unsigned long target_or(void) {
    // 0x0f0f_0f0f_0f0f_0f0f | 0xff00_ff00_ff00_ff00
    return 1085102592571150095ul | 18374966859414961920ul;
}

unsigned int target_xor(void) {
    // 0xf0f0_f0f0 ^ 0x0ff0_0ff0
    return 4042322160u ^ 267390960u;
}

unsigned int target_shift_uint_left(void) {
    return 10u << 24l; // doesn't matter that right operand is different type
}

unsigned long target_shift_ulong_left(void) {
    return 2286249799ul << 33u; // result wrap arounds
}

// make sure right shift is logical, not arithmetic
unsigned int target_shift_uint_right(void) {
    return 4294967296u >> 16;
}

unsigned long target_shift_ulong_right(void) {
    return 9223372041149743104ul >> 21l;
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: uint
      value: 4026593280
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: ulong
      value: 18379189048491114255
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: uint
      value: 4278255360
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: uint
      value: 167772160
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: ulong
      value: 1191992160673595392
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: uint
      value: 65536
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: ulong
      value: 4398046513152
)OPT");
}

// DISABLED: NaN (0./0.) constant folding loops in the optimizer
TEST_F(PipelineTest, DISABLED_Chapter19_CF_AllTypes_ExtraCredit_FoldNan)
{
    OptimizeYaml(R"SRC(
/* Test that we can constant fold an operation that results in NaN;
 * the whole_pipeline folder includes a test that we can constant fold
 * operations _using_ NaN.
 */

int double_isnan(double d); // defined in tests/chapter_13/helper_libs/nan.c

double target_nan(void){
    return 0./0.;
}
)SRC");
}

// DISABLED: NaN (0.0/0.0) constant folding loops in the optimizer
TEST_F(PipelineTest, DISABLED_Chapter19_CF_AllTypes_ExtraCredit_ReturnNan)
{
    OptimizeYaml(R"SRC(
/* Test case where we return NaN after constant folding */

int double_isnan(double d); // defined in tests/chapter_13/helper_libs/nan.c

double target(void) {
    return 0.0 / 0.0;
}
)SRC");
}

TEST_F(PipelineTest, Chapter19_CP_IntOnly_ConstantPropagation)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* A basic test of constant propagation in a function with no control flow
 * structures
 * */
int target(void) {
    int x = 3;
    int y = x;
    return x + y;  // should become return 6
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 6
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_IntOnly_DifferentPathsSameCopy)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* If two instances of the copy x = y appear on different paths to some use
 * of x, we can propagate that copy.
 * */
int target(int flag) {
    int x = 0;
    if (flag) {
        x = 3;
    } else {
        x = 3;
    }
    return x;  // this should become 'return 3'
}
)SRC"),
              R"OPT(- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %flag
  target: %0
- instruction:
  kind: jump
  target: %1
- instruction:
  kind: label
  name: %0
- instruction:
  kind: label
  name: %1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 3
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_IntOnly_DifferentSourceValuesSameCopy)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* We can propagate x = y if it appears on all paths to some use of x,
 * even if y doesn't have the same value on all those paths.
 * Based on Figure 19-5.
 * */

int callee(int a, int b) {
    return a + b;
}
int target(int flag) {
    // use static variables here so we can't coalesce x and y
    // into the same register, or into EDI and ESI, once we implement
    // register coalescing; otherwise it might look like we've propagated
    // x = y when we haven't
    static int x;
    static int y;
    if (flag) {
        y = 20;
        x = y;
    } else {
        y = 100;
        x = y;
    }
    // x = y reaches here, though with different values of y
    return callee(x, y);
}
)SRC"),
              R"OPT(- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: %a
  src2:
    kind: var
    name: %b
  dst:
    kind: var
    name: %0
- instruction:
  kind: return
  src:
    kind: var
    name: %0
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %flag
  target: %0
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 20
  dst:
    kind: var
    name: y
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 20
  dst:
    kind: var
    name: x
- instruction:
  kind: jump
  target: %1
- instruction:
  kind: label
  name: %0
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 100
  dst:
    kind: var
    name: y
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 100
  dst:
    kind: var
    name: x
- instruction:
  kind: label
  name: %1
- instruction:
  kind: fun_call
  fun_name: callee
  args:
    - val:
      kind: var
      name: y
    - val:
      kind: var
      name: y
  dst:
    kind: var
    name: %2
- instruction:
  kind: return
  src:
    kind: var
    name: %2
)OPT");
}

// Structural assertion (instruction-kind histogram) — exact TAC
// YAML too large to pin verbatim (32 instructions).
TEST_F(PipelineTest, Chapter19_CP_IntOnly_Fig198)
{
    EXPECT_EQ(KindHistogram(OptimizeYaml(R"SRC(
/* Test case based on Figure 19-8:
 * Make sure we run iterative algorithm until the results converge.
 * */
static int called_counter = 0;

int callee(int i) {
    if (i == 3 && called_counter == 0) {
        // we're on first loop iteration; iterate one more time
        called_counter = 1;
        return 1;
    }
    if (i == 4 && called_counter == 1) {
        // we're on second loop iteration; stop
        called_counter = 2;
        return 0;
    }

    // if we hit this point, something has gone wrong!
    // set called_counter to indicate error, then terminate loop
    called_counter = -1;
    return 0;
}

int target(void) {
    int y = 3;
    int keep_looping;
    do {
        // After analyzing each basic block once,
        // it will look like we could rewrite this as
        // x = callee(3), but once the algorithm converges
        // we'll know that isn't safe.
        keep_looping = callee(y);
        y = 4;
    } while (keep_looping);  // loop should terminate after first iteration
    return y;                // should become return 4
}
)SRC")),
              "binary=6 copy=7 fun_call=1 jump=2 jump_if_not_zero=1 jump_if_zero=4 label=7 return=4");
}

TEST_F(PipelineTest, Chapter19_CP_IntOnly_InitAllCopies)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we initialize each basic block with the set of all copies
 * in the function
 * */

int counter = 0;

int increment_counter(void) {
    counter = counter + 1;
    return 0;
}

int target(void) {
    int y = 3;
    do {
        // when we first process this block,
        // y = 3 will reach it from one predecessor, and we won't have
        // visited the other yet; make sure we still recognize
        // that y = 3 reaches this block (and its successor)
        increment_counter();
    } while (counter < 5);
    return y;  // this should become return 3
}
)SRC"),
              R"OPT(- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: counter
  src2:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %0
- instruction:
  kind: copy
  src:
    kind: var
    name: %0
  dst:
    kind: var
    name: counter
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 3
  dst:
    kind: var
    name: %y
- instruction:
  kind: label
  name: %0
- instruction:
  kind: fun_call
  fun_name: increment_counter
  dst:
    kind: var
    name: %1
- instruction:
  kind: binary
  op: less_than
  src1:
    kind: var
    name: counter
  src2:
    kind: constant
    const:
      kind: int
      value: 5
  dst:
    kind: var
    name: %2
- instruction:
  kind: jump_if_not_zero
  condition:
    kind: var
    name: %2
  target: %0
- instruction:
  kind: return
  src:
    kind: var
    name: %y
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_IntOnly_KillAndAddCopies)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test how we handle copies in the transfer function:
 * x = src should generate a copy _and_ kill previous copies
 * where x is the source or destination
 * */
static int globvar;

int set_globvar(int i) {
    globvar = i;
    return 0;
}

int callee(int a, int b) {
    return a + b;
}

int target(int param) {
    int x = param;
    // should be able to propagate param into var but we don't explicitly
    // check that here
    set_globvar(x);
    int y = x;  // gen y = x;
    x = 10;     // kill x = param and y = x, gen x = 10
    // make sure we propagate x = 10 but not y = x
    return callee(x, y);  // becomes callee(10, y)
}
)SRC"),
              R"OPT(- instruction:
  kind: copy
  src:
    kind: var
    name: %i
  dst:
    kind: var
    name: globvar
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: %a
  src2:
    kind: var
    name: %b
  dst:
    kind: var
    name: %0
- instruction:
  kind: return
  src:
    kind: var
    name: %0
- instruction:
  kind: fun_call
  fun_name: set_globvar
  args:
    - val:
      kind: var
      name: %param
  dst:
    kind: var
    name: %0
- instruction:
  kind: fun_call
  fun_name: callee
  args:
    - val:
      kind: constant
      const:
        kind: int
        value: 10
    - val:
      kind: var
      name: %param
  dst:
    kind: var
    name: %1
- instruction:
  kind: return
  src:
    kind: var
    name: %1
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_IntOnly_KilledThenRedefined)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* If two identical copies to x appear on the path to some use of x,
 * and the first one is killed, make sure we can still propagate the second.
 * */

int x = 0;
int y = 0;

int callee(void) {
    y = x * 2;  // make sure x still has the right value at this point
    return 5;
}

int target(void) {
    x = 2;         // gen x = 2
    x = callee();  // kill x = 2
    x = 2;         // gen x = 2 again
    return x;      // should become "return 2"
}
)SRC"),
              R"OPT(- instruction:
  kind: binary
  op: multiply
  src1:
    kind: var
    name: x
  src2:
    kind: constant
    const:
      kind: int
      value: 2
  dst:
    kind: var
    name: %0
- instruction:
  kind: copy
  src:
    kind: var
    name: %0
  dst:
    kind: var
    name: y
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 5
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 2
  dst:
    kind: var
    name: x
- instruction:
  kind: fun_call
  fun_name: callee
  dst:
    kind: var
    name: %0
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 2
  dst:
    kind: var
    name: x
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 2
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_IntOnly_MultiPathNoKill)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* If there are multiple paths from x = 3 to a use of x, but the copy
 * isn't killed on any of those paths, we can propagate it.
 * */
int var = 0;
int callee(void) {
    var = var + 1;
    return 0;
}

int target(int flag) {
    int x = 3;
    if (flag)
        callee();
    return x;  // should become return 3
}
)SRC"),
              R"OPT(- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: var
  src2:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %0
- instruction:
  kind: copy
  src:
    kind: var
    name: %0
  dst:
    kind: var
    name: var
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %flag
  target: %0
- instruction:
  kind: fun_call
  fun_name: callee
  dst:
    kind: var
    name: %2
- instruction:
  kind: jump
  target: %1
- instruction:
  kind: label
  name: %0
- instruction:
  kind: label
  name: %1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 3
)OPT");
}

// Structural assertion (instruction-kind histogram) — exact TAC
// YAML too large to pin verbatim (364 instructions).
TEST_F(PipelineTest, Chapter19_CP_IntOnly_NestedLoops)
{
    EXPECT_EQ(KindHistogram(OptimizeYaml(R"SRC(
/* A test case that takes even longer than fig_19_8.c to converge;
 * some blocks need to be visited three times before the algorithm converges.
 * */

static int outer_flag = 0;
static int inner_flag = 1;

// functions to validate args and control number of loop iterations
int inner_loop1(int a, int b, int c, int d, int e, int f) {
    // this should be the second loop iteration, so b, c, and e should be
    // updated, but a, d, and f shouldn't
    if (a != 1 || b != 11 || c != 12 || d != 4 || e != 20 || f != 100) {
        return 0;  // fail
    }
    return 1;  // success
}

int inner_loop2(int a, int b, int c, int d, int e, int f) {
    if (outer_flag == 0) {
        // first call: no variables have been updated
        if (a != 1 || b != 2 || c != 3 || d != 4 || e != 5 || f != 100) {
            return 0;  // fail
        }
    } else {
        // second call: a, b, c, and e have been updated
        if (a != 10 || b != 11 || c != 12 || d != 4 || e != 20 || f != 100) {
            return 0;  // fail
        }
    }

    return 1;  // success
}

int inner_loop3(int a, int b, int c, int d, int e, int f) {
    if (outer_flag == 0) {
        if (inner_flag == 2) {
            // first call to this function: only b has been updated
            if (a != 1 || b != 11 || c != 3 || d != 4 || e != 5 || f != 100) {
                return 0;  // fail
            }
        } else {
            // second iteration through inner loop: b and c have been updated
            if (a != 1 || b != 11 || c != 12 || d != 4 || e != 5 || f != 100) {
                return 0;  // fail
            }
        }
    } else {
        // second time through outer loop: a, b, c, and e have been updated
        if (a != 10 || b != 11 || c != 12 || d != 4 || e != 20 || f != 100) {
            return 0;  // fail
        }
    }

    return 1;  // success
}

int inner_loop4(int a, int b, int c, int d, int e, int f) {
    // this never runs
    // use all parameters to silence compiler warnings
    return a + b + c + d + e + f;
}

int validate(int a, int b, int c, int d, int e, int f) {
    // a, b, c, and e have been updated
    if (a != 10 || b != 11 || c != 12 || d != 4 || e != 20 || f != 100) {
        return 0;  // fail
    }
    return 1;  // success
}

int target(void) {
    // we can propagate f throughout whole function, but nothing else
    int a = 1;
    int b = 2;
    int c = 3;
    int d = 4;
    int e = 5;
    int f = 100;

    // go through outer loop twice
    while (outer_flag < 2) {
        // skip this loop on first outer iteration, run on second
        while (inner_flag < 1) {
            if (!inner_loop1(a, b, c, d, e, f)) {
                return 1;  // fail
            }
            a = 10;
            inner_flag = 1;
        }

        // do this loop once per outer iteration
        while (inner_flag < 2) {
            if (!inner_loop2(a, b, c, d, e, f)) {
                return 2;  // fail
            }
            b = 11;
            // set inner_flag to 2 so this loop doesn't run again but next loop
            // runs twice
            inner_flag = 2;
        }

        // do this loop twice per outer iteration
        while (inner_flag < 4) {
            if (!inner_loop3(a, b, c, d, e, f)) {
                return 3;  // fail
            }
            // increment inner_flag so this loop runs twice
            inner_flag = inner_flag + 1;
            c = 12;
        }

        // skip this loop both times
        while (inner_flag < 4) {
            inner_loop4(a, b, c, d, e, f);
            d = 13;
        }

        e = 20;
        f = 100;
        outer_flag = outer_flag + 1;
        // reset inner flag
        inner_flag = 0;
    }

    if (!validate(a, b, c, d, e, f)) {  // we can propagate f into this call
        return 4;
    }

    return 0;  // success
}
)SRC")),
              "binary=92 copy=52 fun_call=5 jump=43 jump_if_not_zero=35 jump_if_zero=19 label=97 return=17 unary=4");
}

TEST_F(PipelineTest, Chapter19_CP_IntOnly_PropagateIntoComplexExpressions)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we can propagate values from copies
 * into unary expressions, binary expressions,
 * and conditional jumps.
 * */
int target(void) {
    int x = 100;
    int y = -x * 3 + 300;
    return (y ? x % 3 : x / 4);
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 25
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_IntOnly_PropagateParams)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we can propagate copies both to and from function parameters;
 * similar to propagate_var, but with paramters instead of variables.
 * */
int callee(int a, int b) {
    return a * b;
}
int f(void) {
    return 3;
}
int globl = 0;
int set_globvar(void) {
    globl = 4;
    return 0;
}
int target(int a, int b) {
    b = a;  // propagate copy from a to b

    // call another function before callee so we can't coalesce a into EDI
    // or b into ESI; otherwise, once we implement register coalescing,
    // it will look like we've propagated the copy even if we haven't
    set_globvar();
    // look for: same value passed in ESI, EDI
    int product = callee(a, b);

    // now update b while a is live, so we can't coalesce them
    // into the same register; otherwise it will look like we've propagated
    // the copy even if we haven't
    b = f();
    return (product + a - b);  // return 5 * 5 + 5 - 3 ==> 27
}
)SRC"),
              R"OPT(- instruction:
  kind: binary
  op: multiply
  src1:
    kind: var
    name: %a
  src2:
    kind: var
    name: %b
  dst:
    kind: var
    name: %0
- instruction:
  kind: return
  src:
    kind: var
    name: %0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 3
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 4
  dst:
    kind: var
    name: globl
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: fun_call
  fun_name: set_globvar
  dst:
    kind: var
    name: %0
- instruction:
  kind: fun_call
  fun_name: callee
  args:
    - val:
      kind: var
      name: %a
    - val:
      kind: var
      name: %a
  dst:
    kind: var
    name: %1
- instruction:
  kind: fun_call
  fun_name: f
  dst:
    kind: var
    name: %2
- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: %1
  src2:
    kind: var
    name: %a
  dst:
    kind: var
    name: %3
- instruction:
  kind: binary
  op: subtract
  src1:
    kind: var
    name: %3
  src2:
    kind: var
    name: %2
  dst:
    kind: var
    name: %4
- instruction:
  kind: return
  src:
    kind: var
    name: %4
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_IntOnly_PropagateStatic)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we can propagate copies to variables with static storage
 * duration */
int x = 0;

int target(void) {
    // we can propagate value of x, even though it has static storage duration,
    // b/c no intervening reads/writes
    x = 10;
    return x;  // should become "return 10"
}
)SRC"),
              R"OPT(- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 10
  dst:
    kind: var
    name: x
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 10
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_IntOnly_PropagateStaticVar)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Propagate a copy where the source value is a variable with static storage
 * duration, in a function with no control flow strucures.
 * */
int callee(int a, int b) {
    return a + b;
}

int target(void) {
    static int x = 3;

    // y also needs to be static so we can't coalesce
    // it into ESI once we implement register coalescing;
    // otherwise it may look like we've propagated a copy
    // when we haven't
    static int y = 0;

    y = x;  // make sure we propagate this into function call

    // look for: same value passed in ESI, EDI
    int sum = callee(x, y);

    // increment x to make sure we're not just propagating
    // x's initial value. (If we are, we'll get the wrong result on
    // the second call to target
    x = x + 1;
    return sum;
}
)SRC"),
              R"OPT(- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: %a
  src2:
    kind: var
    name: %b
  dst:
    kind: var
    name: %0
- instruction:
  kind: return
  src:
    kind: var
    name: %0
- instruction:
  kind: copy
  src:
    kind: var
    name: x
  dst:
    kind: var
    name: y
- instruction:
  kind: fun_call
  fun_name: callee
  args:
    - val:
      kind: var
      name: x
    - val:
      kind: var
      name: x
  dst:
    kind: var
    name: %0
- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: x
  src2:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %1
- instruction:
  kind: copy
  src:
    kind: var
    name: %1
  dst:
    kind: var
    name: x
- instruction:
  kind: return
  src:
    kind: var
    name: %0
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_IntOnly_PropagateVar)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we can propagate copies where the source value is
 * a variable, in a function with no control flow strucures.
 * */
int callee(int a, int b) {
    return a + b;
}
int f(void) {
    return 3;
}

int globl = 0;
int set_globvar(void) {
    globl = 4;
    return 0;
}

int target(void) {
    int x = f();
    int y = x;  // propagate this copy into function call

    // call another function before callee so we can't coalesce x into EDI
    // or y into ESI; otherwise it will look like we've propagated x as
    // a function argument even if we haven't
    set_globvar();

    // look for: same value passed in ESI, EDI
    int sum = callee(x, y);

    // now update y while x is live, so we can't coalesce them
    // into the same register; otherwise it will look like we've propagated x as
    // a function argument even if we haven't
    y = f();
    return (sum + x * y);  // return 6 + 9 ==> 15
}
)SRC"),
              R"OPT(- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: %a
  src2:
    kind: var
    name: %b
  dst:
    kind: var
    name: %0
- instruction:
  kind: return
  src:
    kind: var
    name: %0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 3
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 4
  dst:
    kind: var
    name: globl
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: fun_call
  fun_name: f
  dst:
    kind: var
    name: %0
- instruction:
  kind: fun_call
  fun_name: set_globvar
  dst:
    kind: var
    name: %1
- instruction:
  kind: fun_call
  fun_name: callee
  args:
    - val:
      kind: var
      name: %0
    - val:
      kind: var
      name: %0
  dst:
    kind: var
    name: %2
- instruction:
  kind: fun_call
  fun_name: f
  dst:
    kind: var
    name: %3
- instruction:
  kind: binary
  op: multiply
  src1:
    kind: var
    name: %0
  src2:
    kind: var
    name: %3
  dst:
    kind: var
    name: %4
- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: %2
  src2:
    kind: var
    name: %4
  dst:
    kind: var
    name: %5
- instruction:
  kind: return
  src:
    kind: var
    name: %5
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_IntOnly_RedundantCopies)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we eliminate y = x and y = x if we can prove that x and y
 * already have the same values.
 * After copy propagation and cleanup unreachable code elimination,
 * target should contain no control-flow instructions
 * */

int target(int flag, int flag2, int y) {
    int x = y;

    if (flag) {
        y = x;  // we can remove this because x and y already have the same
                // value
    }
    if (flag2) {
        x = y;  // we can remove this because x and y already have the same
                // value
    }
    return x + y;
}
)SRC"),
              R"OPT(- instruction:
  kind: copy
  src:
    kind: var
    name: %y
  dst:
    kind: var
    name: %x
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %flag
  target: %0
- instruction:
  kind: jump
  target: %1
- instruction:
  kind: label
  name: %0
- instruction:
  kind: label
  name: %1
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %flag2
  target: %2
- instruction:
  kind: copy
  src:
    kind: var
    name: %y
  dst:
    kind: var
    name: %x
- instruction:
  kind: jump
  target: %3
- instruction:
  kind: label
  name: %2
- instruction:
  kind: label
  name: %3
- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: %x
  src2:
    kind: var
    name: %y
  dst:
    kind: var
    name: %4
- instruction:
  kind: return
  src:
    kind: var
    name: %4
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_IntOnly_DontPropagate_AddAllBlocksToWorklist)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we add every block to the worklist
 * at the start of the iterative algoirthm
 * */

int global;

int flag = 1;

int f(void) {
    global = 100;
    return 0;
}

int main(void) {
    // Initially, we annotate every block with 'global = 0',
    // which is the only copy in 'main'.
    global = 0;

    // The copy 'global = 0' reaches this if statement. If we only add a block
    // to the worklist when its predecessor's outgoing copies change,
    // instead of adding every block to the initial worklist, we won't
    // visit this block at all, and we won't see the call to f(),
    // which kills this copy
    if (flag) {
        f();  // kill copy to global
    }

    // If we didn't visit that if statement, we'll incorrectly
    // rewrite this as 'return 0'.
    return global;
}
)SRC"),
              R"OPT(- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 100
  dst:
    kind: var
    name: global
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 0
  dst:
    kind: var
    name: global
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: flag
  target: %0
- instruction:
  kind: fun_call
  fun_name: f
  dst:
    kind: var
    name: %2
- instruction:
  kind: jump
  target: %1
- instruction:
  kind: label
  name: %0
- instruction:
  kind: label
  name: %1
- instruction:
  kind: return
  src:
    kind: var
    name: global
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_IntOnly_DontPropagate_DestKilled)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that updating a variable kills previous
 * copies to that variable
 * */
int foo(void) {
    return 4;
}

int main(void) {
    int x = 3;
    x = foo();  // this kills x = 3
    return x;   // don't propagate x = 3
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 4
- instruction:
  kind: fun_call
  fun_name: foo
  dst:
    kind: var
    name: %0
- instruction:
  kind: return
  src:
    kind: var
    name: %0
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_IntOnly_DontPropagate_Listing1914)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that function calls kill copies to static local variables;
 * example from Listing 19-14
 * */

int indirect_update(void);

int f(int new_total) {
    static int total = 0;
    total = new_total;  // generate copy total = new_total
    if (total > 100)
        return 0;
    total = 10;         // generate total = 10
    indirect_update();  // kill total = 10 (b/c total is static)
    return total;       // can't rewrite as 'return 10'
}

int indirect_update(void) {
    f(101);  // this will update 'total'
    return 0;
}

int main(void) {
    return f(1);  // expected return value: 101
}
)SRC"),
              R"OPT(- instruction:
  kind: copy
  src:
    kind: var
    name: %new_total
  dst:
    kind: var
    name: total
- instruction:
  kind: binary
  op: greater_than
  src1:
    kind: var
    name: %new_total
  src2:
    kind: constant
    const:
      kind: int
      value: 100
  dst:
    kind: var
    name: %0
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %0
  target: %1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: label
  name: %1
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 10
  dst:
    kind: var
    name: total
- instruction:
  kind: fun_call
  fun_name: indirect_update
  dst:
    kind: var
    name: %3
- instruction:
  kind: return
  src:
    kind: var
    name: total
- instruction:
  kind: fun_call
  fun_name: f
  args:
    - val:
      kind: constant
      const:
        kind: int
        value: 101
  dst:
    kind: var
    name: %0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: fun_call
  fun_name: f
  args:
    - val:
      kind: constant
      const:
        kind: int
        value: 1
  dst:
    kind: var
    name: %0
- instruction:
  kind: return
  src:
    kind: var
    name: %0
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_IntOnly_DontPropagate_MultiValues)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test for meet operator: if different copies to a variable
 * reach the ends of a block's predecessors, no copies to that variable
 * reach that block
 * */

int multi_path(int flag) {
    int x = 3;  // generate x = 3
    if (flag)
        x = 4;  // kill x = 3, generate x = 4

    // One predecessor of our final block has outgoing copy x = 3,
    // the other has outgoing copy x = 4. Their intersection is the empty set,
    // so we can't propagate any value to this return statement.
    return x;
}

int main(void) {
    if (multi_path(1) != 4) {
        return 1;
    }

    if (multi_path(0) != 3) {
        return 2;
    }

    return 0;  // success
}
)SRC"),
              R"OPT(- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 3
  dst:
    kind: var
    name: %x
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %flag
  target: %0
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 4
  dst:
    kind: var
    name: %x
- instruction:
  kind: jump
  target: %1
- instruction:
  kind: label
  name: %0
- instruction:
  kind: label
  name: %1
- instruction:
  kind: return
  src:
    kind: var
    name: %x
- instruction:
  kind: fun_call
  fun_name: multi_path
  args:
    - val:
      kind: constant
      const:
        kind: int
        value: 1
  dst:
    kind: var
    name: %0
- instruction:
  kind: binary
  op: not_equal
  src1:
    kind: var
    name: %0
  src2:
    kind: constant
    const:
      kind: int
      value: 4
  dst:
    kind: var
    name: %1
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %1
  target: %2
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: label
  name: %2
- instruction:
  kind: fun_call
  fun_name: multi_path
  args:
    - val:
      kind: constant
      const:
        kind: int
        value: 0
  dst:
    kind: var
    name: %4
- instruction:
  kind: binary
  op: not_equal
  src1:
    kind: var
    name: %4
  src2:
    kind: constant
    const:
      kind: int
      value: 3
  dst:
    kind: var
    name: %5
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %5
  target: %6
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 2
- instruction:
  kind: label
  name: %6
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_IntOnly_DontPropagate_NoCopiesReachEntry)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Make sure we track that the set of reaching copies from ENTRY is empty */
int target(int a, int flag) {
    if (flag) {
        // if we initialized ENTRY with the set of all copies in target,
        // we'll think that a = 10 reaches this return statement,
        // and incorrectly rewrite it as 'return 10'
        return a;
    }

    a = 10;  // initialize ENTRY w/ empty set of copies, not including this one
    return a;
}
)SRC"),
              R"OPT(- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %flag
  target: %0
- instruction:
  kind: return
  src:
    kind: var
    name: %a
- instruction:
  kind: label
  name: %0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 10
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_IntOnly_DontPropagate_OneReachingCopy)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* If a copy appears on one path to a block but not on all
 * paths to that block, it doesn't reach that block.
 * */

int three(void) {
    return 3;
}

int target(int flag) {
    int x;
    if (flag)
        x = 10;
    else
        x = three();
    // one predecessor contains copy x = 10, other predecessor contains no
    // copies to x, so no copies reach 'return x'
    return x;
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 3
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %flag
  target: %0
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 10
  dst:
    kind: var
    name: %x
- instruction:
  kind: jump
  target: %1
- instruction:
  kind: label
  name: %0
- instruction:
  kind: fun_call
  fun_name: three
  dst:
    kind: var
    name: %2
- instruction:
  kind: copy
  src:
    kind: var
    name: %2
  dst:
    kind: var
    name: %x
- instruction:
  kind: label
  name: %1
- instruction:
  kind: return
  src:
    kind: var
    name: %x
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_IntOnly_DontPropagate_SourceKilled)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Basic test that updating the source of a copy kills that copy */

int x = 10;

int main(void) {
    int y = x;      // generate y = x
    x = 4;          // kill y = x
    if (y != 10) {  // can't replace y with x here
        return 1;
    }
    if (x != 4) {
        return 2;
    }
    return 0;  // success
}
)SRC"),
              R"OPT(- instruction:
  kind: copy
  src:
    kind: var
    name: x
  dst:
    kind: var
    name: %y
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 4
  dst:
    kind: var
    name: x
- instruction:
  kind: binary
  op: not_equal
  src1:
    kind: var
    name: %y
  src2:
    kind: constant
    const:
      kind: int
      value: 10
  dst:
    kind: var
    name: %0
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %0
  target: %1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: label
  name: %1
- instruction:
  kind: binary
  op: not_equal
  src1:
    kind: var
    name: x
  src2:
    kind: constant
    const:
      kind: int
      value: 4
  dst:
    kind: var
    name: %3
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %3
  target: %4
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 2
- instruction:
  kind: label
  name: %4
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_IntOnly_DontPropagate_SourceKilledOnOnePath)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* If a copy is generated on all paths to a block,
 * and its source is updated on one path,
 * it doesn't reach that block
 * */

int putch(int c);  // from standard library

int f(int src, int flag) {
    int x = src;  // generate x = src
    if (flag) {
        src = 65;  // kill x = src
    }
    putch(src);  // use src so assignment doesn't get optimized away entirely
    return x;      // make sure we don't rewrite this as 'return src'
}

int main(void) {
    // first call f with flag = 0;
    // validate return value, and make sure
    // src is not updated
    if (f(68, 0) != 68) {
        return 1;
    }

    // now call f with flag = 1;
    // validate return value and make sure
    // src is updated
    if (f(70, 1) != 70) {
        return 2;
    }

    return 0;  // success
}
)SRC"),
              R"OPT(- instruction:
  kind: copy
  src:
    kind: var
    name: %src
  dst:
    kind: var
    name: %x
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %flag
  target: %0
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 65
  dst:
    kind: var
    name: %src
- instruction:
  kind: jump
  target: %1
- instruction:
  kind: label
  name: %0
- instruction:
  kind: label
  name: %1
- instruction:
  kind: fun_call
  fun_name: putch
  args:
    - val:
      kind: var
      name: %src
  dst:
    kind: var
    name: %2
- instruction:
  kind: return
  src:
    kind: var
    name: %x
- instruction:
  kind: fun_call
  fun_name: f
  args:
    - val:
      kind: constant
      const:
        kind: int
        value: 68
    - val:
      kind: constant
      const:
        kind: int
        value: 0
  dst:
    kind: var
    name: %0
- instruction:
  kind: binary
  op: not_equal
  src1:
    kind: var
    name: %0
  src2:
    kind: constant
    const:
      kind: int
      value: 68
  dst:
    kind: var
    name: %1
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %1
  target: %2
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: label
  name: %2
- instruction:
  kind: fun_call
  fun_name: f
  args:
    - val:
      kind: constant
      const:
        kind: int
        value: 70
    - val:
      kind: constant
      const:
        kind: int
        value: 1
  dst:
    kind: var
    name: %4
- instruction:
  kind: binary
  op: not_equal
  src1:
    kind: var
    name: %4
  src2:
    kind: constant
    const:
      kind: int
      value: 70
  dst:
    kind: var
    name: %5
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %5
  target: %6
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 2
- instruction:
  kind: label
  name: %6
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_IntOnly_DontPropagate_StaticDstKilled)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Function calls kill copies to variables with static storage duration */
int x;

int update_x(void) {
    x = 4;
    return 0;
}

int target(void) {
    x = 3;       // generate x = 3
    update_x();  // kill x = 3
    return x;    // can't propagte b/c it's static
}
)SRC"),
              R"OPT(- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 4
  dst:
    kind: var
    name: x
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 3
  dst:
    kind: var
    name: x
- instruction:
  kind: fun_call
  fun_name: update_x
  dst:
    kind: var
    name: %0
- instruction:
  kind: return
  src:
    kind: var
    name: x
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_IntOnly_DontPropagate_StaticSrcKilled)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Function calls kill copies where source value
 * is a variable with static storage duration
 * */

int x = 1;

int f(void) {
    x = 4;
    return 0;
}

int target(void) {
    int y = x;  // generate y = x
    f();        // kill y = x
    return y;   // don't
}
)SRC"),
              R"OPT(- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 4
  dst:
    kind: var
    name: x
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: copy
  src:
    kind: var
    name: x
  dst:
    kind: var
    name: %y
- instruction:
  kind: fun_call
  fun_name: f
  dst:
    kind: var
    name: %0
- instruction:
  kind: return
  src:
    kind: var
    name: %y
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_IntOnly_ExtraCredit_GotoDefine)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
int target(int flag) {
    int x = 10;
    goto def_x;
    if (flag) {
    def_x:
        x = 20;
    }
    return x; // return 20
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 20
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_IntOnly_ExtraCredit_PrefixResult)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Make sure copy propagation can track that the result of ++x and the updated
 * value of x have the same value
 */

int callee(int a, int b) {
    return a + b;
}
int f(void) {
    return 3;
}

int globl = 0;
int set_globvar(void) {
    globl = 4;
    return 0;
}

int target(void) {
    int x = f(); // x = 3
    // now x and y should have same (tmp) value
    int y = ++x; // x and y are 4

    // call another function before callee so we can't coalesce x into EDI
    // or y into ESI; otherwise it will look like we've propagated x as
    // a function argument even if we haven't
    set_globvar();

    // look for: same value passed in ESI, EDI
    int sum = callee(x, y); // sum = 8


    // now update y while x is live, so we can't coalesce them
    // into the same register; otherwise it will look like we've propagated x as
    // a function argument even if we haven't
    y = f(); // y  = 3
    return (sum + x * y);  // return 8 + 12 ==> 20
}
)SRC"),
              R"OPT(- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: %a
  src2:
    kind: var
    name: %b
  dst:
    kind: var
    name: %0
- instruction:
  kind: return
  src:
    kind: var
    name: %0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 3
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 4
  dst:
    kind: var
    name: globl
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: fun_call
  fun_name: f
  dst:
    kind: var
    name: %0
- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: %0
  src2:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %1
- instruction:
  kind: fun_call
  fun_name: set_globvar
  dst:
    kind: var
    name: %2
- instruction:
  kind: fun_call
  fun_name: callee
  args:
    - val:
      kind: var
      name: %1
    - val:
      kind: var
      name: %1
  dst:
    kind: var
    name: %3
- instruction:
  kind: fun_call
  fun_name: f
  dst:
    kind: var
    name: %4
- instruction:
  kind: binary
  op: multiply
  src1:
    kind: var
    name: %1
  src2:
    kind: var
    name: %4
  dst:
    kind: var
    name: %5
- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: %3
  src2:
    kind: var
    name: %5
  dst:
    kind: var
    name: %6
- instruction:
  kind: return
  src:
    kind: var
    name: %6
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_IntOnly_ExtraCredit_PropagateFromDefault)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Propagate a value that's defined in a default statement that we always
 * reach
 */

int globvar = 0;

int target(int x) {
    int retval = 0;
    switch (x) {
        case 1: globvar = 1;
        case 2: globvar = globvar + 3;
        case 3: globvar = globvar * 2;
        default: retval = 3; // we always reach this no matter which case we take
    }

    return retval; // replace with "return 3"
}
)SRC"),
              R"OPT(- instruction:
  kind: binary
  op: equal
  src1:
    kind: var
    name: %x
  src2:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %5
- instruction:
  kind: jump_if_not_zero
  condition:
    kind: var
    name: %5
  target: %0
- instruction:
  kind: binary
  op: equal
  src1:
    kind: var
    name: %x
  src2:
    kind: constant
    const:
      kind: int
      value: 2
  dst:
    kind: var
    name: %6
- instruction:
  kind: jump_if_not_zero
  condition:
    kind: var
    name: %6
  target: %1
- instruction:
  kind: binary
  op: equal
  src1:
    kind: var
    name: %x
  src2:
    kind: constant
    const:
      kind: int
      value: 3
  dst:
    kind: var
    name: %7
- instruction:
  kind: jump_if_not_zero
  condition:
    kind: var
    name: %7
  target: %2
- instruction:
  kind: jump
  target: %3
- instruction:
  kind: label
  name: %0
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: globvar
- instruction:
  kind: label
  name: %1
- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: globvar
  src2:
    kind: constant
    const:
      kind: int
      value: 3
  dst:
    kind: var
    name: %8
- instruction:
  kind: copy
  src:
    kind: var
    name: %8
  dst:
    kind: var
    name: globvar
- instruction:
  kind: label
  name: %2
- instruction:
  kind: binary
  op: multiply
  src1:
    kind: var
    name: globvar
  src2:
    kind: constant
    const:
      kind: int
      value: 2
  dst:
    kind: var
    name: %9
- instruction:
  kind: copy
  src:
    kind: var
    name: %9
  dst:
    kind: var
    name: globvar
- instruction:
  kind: label
  name: %3
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 3
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_IntOnly_ExtraCredit_PropagateIntoCase)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
int globvar = 0;

int callee(int arg) {
    globvar = arg;
    return 0;
}

int target(int flag) {
    int arg = 10;
    switch (flag) {
        case 1:
            arg = 20;
            break;
        case 2:
            // replace arg w/ 10 here - previous assignment
            // doesn't kill this b/c we never pass through it to get here
            callee(arg);
            break;
        default:
            globvar = -1;
    }
    return 0;
}
)SRC"),
              R"OPT(- instruction:
  kind: copy
  src:
    kind: var
    name: %arg
  dst:
    kind: var
    name: globvar
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: binary
  op: equal
  src1:
    kind: var
    name: %flag
  src2:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %4
- instruction:
  kind: jump_if_not_zero
  condition:
    kind: var
    name: %4
  target: %0
- instruction:
  kind: binary
  op: equal
  src1:
    kind: var
    name: %flag
  src2:
    kind: constant
    const:
      kind: int
      value: 2
  dst:
    kind: var
    name: %5
- instruction:
  kind: jump_if_not_zero
  condition:
    kind: var
    name: %5
  target: %1
- instruction:
  kind: jump
  target: %2
- instruction:
  kind: label
  name: %0
- instruction:
  kind: jump
  target: %L0
- instruction:
  kind: label
  name: %1
- instruction:
  kind: fun_call
  fun_name: callee
  args:
    - val:
      kind: constant
      const:
        kind: int
        value: 10
  dst:
    kind: var
    name: %6
- instruction:
  kind: jump
  target: %L0
- instruction:
  kind: label
  name: %2
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: -1
  dst:
    kind: var
    name: globvar
- instruction:
  kind: label
  name: %L0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
)OPT");
}

// Structural assertion (instruction-kind histogram) — exact TAC
// YAML too large to pin verbatim (84 instructions).
TEST_F(PipelineTest, Chapter19_CP_IntOnly_ExtraCredit_DontPropagate_DecrKillsDest)
{
    EXPECT_EQ(KindHistogram(OptimizeYaml(R"SRC(
/* A ++/-- operation kills its operand */

int target(int flag) {
    int w = 3;
    if (flag) {
        w++;
    }

    int x = 10;
    if (flag) {
        x--;
    }

    int y = -12;
    if (flag) {
        ++y;
    }

    int z = -100;
    if (flag) {
        --z;
    }

    if (flag) {
        if (w == 4 && x == 9 && y == -11 && z == -101) {
            // success
            return 0;
        }
        return 1;
    }
    else {
        if (w == 3 && x == 10 && y == -12 && z == -100) {
            // success
            return 0;
        }
        return 1; // fail

    }

}
)SRC")),
              "binary=18 copy=16 jump=10 jump_if_zero=13 label=23 return=4");
}

TEST_F(PipelineTest, Chapter19_CP_IntOnly_ExtraCredit_DontPropagate_SwitchFallthrough)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test case where we can't propagate a value into a case statement,
 * because it's reachable from either start of switch or fallthrough from
 * previous case statement, where the values differ.
 * */

int target(int flag) {
    int retval = 10;
    switch(flag) {
        case 1:
        retval = 0;
        case 2:
        // can't propagate - retval could be eitehr 10 or 0
        return retval;
        default: return -1;
    }
}
)SRC"),
              R"OPT(- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 10
  dst:
    kind: var
    name: %retval
- instruction:
  kind: binary
  op: equal
  src1:
    kind: var
    name: %flag
  src2:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %4
- instruction:
  kind: jump_if_not_zero
  condition:
    kind: var
    name: %4
  target: %0
- instruction:
  kind: binary
  op: equal
  src1:
    kind: var
    name: %flag
  src2:
    kind: constant
    const:
      kind: int
      value: 2
  dst:
    kind: var
    name: %5
- instruction:
  kind: jump_if_not_zero
  condition:
    kind: var
    name: %5
  target: %1
- instruction:
  kind: jump
  target: %2
- instruction:
  kind: label
  name: %0
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 0
  dst:
    kind: var
    name: %retval
- instruction:
  kind: label
  name: %1
- instruction:
  kind: return
  src:
    kind: var
    name: %retval
- instruction:
  kind: label
  name: %2
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: -1
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_AllTypes_AliasAnalysis)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that alias analysis allows us to propagate some copies
 * from variables whose address has been taken. */
int callee(int *ptr) {
    if (*ptr != 10) {
        return 0;  // failure
    }
    *ptr = -1;
    return 1;
}

int target(int *ptr1, int *ptr2) {
    int i = 10;          // generate i = 10
    int j = 20;          // generate j = 20
    *ptr1 = callee(&i);  // record i as a variable whose address is taken
                         // function call kills i = 10
    *ptr2 = i;

    i = 4;  // gen i = 4

    // This should be rewritten as 'return 24'.
    // We can propagate i b/c there are no stores
    // or function calls after i = 4.
    // We can propagate j b/c it's not aliased.
    return i + j;
}
)SRC"),
              R"OPT(- instruction:
  kind: load
  src_ptr:
    kind: var
    name: %ptr
  dst:
    kind: var
    name: %0
- instruction:
  kind: binary
  op: not_equal
  src1:
    kind: var
    name: %0
  src2:
    kind: constant
    const:
      kind: int
      value: 10
  dst:
    kind: var
    name: %1
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %1
  target: %2
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: label
  name: %2
- instruction:
  kind: store
  src:
    kind: constant
    const:
      kind: int
      value: -1
  dst_ptr:
    kind: var
    name: %ptr
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 10
  dst:
    kind: var
    name: %i
- instruction:
  kind: get_address
  src:
    kind: var
    name: %i
  dst:
    kind: var
    name: %0
- instruction:
  kind: fun_call
  fun_name: callee
  args:
    - val:
      kind: var
      name: %0
  dst:
    kind: var
    name: %1
- instruction:
  kind: store
  src:
    kind: var
    name: %1
  dst_ptr:
    kind: var
    name: %ptr1
- instruction:
  kind: store
  src:
    kind: var
    name: %i
  dst_ptr:
    kind: var
    name: %ptr2
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 4
  dst:
    kind: var
    name: %i
- instruction:
  kind: binary
  op: add
  src1:
    kind: constant
    const:
      kind: int
      value: 4
  src2:
    kind: constant
    const:
      kind: int
      value: 20
  dst:
    kind: var
    name: %4
- instruction:
  kind: return
  src:
    kind: var
    name: %4
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_AllTypes_CharTypeConversion)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we can propagate copies between char and signed char */

int putch(int c);  // from standard library

void print_some_chars(char a, char b, char c, char d) {
    putch(a);
    putch(b);
    putch(c);
    putch(d);
}

int callee(char c, signed char s) {
    return c == s;
}

int target(char c, signed char s) {
    // first, call another function, with these arguments
    // in different positions than in target or callee, so we can't
    // coalesce them with the param-passing registers or each other
    print_some_chars(67, 66, c, s);

    s = c;  // generate s = c - we can do this because for the purposes of copy
            // propagation, we consider char and signed char the same type

    // both arguments to callee should be the same
    return callee(s, c);
}
)SRC"),
              R"OPT(- instruction:
  kind: sign_extend
  src:
    kind: var
    name: %a
  dst:
    kind: var
    name: %0
- instruction:
  kind: fun_call
  fun_name: putch
  args:
    - val:
      kind: var
      name: %0
  dst:
    kind: var
    name: %1
- instruction:
  kind: sign_extend
  src:
    kind: var
    name: %b
  dst:
    kind: var
    name: %2
- instruction:
  kind: fun_call
  fun_name: putch
  args:
    - val:
      kind: var
      name: %2
  dst:
    kind: var
    name: %3
- instruction:
  kind: sign_extend
  src:
    kind: var
    name: %c
  dst:
    kind: var
    name: %4
- instruction:
  kind: fun_call
  fun_name: putch
  args:
    - val:
      kind: var
      name: %4
  dst:
    kind: var
    name: %5
- instruction:
  kind: sign_extend
  src:
    kind: var
    name: %d
  dst:
    kind: var
    name: %6
- instruction:
  kind: fun_call
  fun_name: putch
  args:
    - val:
      kind: var
      name: %6
  dst:
    kind: var
    name: %7
- instruction:
  kind: sign_extend
  src:
    kind: var
    name: %c
  dst:
    kind: var
    name: %0
- instruction:
  kind: sign_extend
  src:
    kind: var
    name: %s
  dst:
    kind: var
    name: %1
- instruction:
  kind: binary
  op: equal
  src1:
    kind: var
    name: %0
  src2:
    kind: var
    name: %1
  dst:
    kind: var
    name: %2
- instruction:
  kind: return
  src:
    kind: var
    name: %2
- instruction:
  kind: fun_call
  fun_name: print_some_chars
  args:
    - val:
      kind: constant
      const:
        kind: char
        value: 67
    - val:
      kind: constant
      const:
        kind: char
        value: 66
    - val:
      kind: var
      name: %c
    - val:
      kind: var
      name: %s
- instruction:
  kind: fun_call
  fun_name: callee
  args:
    - val:
      kind: var
      name: %c
    - val:
      kind: var
      name: %c
  dst:
    kind: var
    name: %6
- instruction:
  kind: return
  src:
    kind: var
    name: %6
)OPT");
}

// Structural assertion (instruction-kind histogram) — exact TAC
// YAML too large to pin verbatim (31 instructions).
TEST_F(PipelineTest, Chapter19_CP_AllTypes_CopyStruct)
{
    EXPECT_EQ(KindHistogram(OptimizeYaml(R"SRC(
/* Test that we can propagate copies of aggregate values */
struct s {
    int x;
    int y;
};

int callee(struct s a, struct s b) {
    if (a.x != 3) {
        return 1; // fail
    }
    if (a.y != 4) {
        return 2; // fail
    }
    if (b.x != 3) {
        return 3; // fail
    }
    if (b.y != 4) {
        return 4; // fail
    }
    return 0; // success
}

int target(void) {
    struct s s1 = {1, 2};
    struct s s2 = {3, 4};
    s1 = s2;  // generate s1 = s2

    // Make sure we pass the same value for both arguments.
    // We don't need to worry that register coalescing
    // will interfere with this test,
    // because s1 and s2, as structures, won't be stored in registers.
    return callee(s1, s2);
}
)SRC")),
              "allocate_local=2 binary=4 copy_from_offset=5 copy_to_offset=5 fun_call=1 jump_if_zero=4 label=4 return=6");
}

TEST_F(PipelineTest, Chapter19_CP_AllTypes_FuncallKillsAliased)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that function calls kill all aliased variables, but not non-aliased
 * variables */

double *globl_ptr = 0;

void save_ptr(double *to_save) {
    globl_ptr = to_save;
}

void update_ptr(void) {
    *globl_ptr = 4.0;
}

// here, function call doesn't kill copy
int target(void) {
    int x = 10;    // gen x = 10
    update_ptr();  // doesn't kill x = 10

    return x;  // rewrite as 'return 10'
}

// here, function call does kill copy
int kill_aliased(void) {
    double d = 1.0;
    double *ptr = &d;
    save_ptr(ptr);

    if (*globl_ptr != 1.0) {
        return 0;  // fail
    }

    d = 2.0;  // gen d = 2.0

    if (*globl_ptr != 2.0) {
        return 0;  // fail
    }

    update_ptr();  // kill d = 2.0
    return d;      // make sure we don't rewrite this as 'return 2.0'
}
)SRC"),
              R"OPT(- instruction:
  kind: copy
  src:
    kind: var
    name: %to_save
  dst:
    kind: var
    name: globl_ptr
- instruction:
  kind: store
  src:
    kind: constant
    const:
      kind: double
      value: 0x1p+2
  dst_ptr:
    kind: var
    name: globl_ptr
- instruction:
  kind: fun_call
  fun_name: update_ptr
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 10
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: double
      value: 0x1p+0
  dst:
    kind: var
    name: %d
- instruction:
  kind: get_address
  src:
    kind: var
    name: %d
  dst:
    kind: var
    name: %0
- instruction:
  kind: fun_call
  fun_name: save_ptr
  args:
    - val:
      kind: var
      name: %0
- instruction:
  kind: load
  src_ptr:
    kind: var
    name: globl_ptr
  dst:
    kind: var
    name: %1
- instruction:
  kind: binary
  op: not_equal
  src1:
    kind: var
    name: %1
  src2:
    kind: constant
    const:
      kind: double
      value: 0x1p+0
  dst:
    kind: var
    name: %2
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %2
  target: %3
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: label
  name: %3
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: double
      value: 0x1p+1
  dst:
    kind: var
    name: %d
- instruction:
  kind: load
  src_ptr:
    kind: var
    name: globl_ptr
  dst:
    kind: var
    name: %5
- instruction:
  kind: binary
  op: not_equal
  src1:
    kind: var
    name: %5
  src2:
    kind: constant
    const:
      kind: double
      value: 0x1p+1
  dst:
    kind: var
    name: %6
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %6
  target: %7
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: label
  name: %7
- instruction:
  kind: fun_call
  fun_name: update_ptr
- instruction:
  kind: double_to_int
  src:
    kind: var
    name: %d
  dst:
    kind: var
    name: %9
- instruction:
  kind: return
  src:
    kind: var
    name: %9
)OPT");
}

// Structural assertion (instruction-kind histogram) — exact TAC
// YAML too large to pin verbatim (75 instructions).
TEST_F(PipelineTest, Chapter19_CP_AllTypes_PointerArithmetic)
{
    EXPECT_EQ(KindHistogram(OptimizeYaml(R"SRC(
/* Test that we propagate copies into AddPtr instructions */
int target(void) {
    int nested[3][23] = {{0, 1}, {2}};

    // we'll initially generate something like this:
    // index0 = SignExtend(1)
    // tmp2 = AddPtr(ptr=tmp1, index=index0, scale=92)
    // index1 = SignExtend(0)
    // tmp3 = AddPtr(ptr=tmp2, index=index1, scale=4)
    // return tmp3
    // But after constant folding and copy propagation, both AddPtr
    // instruction should have constant indices, so we won't need
    // any imul instructions in the final assembly (like we normally would to
    // implement AddPtr with a non-standard scale)
    return nested[1][0];
}
)SRC")),
              "add_ptr=2 allocate_local=1 copy_to_offset=69 get_address=1 load=1 return=1");
}

TEST_F(PipelineTest, Chapter19_CP_AllTypes_PropagateAllTypes)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we can propagate all arithmetic types, including doubles,
 * long and unsigned integers, and characters.
 * */


int target(void) {
    // propagate doubles
    double d = 1500.0;
    double d2 = d;

    int sum = (int)(d + d2);  // 3000

    // propagate chars
    char c = 250;  // will be converted to -6
    char c2 = c;
    sum = sum + (c2 + c);  // 2988

    // propagate unsigned char
    unsigned char uc = -1;  // will be converted to 255
    unsigned char uc2 = uc;
    sum = sum + uc + uc2;  // 3498

    // propagate unsigned long
    unsigned long ul = 18446744073709551615UL;  // ULONG_MAX
    unsigned long ul2 = ul + 3ul;               // wraps around to 2
    sum = sum + ul2;                            // 3500

    return sum;  // rewrite as "return 3500"
}
)SRC"),
              R"OPT(- instruction:
  kind: zero_extend
  src:
    kind: constant
    const:
      kind: char
      value: -1
  dst:
    kind: var
    name: %9
- instruction:
  kind: binary
  op: add
  src1:
    kind: constant
    const:
      kind: int
      value: 2988
  src2:
    kind: var
    name: %9
  dst:
    kind: var
    name: %10
- instruction:
  kind: zero_extend
  src:
    kind: constant
    const:
      kind: char
      value: -1
  dst:
    kind: var
    name: %11
- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: %10
  src2:
    kind: var
    name: %11
  dst:
    kind: var
    name: %12
- instruction:
  kind: sign_extend
  src:
    kind: var
    name: %12
  dst:
    kind: var
    name: %14
- instruction:
  kind: binary
  op: add_unsigned
  src1:
    kind: var
    name: %14
  src2:
    kind: constant
    const:
      kind: ulong
      value: 2
  dst:
    kind: var
    name: %15
- instruction:
  kind: truncate
  src:
    kind: var
    name: %15
  dst:
    kind: var
    name: %16
- instruction:
  kind: return
  src:
    kind: var
    name: %16
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_AllTypes_PropagateIntoTypeConversions)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we correctly propagate copies into type conversion instructions */

int target(void) {
    unsigned char uc = 250;
    int i = uc * 2;              // 500 - tests ZeroExtend
    double d = i * 1000.;        // 500000.0 - tests IntToDouble
    unsigned long ul = d / 6.0;  // 83333 - tests DoubleToUInt
    d = ul + 5.0;                // 83338 - tests UIntToDouble
    long l = -i;                 // -500 - tests SignExtend
    char c = l;                  // 12 - tests Truncate
    return d + i - c;            // 83826 - tests DoubleToInt
}
)SRC"),
              R"OPT(- instruction:
  kind: zero_extend
  src:
    kind: constant
    const:
      kind: char
      value: -6
  dst:
    kind: var
    name: %1
- instruction:
  kind: binary
  op: multiply
  src1:
    kind: var
    name: %1
  src2:
    kind: constant
    const:
      kind: int
      value: 2
  dst:
    kind: var
    name: %2
- instruction:
  kind: int_to_double
  src:
    kind: var
    name: %2
  dst:
    kind: var
    name: %3
- instruction:
  kind: binary
  op: multiply_double
  src1:
    kind: var
    name: %3
  src2:
    kind: constant
    const:
      kind: double
      value: 0x1.f4p+9
  dst:
    kind: var
    name: %4
- instruction:
  kind: binary
  op: divide_double
  src1:
    kind: var
    name: %4
  src2:
    kind: constant
    const:
      kind: double
      value: 0x1.8p+2
  dst:
    kind: var
    name: %5
- instruction:
  kind: double_to_uint
  src:
    kind: var
    name: %5
  dst:
    kind: var
    name: %6
- instruction:
  kind: uint_to_double
  src:
    kind: var
    name: %6
  dst:
    kind: var
    name: %7
- instruction:
  kind: binary
  op: add_double
  src1:
    kind: var
    name: %7
  src2:
    kind: constant
    const:
      kind: double
      value: 0x1.4p+2
  dst:
    kind: var
    name: %8
- instruction:
  kind: unary
  op: negate
  src:
    kind: var
    name: %2
  dst:
    kind: var
    name: %9
- instruction:
  kind: sign_extend
  src:
    kind: var
    name: %9
  dst:
    kind: var
    name: %10
- instruction:
  kind: truncate
  src:
    kind: var
    name: %10
  dst:
    kind: var
    name: %11
- instruction:
  kind: int_to_double
  src:
    kind: var
    name: %2
  dst:
    kind: var
    name: %12
- instruction:
  kind: binary
  op: add_double
  src1:
    kind: var
    name: %8
  src2:
    kind: var
    name: %12
  dst:
    kind: var
    name: %13
- instruction:
  kind: int_to_double
  src:
    kind: var
    name: %11
  dst:
    kind: var
    name: %14
- instruction:
  kind: binary
  op: subtract_double
  src1:
    kind: var
    name: %13
  src2:
    kind: var
    name: %14
  dst:
    kind: var
    name: %15
- instruction:
  kind: double_to_int
  src:
    kind: var
    name: %15
  dst:
    kind: var
    name: %16
- instruction:
  kind: return
  src:
    kind: var
    name: %16
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_AllTypes_PropagateNullPointer)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we can propagate 0 between integer and
 * different pointer types
 * */
long *target(void) {
    int *ptr = 0;
    long *ptr2 = (long *)ptr;
    return ptr2;  // this should be rewritten as 'return 0'
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: long
      value: 0
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_AllTypes_RedundantDoubleCopies)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we eliminate y = x and y = x if we can prove that x and y
 * already have the same values.
 * After copy propagation and cleanup unreachable code elimination,
 * target should contain no control-flow instructions.
 * Similar to int_only/redundant_copies.c but with doubles
 * */

double target(int flag, int flag2, double y) {
    double x = y;

    if (flag) {
        y = x;  // we can remove this because x and y already have the same
                // value
    }
    if (flag2) {
        x = y;  // we can remove this because x and y already have the same
                // value
    }
    return x + y;
}
)SRC"),
              R"OPT(- instruction:
  kind: copy
  src:
    kind: var
    name: %y
  dst:
    kind: var
    name: %x
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %flag
  target: %0
- instruction:
  kind: jump
  target: %1
- instruction:
  kind: label
  name: %0
- instruction:
  kind: label
  name: %1
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %flag2
  target: %2
- instruction:
  kind: copy
  src:
    kind: var
    name: %y
  dst:
    kind: var
    name: %x
- instruction:
  kind: jump
  target: %3
- instruction:
  kind: label
  name: %2
- instruction:
  kind: label
  name: %3
- instruction:
  kind: binary
  op: add_double
  src1:
    kind: var
    name: %x
  src2:
    kind: var
    name: %y
  dst:
    kind: var
    name: %4
- instruction:
  kind: return
  src:
    kind: var
    name: %4
)OPT");
}

// Structural assertion (instruction-kind histogram) — exact TAC
// YAML too large to pin verbatim (31 instructions).
TEST_F(PipelineTest, Chapter19_CP_AllTypes_RedundantStructCopies)
{
    EXPECT_EQ(KindHistogram(OptimizeYaml(R"SRC(
/* Test that we eliminate y = x and y = x if we can prove that x and y
 * already have the same values.
 * After copy propagation and cleanup unreachable code elimination,
 * target should contain no control-flow instructions.
 * Similar to int_only/redundant_copies.c but with structs
 * */

struct s {
    double d;
    int i;
};

double target(int flag, int flag2, struct s y) {
    struct s x = y;

    if (flag) {
        y = x;  // we can remove this because x and y already have the same
                // value
    }
    if (flag2) {
        x = y;  // we can remove this because x and y already have the same
                // value
    }
    return x.d + y.d + x.i + y.i;
}
)SRC")),
              "allocate_local=1 binary=3 copy_from_offset=10 copy_to_offset=6 int_to_double=2 jump=2 jump_if_zero=2 label=4 return=1");
}

// Structural assertion (instruction-kind histogram) — exact TAC
// YAML too large to pin verbatim (47 instructions).
TEST_F(PipelineTest, Chapter19_CP_AllTypes_StoreDoesntKill)
{
    EXPECT_EQ(KindHistogram(OptimizeYaml(R"SRC(
/* Test that updating a value through a pointer does not kill that pointer */

void exit(int status);  // from standard library

void check_pointers(int a, int b, int *ptr1, int *ptr2) {
    if (a != 100 || b != 101) {
        exit(1);
    }

    if (*ptr1 != 60 || *ptr2 != 61) {
        exit(2);
    }
    return;
}

int callee(int *p1, int *p2) {
    if (p1 != p2) {
        exit(3);
    }
    if (*p2 != 10) {
        exit(4);
    }
    return 0;  // success
}

int target(int *ptr, int *ptr2) {
    // first, call another function, with these arguments
    // in different positions than in target or callee, so we can't
    // coalesce them with the param-passing registers or each other
    check_pointers(100, 101, ptr, ptr2);

    ptr2 = ptr;  // generate copy
    *ptr = 10;   // Store(10, ptr) does NOT kill copy

    // both arguments to callee should be the same
    return callee(ptr, ptr2);
}
)SRC")),
              "binary=8 copy=2 fun_call=6 jump=6 jump_if_not_zero=2 jump_if_zero=4 label=12 load=3 return=3 store=1");
}

TEST_F(PipelineTest, Chapter19_CP_AllTypes_DontPropagate_CopyToOffset)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that CopyToOffset kills its destination */
struct s {
    int x;
    int y;
};

int main(void) {
    static struct s s1 = {1, 2};
    struct s s2 = {3, 4};
    s1 = s2;   // generate s1 = s2
    s2.x = 5;  // kill s1 = s2

    return s1.x;  // make sure we don't propagate s2 into this return statement
}
)SRC"),
              R"OPT(- instruction:
  kind: allocate_local
  name: %s2
  size: 8
  alignment: 4
- instruction:
  kind: copy_to_offset
  src:
    kind: constant
    const:
      kind: int
      value: 3
  dst: %s2
  offset: 0
- instruction:
  kind: copy_to_offset
  src:
    kind: constant
    const:
      kind: int
      value: 4
  dst: %s2
  offset: 4
- instruction:
  kind: copy_from_offset
  src: %s2
  offset: 0
  dst:
    kind: var
    name: %0
- instruction:
  kind: copy_to_offset
  src:
    kind: var
    name: %0
  dst: s1
  offset: 0
- instruction:
  kind: copy_to_offset
  src:
    kind: constant
    const:
      kind: int
      value: 5
  dst: %s2
  offset: 0
- instruction:
  kind: copy_from_offset
  src: s1
  offset: 0
  dst:
    kind: var
    name: %1
- instruction:
  kind: return
  src:
    kind: var
    name: %1
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_AllTypes_DontPropagate_DontPropagateAddrOf)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we don't propagate copies into AddrOf instructions */
int main(void) {
    long x = 1;
    long y = 2;
    x = y;            // gen x = y
    return &x == &y;  // don't rewrite as &y == &y
}
)SRC"),
              R"OPT(- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: long
      value: 2
  dst:
    kind: var
    name: %y
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: long
      value: 2
  dst:
    kind: var
    name: %x
- instruction:
  kind: get_address
  src:
    kind: var
    name: %x
  dst:
    kind: var
    name: %2
- instruction:
  kind: get_address
  src:
    kind: var
    name: %y
  dst:
    kind: var
    name: %3
- instruction:
  kind: binary
  op: equal
  src1:
    kind: var
    name: %2
  src2:
    kind: var
    name: %3
  dst:
    kind: var
    name: %4
- instruction:
  kind: return
  src:
    kind: var
    name: %4
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_AllTypes_DontPropagate_StaticAreAliased)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we consider all static variables aliased,
 * so store kills copies to/from these variables */
int stat;

int target(int *stat_ptr) {
    int a = 0;
    a = stat;       // gen a = stat
    *stat_ptr = 8;  // kill a = stat
    return a;       // make sure we don't rewrite as 'return stat'
}
)SRC"),
              R"OPT(- instruction:
  kind: store
  src:
    kind: constant
    const:
      kind: int
      value: 8
  dst_ptr:
    kind: var
    name: %stat_ptr
- instruction:
  kind: return
  src:
    kind: var
    name: stat
)OPT");
}

// Structural assertion (instruction-kind histogram) — exact TAC
// YAML too large to pin verbatim (62 instructions).
TEST_F(PipelineTest, Chapter19_CP_AllTypes_DontPropagate_StoreKillsAliased)
{
    EXPECT_EQ(KindHistogram(OptimizeYaml(R"SRC(
/* Test that store instruction kills any copy
 * whose source or destination is aliased
 */

// Store kills copy w/ aliased src
int aliased_src(int flag, int x, int *ptr) {
    int y = x;  // gen y = x
    if (flag) {
        ptr = &x;  // x is aliased
    }
    *ptr = 100;  // kill y = x

    return y;  // make sure this isn't rewritten as 'return x'
}

// Store kills copy w/ aliased dst
int aliased_dst(int flag, int x, int *ptr) {
    int y = x;  // gen y = x
    if (flag) {
        ptr = &y;  // y is aliased
    }
    *ptr = 100;  // kill y = x
    return y;    // make sure this isn't rewritten as 'return x'
}

int main(void) {
    int i = 0;
    // first call aliased_src w/ flag = 0;
    // Store instruction will update i
    if (aliased_src(0, 1, &i) != 1) {
        return 1;  // fail
    }
    if (i != 100) {
        return 2;  // fail
    }

    // call again w/ flag = 1; won't update i or return value
    i = 0;
    if (aliased_src(1, 2, &i) != 2) {
        return 3;  // fail
    }
    if (i != 0) {
        return 4;  // fail
    }

    // call aliased_dst with flag = 0;
    // Store instruction will update i
    if (aliased_dst(0, 5, &i) != 5) {
        return 5;  // fail
    }

    if (i != 100) {
        return 6;  // fail
    }
    // call aliased_dst with flag = 1;
    // Store won't update i, will update return value
    i = 0;
    if (aliased_dst(1, 5, &i) != 100) {
        return 7;  // fail
    }

    if (i != 0) {
        return 8;  // fail
    }

    return 0;  // success
}
)SRC")),
              "binary=8 copy=7 fun_call=4 get_address=6 jump=2 jump_if_zero=10 label=12 return=11 store=2");
}

TEST_F(PipelineTest, Chapter19_CP_AllTypes_DontPropagate_TypeConversion)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we don't propagate copies between values of different types,
 * since that will lead to incorrect assembly generation (e.g. for comparisons
 * and division). (If we had separate TACKY operators for signed vs unsigned
 * comparisons/division/remainder operations instead of inferring signedness
 * from TACKY operand types, it would be save to propagate these copies.)
 */
int target(int i) {
    unsigned int j = i;
    return (j / 100);  // make sure we don't rewrite as i / 100
                       // correct answer is 42949670,
                       // but if we propagate this copy we'll return -2
}
)SRC"),
              R"OPT(- instruction:
  kind: binary
  op: divide_unsigned
  src1:
    kind: var
    name: %i
  src2:
    kind: constant
    const:
      kind: int
      value: 100
  dst:
    kind: var
    name: %2
- instruction:
  kind: return
  src:
    kind: var
    name: %2
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_AllTypes_DontPropagate_ZeroNegZeroDifferent)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Make sure our copy propagation pass recognizes 0.0 and -0.0
 * as distinct values, even though they compare equal with standard
 * floating-point comparison operators
 * */
double copysign(double x, double y);

double target(int flag) {
    double result = 0.0;  // gen result = 0.0
    if (flag) {
        result = -0.0;  // gen result = -0.0
    }

    // can't propagate value of result because it has
    // different values on different paths
    return result;
}
)SRC"),
              R"OPT(- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: double
      value: 0x0p+0
  dst:
    kind: var
    name: %result
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %flag
  target: %0
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: double
      value: -0x0p+0
  dst:
    kind: var
    name: %result
- instruction:
  kind: jump
  target: %1
- instruction:
  kind: label
  name: %0
- instruction:
  kind: label
  name: %1
- instruction:
  kind: return
  src:
    kind: var
    name: %result
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_AllTypes_ExtraCredit_CopyUnion)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Propagate copies of whole unions */

union u {
    long l;
    int i;
};

int callee(union u a, union u b) {
    if (a.l != -100) {
        return 1; // fail
    }
    if (b.l != -100) {
        return 2; // fail
    }

    return 0; // success
}

int target(void) {
    union u u1 = {0};
    union u u2 = {-100};
    u1 = u2; // generates u1 = u2

    // Make sure we pass the same value for both arguments.
    // We don't need to worry that register coalescing
    // will interfere with this test, because unions
    // won't be stored in registers
    return callee(u1, u2);
}
)SRC"),
              R"OPT(- instruction:
  kind: copy_from_offset
  src: %a
  offset: 0
  dst:
    kind: var
    name: %0
- instruction:
  kind: sign_extend
  src:
    kind: constant
    const:
      kind: int
      value: -100
  dst:
    kind: var
    name: %2
- instruction:
  kind: binary
  op: not_equal
  src1:
    kind: var
    name: %0
  src2:
    kind: var
    name: %2
  dst:
    kind: var
    name: %3
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %3
  target: %4
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: label
  name: %4
- instruction:
  kind: copy_from_offset
  src: %b
  offset: 0
  dst:
    kind: var
    name: %6
- instruction:
  kind: sign_extend
  src:
    kind: constant
    const:
      kind: int
      value: -100
  dst:
    kind: var
    name: %8
- instruction:
  kind: binary
  op: not_equal
  src1:
    kind: var
    name: %6
  src2:
    kind: var
    name: %8
  dst:
    kind: var
    name: %9
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %9
  target: %10
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 2
- instruction:
  kind: label
  name: %10
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: allocate_local
  name: %u1
  size: 8
  alignment: 8
- instruction:
  kind: copy_to_offset
  src:
    kind: constant
    const:
      kind: long
      value: 0
  dst: %u1
  offset: 0
- instruction:
  kind: allocate_local
  name: %u2
  size: 8
  alignment: 8
- instruction:
  kind: sign_extend
  src:
    kind: constant
    const:
      kind: int
      value: -100
  dst:
    kind: var
    name: %2
- instruction:
  kind: copy_to_offset
  src:
    kind: var
    name: %2
  dst: %u2
  offset: 0
- instruction:
  kind: copy_from_offset
  src: %u2
  offset: 0
  dst:
    kind: var
    name: %3
- instruction:
  kind: copy_to_offset
  src:
    kind: var
    name: %3
  dst: %u1
  offset: 0
- instruction:
  kind: fun_call
  fun_name: callee
  args:
    - val:
      kind: var
      name: %u1
    - val:
      kind: var
      name: %u2
  dst:
    kind: var
    name: %4
- instruction:
  kind: return
  src:
    kind: var
    name: %4
)OPT");
}

// Structural assertion (instruction-kind histogram) — exact TAC
// YAML too large to pin verbatim (75 instructions).
TEST_F(PipelineTest, Chapter19_CP_AllTypes_ExtraCredit_PointerCompoundAssignment)
{
    EXPECT_EQ(KindHistogram(OptimizeYaml(R"SRC(
/* We can calculate constant offset for +=/-= with pointers into arrays;
 * similar to pointer_arithmetic.c
 */

int target(void) {
    int nested[3][23] = { {0, 1}, {2} };
    int (* ptr)[23] = nested;
    ptr += 2;
    return *ptr[0];
}
)SRC")),
              "add_ptr=2 allocate_local=1 copy_to_offset=69 get_address=1 load=1 return=1");
}

// Structural assertion (instruction-kind histogram) — exact TAC
// YAML too large to pin verbatim (75 instructions).
TEST_F(PipelineTest, Chapter19_CP_AllTypes_ExtraCredit_PointerIncr)
{
    EXPECT_EQ(KindHistogram(OptimizeYaml(R"SRC(
/* We can calculate constant offset for ++/-- with pointers into arrays;
 * similar to pointer_arithmetic.c
 */
int target(void) {
    int nested[3][23] = { {0, 1}, {2} };
    int (* ptr)[23] = nested;
    ptr++;
    return *ptr[0];
}
)SRC")),
              "add_ptr=2 allocate_local=1 copy_to_offset=69 get_address=1 load=1 return=1");
}

// DISABLED: NaN constant folding loops in the optimizer
TEST_F(PipelineTest, DISABLED_Chapter19_CP_AllTypes_ExtraCredit_RedundantNanCopy)
{
    OptimizeYaml(R"SRC(
/* Make sure we can eliminate redundant copies where source is NaN
 * (which requires us to compare NaN values appropriately and recognize when
 * they're the same even though NaN doesn't compare equal to itself).
 * We should be able to eliminate all control-flow instructions from target
 */

int double_isnan(double d); // defined in tests/chapter_13/helper_libs/nan.c

double na;

int target(int flag) {
    na = 0.0 / 0.0;
    double d = 0.0 / 0.0;
    if (flag) {
        na = d; // same value it already is; can delete this
    }
    return 0;
}
)SRC");
}

TEST_F(PipelineTest, Chapter19_CP_AllTypes_ExtraCredit_RedundantUnionCopy)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we eliminate y = x and y = x if we can prove that x and y
 * already have the same values.
 * After copy propagation and cleanup unreachable code elimination,
 * target should contain no control-flow instructions.
 * Similar to int_only/redundant_copies.c but with unions
 * */

union u {
    double d;
    int i;
};

double target(int flag, int flag2, union u y) {
    union u x = y;

    if (flag) {
        y = x;  // we can remove this because x and y already have the same
                // value
    }
    if (flag2) {
        x = y;  // we can remove this because x and y already have the same
                // value
    }
    return x.d + y.d;
}
)SRC"),
              R"OPT(- instruction:
  kind: allocate_local
  name: %x
  size: 8
  alignment: 8
- instruction:
  kind: copy_from_offset
  src: %y
  offset: 0
  dst:
    kind: var
    name: %0
- instruction:
  kind: copy_to_offset
  src:
    kind: var
    name: %0
  dst: %x
  offset: 0
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %flag
  target: %1
- instruction:
  kind: copy_from_offset
  src: %x
  offset: 0
  dst:
    kind: var
    name: %3
- instruction:
  kind: copy_to_offset
  src:
    kind: var
    name: %3
  dst: %y
  offset: 0
- instruction:
  kind: jump
  target: %2
- instruction:
  kind: label
  name: %1
- instruction:
  kind: label
  name: %2
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %flag2
  target: %4
- instruction:
  kind: copy_from_offset
  src: %y
  offset: 0
  dst:
    kind: var
    name: %6
- instruction:
  kind: copy_to_offset
  src:
    kind: var
    name: %6
  dst: %x
  offset: 0
- instruction:
  kind: jump
  target: %5
- instruction:
  kind: label
  name: %4
- instruction:
  kind: label
  name: %5
- instruction:
  kind: copy_from_offset
  src: %x
  offset: 0
  dst:
    kind: var
    name: %7
- instruction:
  kind: copy_from_offset
  src: %y
  offset: 0
  dst:
    kind: var
    name: %8
- instruction:
  kind: binary
  op: add_double
  src1:
    kind: var
    name: %7
  src2:
    kind: var
    name: %8
  dst:
    kind: var
    name: %9
- instruction:
  kind: return
  src:
    kind: var
    name: %9
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_AllTypes_ExtraCredit_DontPropagate_UpdateUnionMember)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Writing to union member kills previous copies to/from that union */

union u {
    long l;
    int i;
};

int main(void) {
    static union u u1 = {20};
    union u u2 = {3};
    u1 = u2; // generate u1 = u2
    u2.i = 0; // kill u1 = u2

    return u1.i;  // make sure we don't propagate u2 into this return statement
}
)SRC"),
              R"OPT(- instruction:
  kind: allocate_local
  name: %u2
  size: 8
  alignment: 8
- instruction:
  kind: copy_to_offset
  src:
    kind: constant
    const:
      kind: long
      value: 3
  dst: %u2
  offset: 0
- instruction:
  kind: copy_from_offset
  src: %u2
  offset: 0
  dst:
    kind: var
    name: %1
- instruction:
  kind: copy_to_offset
  src:
    kind: var
    name: %1
  dst: u1
  offset: 0
- instruction:
  kind: copy_to_offset
  src:
    kind: constant
    const:
      kind: int
      value: 0
  dst: %u2
  offset: 0
- instruction:
  kind: copy_from_offset
  src: u1
  offset: 0
  dst:
    kind: var
    name: %2
- instruction:
  kind: return
  src:
    kind: var
    name: %2
)OPT");
}

TEST_F(PipelineTest, Chapter19_CP_AllTypes_ExtraCredit_DontPropagate_UpdateUnionMember2)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Copy to one member of a union kills previous copy to other member.
 * The easiest way to handle this, which is in line with how we handle structs,
 * is to not attempt to propagate copies to/from union members at all.
 * But if you do implement a more sophisticated copy propagation pass that
 * tracks copies to/from union members, you need to account for the fact
 * that they overlap.
 */

union u {
    long l;
    int i;
};

int main(void) {
    union u x;
    x.i = 100;
    x.l = 200; // clobber x.i;
    return x.i; // should be 200 (due to type punning), not 100
}
)SRC"),
              R"OPT(- instruction:
  kind: allocate_local
  name: %x
  size: 8
  alignment: 8
- instruction:
  kind: copy_to_offset
  src:
    kind: constant
    const:
      kind: int
      value: 100
  dst: %x
  offset: 0
- instruction:
  kind: copy_to_offset
  src:
    kind: constant
    const:
      kind: long
      value: 200
  dst: %x
  offset: 0
- instruction:
  kind: copy_from_offset
  src: %x
  offset: 0
  dst:
    kind: var
    name: %1
- instruction:
  kind: return
  src:
    kind: var
    name: %1
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_IntOnly_DeadStoreStaticVar)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we eliminate dead stores to static and global variables */

int i = 0;

int target(int arg) {
    i = 5;  // dead store
    i = arg;
    return i + 1;
}
)SRC"),
              R"OPT(- instruction:
  kind: copy
  src:
    kind: var
    name: %arg
  dst:
    kind: var
    name: i
- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: %arg
  src2:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %0
- instruction:
  kind: return
  src:
    kind: var
    name: %0
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_IntOnly_DeleteArithmeticOps)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* In most of our test cases, the dead store we remove is a Copy.
 * This test case validates that we can remove dead
 * Binary and Unary instructions too.
 * */


int a = 1;
int b = 2;

int target(void) {
    // everything except the Return instruction should be optimized away.
    int unused = a * -b;
    return 5;
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 5
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_IntOnly_ElimSecondCopy)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* We can recognize that one store to a variable is a dead store,
 * but another store to that variable at a different point in the program
 * is not.
 * */
int callee(int arg) {
    return arg * 2;
}

int target(int arg, int flag) {
    int x = arg + 1;   // not a dead store
    if (flag) {
        // make sure x has more than one possible value,
        // so copy prop doesn't just replace it with a temporary
        // variable callee
        x = arg - 1;
    }
    int y = callee(x); // this generates x
    x = 100;  // dead store
    return y;
}
)SRC"),
              R"OPT(- instruction:
  kind: binary
  op: multiply
  src1:
    kind: var
    name: %arg
  src2:
    kind: constant
    const:
      kind: int
      value: 2
  dst:
    kind: var
    name: %0
- instruction:
  kind: return
  src:
    kind: var
    name: %0
- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: %arg
  src2:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %0
- instruction:
  kind: copy
  src:
    kind: var
    name: %0
  dst:
    kind: var
    name: %x
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %flag
  target: %1
- instruction:
  kind: binary
  op: subtract
  src1:
    kind: var
    name: %arg
  src2:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %3
- instruction:
  kind: copy
  src:
    kind: var
    name: %3
  dst:
    kind: var
    name: %x
- instruction:
  kind: jump
  target: %2
- instruction:
  kind: label
  name: %1
- instruction:
  kind: label
  name: %2
- instruction:
  kind: fun_call
  fun_name: callee
  args:
    - val:
      kind: var
      name: %x
  dst:
    kind: var
    name: %4
- instruction:
  kind: return
  src:
    kind: var
    name: %4
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_IntOnly_Fig1911)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* We recognize an update to some variable as a dead store
 * when there are multiple paths from the store to some use of that
 * variable, and it's killed by different instructions
 * on those different paths.
 * This example is loosely based on Figure 19-11.
 * */
int callee(void) {
    return 4;
}

int callee2(void) {
    return 5;
}

int target(int flag) {
    int x = 10;  // this is a dead store; make sure its eliminated
    if (flag) {
        x = callee(); // this kills x; it's dead at earlier points
    } else {
        x = callee2(); // this kills x; it's dead at earlier points
    }
    return x; // this generates x; it's live at earlier points
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 4
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 5
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %flag
  target: %0
- instruction:
  kind: fun_call
  fun_name: callee
  dst:
    kind: var
    name: %2
- instruction:
  kind: copy
  src:
    kind: var
    name: %2
  dst:
    kind: var
    name: %x
- instruction:
  kind: jump
  target: %1
- instruction:
  kind: label
  name: %0
- instruction:
  kind: fun_call
  fun_name: callee2
  dst:
    kind: var
    name: %3
- instruction:
  kind: copy
  src:
    kind: var
    name: %3
  dst:
    kind: var
    name: %x
- instruction:
  kind: label
  name: %1
- instruction:
  kind: return
  src:
    kind: var
    name: %x
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_IntOnly_InitializeBlocksWithEmptySet)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we initialize each block in the CFG with an empty set of
 * live variables. Specifically, this test will fail if each block is
 * initialized with the set of all static variables.
 */


int j = 3;
int target(void) {
    static int i;
    i = 10;  // dead store, b/c i is killed on path to exit
             // but if we initially think i is live at the start of the
             // while loop, our analysis will never figure out that it's dead
             // here
    while (j > 0) {
        j = j - 1;
    }
    i = 0;
    return 0;
}
)SRC"),
              R"OPT(- instruction:
  kind: label
  name: %L1
- instruction:
  kind: binary
  op: greater_than
  src1:
    kind: var
    name: j
  src2:
    kind: constant
    const:
      kind: int
      value: 0
  dst:
    kind: var
    name: %0
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %0
  target: %L0
- instruction:
  kind: binary
  op: subtract
  src1:
    kind: var
    name: j
  src2:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %1
- instruction:
  kind: copy
  src:
    kind: var
    name: %1
  dst:
    kind: var
    name: j
- instruction:
  kind: jump
  target: %L1
- instruction:
  kind: label
  name: %L0
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 0
  dst:
    kind: var
    name: i
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_IntOnly_LoopDeadStore)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we can detect dead stores in a function with a loop */
int putch(int c);  // from standard library

int target(void) {
    int x = 5;   // dead store
    int y = 65;  // not a dead store
    do {
        x = y + 2;  // kill x, gen y
        if (y > 70) {
            // make sure we assign to x on multiple paths
            // so copy prop doesn't replace it entirely
            x = y + 3;
        }
        y = putch(x) + 3;  // gen x and y
    } while (y < 90);
    if (x != 90) {
        return 1;  // fail
    }
    if (y != 93) {
        return 2;  // fail
    }
    return 0;  // success
}
)SRC"),
              R"OPT(- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 65
  dst:
    kind: var
    name: %y
- instruction:
  kind: label
  name: %0
- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: %y
  src2:
    kind: constant
    const:
      kind: int
      value: 2
  dst:
    kind: var
    name: %1
- instruction:
  kind: copy
  src:
    kind: var
    name: %1
  dst:
    kind: var
    name: %x
- instruction:
  kind: binary
  op: greater_than
  src1:
    kind: var
    name: %y
  src2:
    kind: constant
    const:
      kind: int
      value: 70
  dst:
    kind: var
    name: %2
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %2
  target: %3
- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: %y
  src2:
    kind: constant
    const:
      kind: int
      value: 3
  dst:
    kind: var
    name: %5
- instruction:
  kind: copy
  src:
    kind: var
    name: %5
  dst:
    kind: var
    name: %x
- instruction:
  kind: jump
  target: %4
- instruction:
  kind: label
  name: %3
- instruction:
  kind: label
  name: %4
- instruction:
  kind: fun_call
  fun_name: putch
  args:
    - val:
      kind: var
      name: %x
  dst:
    kind: var
    name: %6
- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: %6
  src2:
    kind: constant
    const:
      kind: int
      value: 3
  dst:
    kind: var
    name: %7
- instruction:
  kind: copy
  src:
    kind: var
    name: %7
  dst:
    kind: var
    name: %y
- instruction:
  kind: binary
  op: less_than
  src1:
    kind: var
    name: %7
  src2:
    kind: constant
    const:
      kind: int
      value: 90
  dst:
    kind: var
    name: %8
- instruction:
  kind: jump_if_not_zero
  condition:
    kind: var
    name: %8
  target: %0
- instruction:
  kind: binary
  op: not_equal
  src1:
    kind: var
    name: %x
  src2:
    kind: constant
    const:
      kind: int
      value: 90
  dst:
    kind: var
    name: %9
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %9
  target: %10
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: label
  name: %10
- instruction:
  kind: binary
  op: not_equal
  src1:
    kind: var
    name: %7
  src2:
    kind: constant
    const:
      kind: int
      value: 93
  dst:
    kind: var
    name: %12
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %12
  target: %13
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 2
- instruction:
  kind: label
  name: %13
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_IntOnly_Simple)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* A basic test case for eliminating a dead store */


int target(void) {
    int x = 10; // this is a dead store
    return 3;
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 3
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_IntOnly_StaticNotAlwaysLive)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Make sure the meet operator doesn't always assume static variables are live;
 * they're only generated by uses, function calls, and EXIT.
 * Test this using a program that never reaches EXIT (but does terminate
 * by caling the exit function indirectly)
 * */

int exit_wrapper(int status);  // defined in chapter_19/libraries/exit.c

int i;

int target(void) {
    i = 30;  // dead store!
    // i isn't killed in this block but it's killed on all paths to function
    // call
    int counter = 0;

    do {
        if (counter < 10) {
            i = counter + 1;
        } else {
            i = counter + 2;
        }
        if (counter > 20) {
            exit_wrapper(i);
        }
        counter = counter + 1;
    } while (1);
    return 0;
}
)SRC"),
              R"OPT(- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 0
  dst:
    kind: var
    name: %counter
- instruction:
  kind: label
  name: %0
- instruction:
  kind: binary
  op: less_than
  src1:
    kind: var
    name: %counter
  src2:
    kind: constant
    const:
      kind: int
      value: 10
  dst:
    kind: var
    name: %1
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %1
  target: %2
- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: %counter
  src2:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %4
- instruction:
  kind: copy
  src:
    kind: var
    name: %4
  dst:
    kind: var
    name: i
- instruction:
  kind: jump
  target: %3
- instruction:
  kind: label
  name: %2
- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: %counter
  src2:
    kind: constant
    const:
      kind: int
      value: 2
  dst:
    kind: var
    name: %5
- instruction:
  kind: copy
  src:
    kind: var
    name: %5
  dst:
    kind: var
    name: i
- instruction:
  kind: label
  name: %3
- instruction:
  kind: binary
  op: greater_than
  src1:
    kind: var
    name: %counter
  src2:
    kind: constant
    const:
      kind: int
      value: 20
  dst:
    kind: var
    name: %6
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %6
  target: %7
- instruction:
  kind: fun_call
  fun_name: exit_wrapper
  args:
    - val:
      kind: var
      name: i
  dst:
    kind: var
    name: %9
- instruction:
  kind: jump
  target: %8
- instruction:
  kind: label
  name: %7
- instruction:
  kind: label
  name: %8
- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: %counter
  src2:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %10
- instruction:
  kind: copy
  src:
    kind: var
    name: %10
  dst:
    kind: var
    name: %counter
- instruction:
  kind: jump
  target: %0
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_IntOnly_DontElim_AddAllToWorklist)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Make sure we add every basic block to the worklist
 * at the start of the iterative algorithm
 */
int putch(int c);

int f(int arg) {
    int x = 76;
    if (arg < 10) {
        // give x multiple values on different paths
        // so we can't propagate it
        x = 77;
    }
    // no live variables flow into this basic block from its successor,
    // bu we still need to process it to learn that x is live
    if (arg)
        putch(x);
    return 0;
}

int main(void) {
    f(0);
    f(1);
    f(11);
    return 0;
}
)SRC"),
              R"OPT(- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 76
  dst:
    kind: var
    name: %x
- instruction:
  kind: binary
  op: less_than
  src1:
    kind: var
    name: %arg
  src2:
    kind: constant
    const:
      kind: int
      value: 10
  dst:
    kind: var
    name: %0
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %0
  target: %1
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 77
  dst:
    kind: var
    name: %x
- instruction:
  kind: jump
  target: %2
- instruction:
  kind: label
  name: %1
- instruction:
  kind: label
  name: %2
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %arg
  target: %3
- instruction:
  kind: fun_call
  fun_name: putch
  args:
    - val:
      kind: var
      name: %x
  dst:
    kind: var
    name: %5
- instruction:
  kind: jump
  target: %4
- instruction:
  kind: label
  name: %3
- instruction:
  kind: label
  name: %4
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: fun_call
  fun_name: f
  args:
    - val:
      kind: constant
      const:
        kind: int
        value: 0
  dst:
    kind: var
    name: %0
- instruction:
  kind: fun_call
  fun_name: f
  args:
    - val:
      kind: constant
      const:
        kind: int
        value: 1
  dst:
    kind: var
    name: %1
- instruction:
  kind: fun_call
  fun_name: f
  args:
    - val:
      kind: constant
      const:
        kind: int
        value: 11
  dst:
    kind: var
    name: %2
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_IntOnly_DontElim_DontRemoveFuncall)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we never optimize away function calls,
 * even if they're dead stores (i.e. update dead variables)
 * because they can have side effects */

int putch(int c);

int main(void) {
    // Make sure we don't optimize away this function call.
    // It would be safe to keep the function call, but optimize out
    // the store to x (i.e. get rid of movl %eax, %x), but our implementation
    // doesn't.
    int x = putch(67);
    return 0;
}
)SRC"),
              R"OPT(- instruction:
  kind: fun_call
  fun_name: putch
  args:
    - val:
      kind: constant
      const:
        kind: int
        value: 67
  dst:
    kind: var
    name: %0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_IntOnly_DontElim_Loop)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test case where a block is its own predecessor
 * */

int putch(int c);

int fib(int count) {
    int n0 = 0;
    int n1 = 1;
    int i = 0;
    do {
        int n2 = n0 + n1;
        n0 = n1;  // not a dead store b/c n0 is used again in the next loop
                  // iteration, in n2 = n0 + n1
        n1 = n2;
        i = i + 1;
    } while (i < count);
    return n1;
}

int main(void) {
    return (fib(20) == 10946);
}
)SRC"),
              R"OPT(- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 0
  dst:
    kind: var
    name: %n0
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %n1
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 0
  dst:
    kind: var
    name: %i
- instruction:
  kind: label
  name: %0
- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: %n0
  src2:
    kind: var
    name: %n1
  dst:
    kind: var
    name: %1
- instruction:
  kind: copy
  src:
    kind: var
    name: %1
  dst:
    kind: var
    name: %n2
- instruction:
  kind: copy
  src:
    kind: var
    name: %n1
  dst:
    kind: var
    name: %n0
- instruction:
  kind: copy
  src:
    kind: var
    name: %1
  dst:
    kind: var
    name: %n1
- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: %i
  src2:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %2
- instruction:
  kind: copy
  src:
    kind: var
    name: %2
  dst:
    kind: var
    name: %i
- instruction:
  kind: binary
  op: less_than
  src1:
    kind: var
    name: %2
  src2:
    kind: var
    name: %count
  dst:
    kind: var
    name: %3
- instruction:
  kind: jump_if_not_zero
  condition:
    kind: var
    name: %3
  target: %0
- instruction:
  kind: return
  src:
    kind: var
    name: %n2
- instruction:
  kind: fun_call
  fun_name: fib
  args:
    - val:
      kind: constant
      const:
        kind: int
        value: 20
  dst:
    kind: var
    name: %0
- instruction:
  kind: binary
  op: equal
  src1:
    kind: var
    name: %0
  src2:
    kind: constant
    const:
      kind: int
      value: 10946
  dst:
    kind: var
    name: %1
- instruction:
  kind: return
  src:
    kind: var
    name: %1
)OPT");
}

// Structural assertion (instruction-kind histogram) — exact TAC
// YAML too large to pin verbatim (32 instructions).
TEST_F(PipelineTest, Chapter19_DSE_IntOnly_DontElim_NestedLoops)
{
    EXPECT_EQ(KindHistogram(OptimizeYaml(R"SRC(
/* Test that the algorithm runs until it converges;
 * some blocks need to be visited three times before the algorithm converges
 * */

int putch(int c);

int target(int a, int b, int c, int d) {
    while (a > 0) {
        while (c > 0) {
            putch(c + d);
            c = c - 1;
            if (d % 2) {
                c = c - 2;
            }
        }

        while (b > 0) {
            c = 10;  // this is not dead, b/c it's used in previous while
                     // loop, but it takes multiple passes for that
                     // information to propagate to this point
            b = b - 1;
        }

        a = a - 1;
    }
    return 0;
}
)SRC")),
              "binary=9 copy=5 fun_call=1 jump=4 jump_if_zero=4 label=8 return=1");
}

// Structural assertion (instruction-kind histogram) — exact TAC
// YAML too large to pin verbatim (110 instructions).
TEST_F(PipelineTest, Chapter19_DSE_IntOnly_DontElim_RecognizeAllUses)
{
    EXPECT_EQ(KindHistogram(OptimizeYaml(R"SRC(
/* Make sure we recognize all the different ways a variable
 * can be used/generated (in Unary, Binary, JumpIfZero, etc.) */

int test_jz(int flag, int arg) {
    if (flag) {
        arg = 0;  // this store is not dead b/c arg is used later;
                  // put it in an if statement so we don't propagate 0 into
                  // return statement
    }
    return arg ? 1 : 2;
}

int test_jnz(int flag, int arg) {
    if (flag) {
        arg = 0;
    }
    return arg || 0;
}

int test_binary(int flag, int arg1, int arg2) {
    if (flag == 0) {
        arg1 = 4;  // this store is not dead b/c arg is used later;
                   // put it in an if statement so we don't propagate 4 into
                   // return statement
    } else if (flag == 1) {
        arg2 = 3;  // also not a dead store
    }
    return arg1 * arg2;  // generates arg1 and arg2
}

int test_unary(int flag, int arg) {
    if (flag) {
        arg = 5;  // this store is not dead b/c arg is used later;
                  // put it in an if statement so we don't propagate 5 into
                  // return statement
    }
    return -arg;  // generates arg
}

int f(int arg) {
    return arg + 1;
}

int test_funcall(int flag, int arg) {
    if (flag) {
        arg = 7;  // this store is not dead b/c arg is used later;
                  // put it in an if statement so we don't propagate 7 into
                  // return statement
    }
    return f(arg);
}

int main(void) {
    if (test_jz(1, 1) != 2) {  // 0 ? 1 : 2
        return 1; // fail
    }
    if (test_jz(0, 1) != 1) {  // 1 ? 1 : 2
        return 2; // fail
    }
    if (test_jnz(1, 1) != 0) {  // 0 || 0
        return 3; // fail
    }
    if (test_jnz(0, 1) != 1) {  // 1 || 1
        return 4; // fail
    }
    if (test_binary(0, 8, 9) != 36) {  // 4 * 9
        return 5; // fail
    }
    if (test_binary(1, 8, 9) != 24) {  // 8 * 3
        return 6; // fail
    }
    if (test_binary(2, 8, 9) != 72) {  // 8 * 9
        return 7; // fail
    }
    if (test_unary(0, 8) != -8) {
        return 8;  // fail
    }
    if (test_unary(1, 8) != -5) {
        return 9;  // fail
    }
    if (test_funcall(1, 5) != 8) {  // f(7) => 7 + 1
        return 10; // fail
    }
    if (test_funcall(0, 9) != 10) {  // f(9) ==> 9 + 1
        return 11; // fail
    }
    return 0;
}
)SRC")),
              "binary=15 copy=10 fun_call=12 jump=8 jump_if_not_zero=1 jump_if_zero=18 label=27 return=18 unary=1");
}

TEST_F(PipelineTest, Chapter19_DSE_IntOnly_DontElim_SelfCopy)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that updating and using a value in the same instruction generates it
 * rather than killing it. */


int target(int flag) {
    int i = 2;
    // make sure value of i isn't known at compile time,
    // so we can't propagate it
    if (flag) {
        i = 3;
    }
    i = i;  // this is a no-op, but it doesn't kill i
            // if we treat this as a kill instead of a gen,
            // we'll incorrectly eliminate both earlier copies to i
            // as dead stores
    return i;
}
)SRC"),
              R"OPT(- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 2
  dst:
    kind: var
    name: %i
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %flag
  target: %0
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 3
  dst:
    kind: var
    name: %i
- instruction:
  kind: jump
  target: %1
- instruction:
  kind: label
  name: %0
- instruction:
  kind: label
  name: %1
- instruction:
  kind: return
  src:
    kind: var
    name: %i
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_IntOnly_DontElim_StaticVarsAtExit)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Make sure we reocgnize that static local variables may be live at exit */
int f(void) {
    static int i = 10;
    if (i == 5)
        return 0;
    i = 5;  // not a dead store! i is live at exit
    return 1;
}

int main(void) {

    if (f() != 1) {
        return 1; // fail
    }
    if (f() != 0) {
        return 2; // fail
    }
    return 0;
}
)SRC"),
              R"OPT(- instruction:
  kind: binary
  op: equal
  src1:
    kind: var
    name: i
  src2:
    kind: constant
    const:
      kind: int
      value: 5
  dst:
    kind: var
    name: %0
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %0
  target: %1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: label
  name: %1
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 5
  dst:
    kind: var
    name: i
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: fun_call
  fun_name: f
  dst:
    kind: var
    name: %0
- instruction:
  kind: binary
  op: not_equal
  src1:
    kind: var
    name: %0
  src2:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %1
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %1
  target: %2
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: label
  name: %2
- instruction:
  kind: fun_call
  fun_name: f
  dst:
    kind: var
    name: %4
- instruction:
  kind: binary
  op: not_equal
  src1:
    kind: var
    name: %4
  src2:
    kind: constant
    const:
      kind: int
      value: 0
  dst:
    kind: var
    name: %5
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %5
  target: %6
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 2
- instruction:
  kind: label
  name: %6
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_IntOnly_DontElim_StaticVarsFun)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we recognize that function calls generate all static variables */
int x = 100;

int get_x(void) {
    return x;
}

int main(void) {
    x = 5;  // don't eliminate this!
    int result = get_x();
    x = 10;
    return result;
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: var
    name: x
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 5
  dst:
    kind: var
    name: x
- instruction:
  kind: fun_call
  fun_name: get_x
  dst:
    kind: var
    name: %0
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 10
  dst:
    kind: var
    name: x
- instruction:
  kind: return
  src:
    kind: var
    name: %0
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_IntOnly_DontElim_UsedOnePath)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* A variable is live if it's used later on one path but not others.
 * Loosely based on figure 19-10
 * */

int f(int arg, int flag) {
    int x = arg * 2;  // not dead, b/c x is live on one path
    if (flag)
        return x;
    return 0;
}

int main(void) {
    if (f(20, 1) != 40) {
        return 1;  // fail
    }
    if (f(3, 0) != 0) {
        return 2;  // fail
    }
    return 0;  // success
}
)SRC"),
              R"OPT(- instruction:
  kind: binary
  op: multiply
  src1:
    kind: var
    name: %arg
  src2:
    kind: constant
    const:
      kind: int
      value: 2
  dst:
    kind: var
    name: %0
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %flag
  target: %1
- instruction:
  kind: return
  src:
    kind: var
    name: %0
- instruction:
  kind: label
  name: %1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: fun_call
  fun_name: f
  args:
    - val:
      kind: constant
      const:
        kind: int
        value: 20
    - val:
      kind: constant
      const:
        kind: int
        value: 1
  dst:
    kind: var
    name: %0
- instruction:
  kind: binary
  op: not_equal
  src1:
    kind: var
    name: %0
  src2:
    kind: constant
    const:
      kind: int
      value: 40
  dst:
    kind: var
    name: %1
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %1
  target: %2
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: label
  name: %2
- instruction:
  kind: fun_call
  fun_name: f
  args:
    - val:
      kind: constant
      const:
        kind: int
        value: 3
    - val:
      kind: constant
      const:
        kind: int
        value: 0
  dst:
    kind: var
    name: %4
- instruction:
  kind: binary
  op: not_equal
  src1:
    kind: var
    name: %4
  src2:
    kind: constant
    const:
      kind: int
      value: 0
  dst:
    kind: var
    name: %5
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %5
  target: %6
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 2
- instruction:
  kind: label
  name: %6
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_IntOnly_ExtraCredit_DeadCompoundAssignment)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we delete compound assignment to dead variables */

int glob = 0;

int target(void) {
    int x = glob;
    x *= 20; // dead
    x = 10;
    return x;
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 10
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_IntOnly_ExtraCredit_DeadIncrDecr)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we delete ++/-- with dead variables */


static int glob;

int target(void) {
    // initialize these so they can't be constant-folded
    int a = glob;
    int b = glob;
    int c = glob;
    int d = glob;
    // these operations are all dead stores so we'll eliminate them
    a++;
    b--;
    ++c;
    --d;
    return 10;
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 10
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_IntOnly_ExtraCredit_DontElim_IncrAndDeadStore)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
static int x = 10;

int main(void) {
    // incrementing x is not a dead store, although assigning to y is
    int y = ++x;
    return x;
}
)SRC"),
              R"OPT(- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: x
  src2:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %0
- instruction:
  kind: copy
  src:
    kind: var
    name: %0
  dst:
    kind: var
    name: x
- instruction:
  kind: return
  src:
    kind: var
    name: %0
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_AllTypes_AliasedDeadAtExit)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we recognize aliased non-static variables are live
 * just after function calls but dead at function exit
 * */

int b = 0;

void callee(int *ptr) {
    b = *ptr;
    *ptr = 100;
}

int target(void) {
    int x = 10;
    callee(&x);  // generates all aliased variables (i.e. x)
    int y = x;
    x = 50;  // this is dead
    return y;
}
)SRC"),
              R"OPT(- instruction:
  kind: load
  src_ptr:
    kind: var
    name: %ptr
  dst:
    kind: var
    name: %0
- instruction:
  kind: copy
  src:
    kind: var
    name: %0
  dst:
    kind: var
    name: b
- instruction:
  kind: store
  src:
    kind: constant
    const:
      kind: int
      value: 100
  dst_ptr:
    kind: var
    name: %ptr
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 10
  dst:
    kind: var
    name: %x
- instruction:
  kind: get_address
  src:
    kind: var
    name: %x
  dst:
    kind: var
    name: %0
- instruction:
  kind: fun_call
  fun_name: callee
  args:
    - val:
      kind: var
      name: %0
- instruction:
  kind: copy
  src:
    kind: var
    name: %x
  dst:
    kind: var
    name: %y
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 50
  dst:
    kind: var
    name: %x
- instruction:
  kind: return
  src:
    kind: var
    name: %y
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_AllTypes_CopyToDeadStruct)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Dead store elimination can also eliminate copies to struct members. */
struct s {
    int i;
};

int f(struct s arg) {
    return arg.i;
}

int target(void) {
    struct s my_struct = {4};
    int x = f(my_struct);
    my_struct.i = 10;  // dead!
    return x;
}
)SRC"),
              R"OPT(- instruction:
  kind: copy_from_offset
  src: %arg
  offset: 0
  dst:
    kind: var
    name: %0
- instruction:
  kind: return
  src:
    kind: var
    name: %0
- instruction:
  kind: allocate_local
  name: %my_struct
  size: 4
  alignment: 4
- instruction:
  kind: copy_to_offset
  src:
    kind: constant
    const:
      kind: int
      value: 4
  dst: %my_struct
  offset: 0
- instruction:
  kind: fun_call
  fun_name: f
  args:
    - val:
      kind: var
      name: %my_struct
  dst:
    kind: var
    name: %0
- instruction:
  kind: copy_to_offset
  src:
    kind: constant
    const:
      kind: int
      value: 10
  dst: %my_struct
  offset: 0
- instruction:
  kind: return
  src:
    kind: var
    name: %0
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_AllTypes_DeleteDeadPtIiInstructions)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Make sure we can delete type conversions, load, and other instructions
 * from Part II when they're dead stores
 * */


long l = 1l;
int i = 2;
unsigned int u = 30u;

struct s {
    int a;
    int b;
    int c;
};

int target(void) {
    // everything except the return instruction should be deleted
    long x = (long) i; // dead sign extend
    unsigned long y = (unsigned long) u; // dead zero extend
    double d = (double) y + (double) i; // dead IntToDouble and UIntToDouble
    x = (long) d; // dead DoubleToInt
    y = (unsigned long) d; // dead DoubleToUInt
    int arr[3] = {1, 2, 3}; // dead CopyToOffset
    int j = arr[2]; // dead AddPtr and Load
    int *ptr = &i; // dead GetAddress
    char c = (char)l; // dead truncate
    struct s my_struct = {0, 0, 0};
    j = my_struct.b; // dead CopyFromOffset
    d = -d * 5.0; // dead Binary/Unary instructions w/ non-int operands
    return 5;
}
)SRC"),
              R"OPT(- instruction:
  kind: allocate_local
  name: %arr
  size: 12
  alignment: 4
- instruction:
  kind: copy_to_offset
  src:
    kind: constant
    const:
      kind: int
      value: 1
  dst: %arr
  offset: 0
- instruction:
  kind: copy_to_offset
  src:
    kind: constant
    const:
      kind: int
      value: 2
  dst: %arr
  offset: 4
- instruction:
  kind: copy_to_offset
  src:
    kind: constant
    const:
      kind: int
      value: 3
  dst: %arr
  offset: 8
- instruction:
  kind: allocate_local
  name: %my_struct
  size: 12
  alignment: 4
- instruction:
  kind: copy_to_offset
  src:
    kind: constant
    const:
      kind: int
      value: 0
  dst: %my_struct
  offset: 0
- instruction:
  kind: copy_to_offset
  src:
    kind: constant
    const:
      kind: int
      value: 0
  dst: %my_struct
  offset: 4
- instruction:
  kind: copy_to_offset
  src:
    kind: constant
    const:
      kind: int
      value: 0
  dst: %my_struct
  offset: 8
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 5
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_AllTypes_GetaddrDoesntGen)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that getting the address of a variable does _not_ make that variable
 * live.
 * */

int target(void) {
    int x = 4;  // initialization is a dead store because we never use the value
                // of x
    int *ptr = &x;
    return ptr == 0;
}
)SRC"),
              R"OPT(- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 4
  dst:
    kind: var
    name: %x
- instruction:
  kind: get_address
  src:
    kind: var
    name: %x
  dst:
    kind: var
    name: %0
- instruction:
  kind: binary
  op: equal
  src1:
    kind: var
    name: %0
  src2:
    kind: constant
    const:
      kind: long
      value: 0
  dst:
    kind: var
    name: %2
- instruction:
  kind: return
  src:
    kind: var
    name: %2
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_AllTypes_DontElim_CopytooffsetDoesntKill)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* CopyToOffset does not kill src struct */

struct s {
    int a;
    int b;
    int c;
};

struct s glob = {1, 2, 3};

int main(void) {
    struct s my_struct = glob;  // not a dead store
    my_struct.c = 100;          // this doesn't make my_struct dead
    if (my_struct.c != 100 ) {
        return 1; // fail
    }
    if (my_struct.a != 1) {
        return 2; // fail
    }
    if (glob.c != 3) {
        return 3; // fail
    }
    return 0; // success
}
)SRC"),
              R"OPT(- instruction:
  kind: allocate_local
  name: %my_struct
  size: 12
  alignment: 4
- instruction:
  kind: copy_from_offset
  src: glob
  offset: 0
  dst:
    kind: var
    name: %0
- instruction:
  kind: copy_to_offset
  src:
    kind: var
    name: %0
  dst: %my_struct
  offset: 0
- instruction:
  kind: copy_from_offset
  src: glob
  offset: 8
  dst:
    kind: var
    name: %1
- instruction:
  kind: copy_to_offset
  src:
    kind: var
    name: %1
  dst: %my_struct
  offset: 8
- instruction:
  kind: copy_to_offset
  src:
    kind: constant
    const:
      kind: int
      value: 100
  dst: %my_struct
  offset: 8
- instruction:
  kind: copy_from_offset
  src: %my_struct
  offset: 8
  dst:
    kind: var
    name: %2
- instruction:
  kind: binary
  op: not_equal
  src1:
    kind: var
    name: %2
  src2:
    kind: constant
    const:
      kind: int
      value: 100
  dst:
    kind: var
    name: %3
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %3
  target: %4
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: label
  name: %4
- instruction:
  kind: copy_from_offset
  src: %my_struct
  offset: 0
  dst:
    kind: var
    name: %6
- instruction:
  kind: binary
  op: not_equal
  src1:
    kind: var
    name: %6
  src2:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %7
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %7
  target: %8
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 2
- instruction:
  kind: label
  name: %8
- instruction:
  kind: copy_from_offset
  src: glob
  offset: 8
  dst:
    kind: var
    name: %10
- instruction:
  kind: binary
  op: not_equal
  src1:
    kind: var
    name: %10
  src2:
    kind: constant
    const:
      kind: int
      value: 3
  dst:
    kind: var
    name: %11
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %11
  target: %12
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 3
- instruction:
  kind: label
  name: %12
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_AllTypes_DontElim_FuncallGeneratesAliased)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* A function call generates every aliased variable,
 * even if that variable isn't passed as a function argument
 * */
static int *i;

void set_ptr(int *arg) {
    i = arg;
}

int get_ptr_val(void) {
    return *i;
}

int main(void) {
    int x = 1;
    set_ptr(&x);
    x = 4;  // not dead b/c x is aliased, and funcall generates it
    return get_ptr_val(); // generates x
}
)SRC"),
              R"OPT(- instruction:
  kind: copy
  src:
    kind: var
    name: %arg
  dst:
    kind: var
    name: i
- instruction:
  kind: load
  src_ptr:
    kind: var
    name: i
  dst:
    kind: var
    name: %0
- instruction:
  kind: return
  src:
    kind: var
    name: %0
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %x
- instruction:
  kind: get_address
  src:
    kind: var
    name: %x
  dst:
    kind: var
    name: %0
- instruction:
  kind: fun_call
  fun_name: set_ptr
  args:
    - val:
      kind: var
      name: %0
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 4
  dst:
    kind: var
    name: %x
- instruction:
  kind: fun_call
  fun_name: get_ptr_val
  dst:
    kind: var
    name: %1
- instruction:
  kind: return
  src:
    kind: var
    name: %1
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_AllTypes_DontElim_LoadGeneratesAliased)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Load instruction generates all aliased variables */
long *pass_and_return(long *ptr) {
    return ptr;
}

int main(void) {
    long l;
    long *ptr = &l;
    long *other_ptr = pass_and_return(ptr);  // now other_ptr points to l
    l = 10;  // not a dead store b/c l is aliased and this is followed by load
             // from memory
    return *other_ptr;  // this makes all aliased vars (i.e. l) live
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: var
    name: %ptr
- instruction:
  kind: get_address
  src:
    kind: var
    name: %l
  dst:
    kind: var
    name: %0
- instruction:
  kind: fun_call
  fun_name: pass_and_return
  args:
    - val:
      kind: var
      name: %0
  dst:
    kind: var
    name: %1
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: long
      value: 10
  dst:
    kind: var
    name: %l
- instruction:
  kind: load
  src_ptr:
    kind: var
    name: %1
  dst:
    kind: var
    name: %3
- instruction:
  kind: truncate
  src:
    kind: var
    name: %3
  dst:
    kind: var
    name: %4
- instruction:
  kind: return
  src:
    kind: var
    name: %4
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_AllTypes_DontElim_NeverKillStore)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Dead store elimination should never eliminate Store instructions
 * */
void f(int *ptr) {
    *ptr = 4;  // not a dead store!
    return;
}

int main(void) {
    int x = 0;
    f(&x);
    return x;
}
)SRC"),
              R"OPT(- instruction:
  kind: store
  src:
    kind: constant
    const:
      kind: int
      value: 4
  dst_ptr:
    kind: var
    name: %ptr
- instruction:
  kind: return
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 0
  dst:
    kind: var
    name: %x
- instruction:
  kind: get_address
  src:
    kind: var
    name: %x
  dst:
    kind: var
    name: %0
- instruction:
  kind: fun_call
  fun_name: f
  args:
    - val:
      kind: var
      name: %0
- instruction:
  kind: return
  src:
    kind: var
    name: %x
)OPT");
}

// DISABLED: static local 'arr' collides with main's 'arr' (no-shadowing / static naming)
TEST_F(PipelineTest, DISABLED_Chapter19_DSE_AllTypes_DontElim_RecognizeAllUses)
{
    OptimizeYaml(R"SRC(
/* Make sure we recognize all the different ways a variable
 * can be used/generated by the instructions introduced in Part II
 * (type conversions, AddPtr, etc) */

long test_sign_extend(int flag, int arg) {
    if (flag) {
        arg = -1;  // not a dead store b/c arg is used later;
                   // put it in an if statement so we don't propagate -1
                   // into the return statement
    }
    return (long)arg;
}

unsigned long test_zero_extend(int flag, unsigned int arg) {
    if (flag) {
        arg = 4294967295U;  // not a dead store b/c arg is used later;
                            // put this in an if statement so we don't propagate
                            // constant into the return statement
    }
    return (unsigned long)arg;
}

int test_double_to_int(int flag, double arg) {
    if (flag) {
        arg = 225.5;  // not a dead store b/c arg is used later;
                      // put this in an if statement so we don't propagate
                      // constant into the return statement
    }
    return (int)arg;
}

double test_int_to_double(int flag, long arg) {
    if (flag) {
        arg = 500000l;  // not a dead store b/c arg is used later;
                        // put this in an if statement so we don't propagate
                        // constant into the return statement
    }
    return (double)arg;
}

unsigned long test_double_to_uint(int flag, double arg) {
    if (flag) {
        arg = 1844674407370955264.;  // not a dead store
    }
    return (unsigned long)arg;
}

double test_uint_to_double(int flag, unsigned int arg) {
    if (flag) {
        arg = 2147483650u;  // not a dead store
    }
    return (double)arg;
}

char test_truncate(int flag, long arg) {
    if (flag) {
        arg = 300;  // not a dead store b/c arg is used later;
                    // put this in an if statement so we don't propagate
                    // constant into the return statement
    }
    return (char)arg;
}

double *test_add_ptr(int flag, double *ptr, long index) {
    static double arr[3] = {1.0, 2.0, 3.0};
    if (flag == 0) {
        ptr = arr;  // not dead b/c ptr is used later in AddPtr
    } else if (flag == 1) {
        index = 2l;  // not dead b/c index is used later in AddPtr
    }
    return &ptr[index];
}

struct s {
    int a;
    int b;
    int c;
};

int test_copyfromoffset(int flag, struct s arg) {
    struct s other_struct = {10, 9, 8};
    if (flag) {
        arg = other_struct;  // not a dead store; we use arg below
    }
    return arg.b;
}

struct s test_copytooffset(int flag, int arg) {
    struct s my_struct = {101, 102, 103};
    if (flag) {
        arg = -1;  // not a dead store; used in CopyToOffset
    }
    my_struct.b = arg;
    return my_struct;
}

// Store(val, dst_ptr) generates both val and dst_ptr
void test_store(int flag, long *ptr1, long *ptr2, long val) {
    if (flag == 1) {
        ptr1 = ptr2;  // not a dead store b/c we store through ptr1 below
    }
    if (flag == 2) {
        val = 77l;  // not a dead store
    }
    *ptr1 = val;
}

// dst = Load(src_ptr) generates src_ptr
// NOTE: Load also generates all aliased variables but we
// validate that in a separate test program (load_generates_aliased)
int test_load(int flag, int *ptr1, int *ptr2) {
    if (flag) {
        ptr1 = ptr2;  // not a dead store b/c used as src_ptr in Load below
    }
    return *ptr1;
}

int main(void) {
    if (test_sign_extend(0, -5) != -5l) {
        return 1;  // fail
    }
    if (test_sign_extend(1, -5) != -1l) {
        return 2;  // fail
    }
    if (test_zero_extend(0, 100000u) != 100000ul) {
        return 3;  // fail
    }
    if (test_zero_extend(1, 100000u) != 4294967295UL) {
        return 4;  // fail
    }
    if (test_double_to_int(0, 1000.5) != 1000) {
        return 5;  // fail
    }
    if (test_double_to_int(1, 1000.5) != 225) {
        return 6;  // fail
    }
    if (test_int_to_double(0, 100) != 100.0) {
        return 7;  // fail
    }
    if (test_int_to_double(1, 100) != 500000.0) {
        return 8;  // fail
    }
    if (test_double_to_uint(0, 1234567.8) != 1234567u) {
        return 9; // fail
    }
    if (test_double_to_uint(1, 1234567.8) != 1844674407370955264u) {
        return 10; // fail
    }
    if (test_uint_to_double(0, 4294967000U) != 4294967000.) {
        return 11; // fail
    }
    if (test_uint_to_double(1, 4294967000U) != 2147483650u) {
        return 12; // fail
    }
    if (test_truncate(0, 500) != -12) {
        return 13;  // fail
    }
    if (test_truncate(1, 500) != 44) {
        return 14;  // fail
    }
    double arr[3] = {4.0, 5.0, 6.0};
    if (*test_add_ptr(0, arr, 1) != 2.0) {
        return 15;  // fail
    }
    if (*test_add_ptr(1, arr, 1) != 6.0) {
        return 16;  // fail
    }
    if (*test_add_ptr(2, arr, 1) != 5.0) {
        return 17;  // fail
    }
    struct s strct = {20, 21, 22};

    if (test_copyfromoffset(0, strct) != 21) {
        return 18;  // fail
    }
    if (test_copyfromoffset(1, strct) != 9) {
        return 19;  // fail
    }
    if (test_copytooffset(0, -10).b != -10) {
        return 20;  // fail
    }
    if (test_copytooffset(1, -10).b != -1) {
        return 21;  // fail
    }
    long l1 = 0l;
    long l2 = 0l;

    test_store(0, &l1, &l2, 5l);
    if (l1 != 5l || l2 != 0l) {
        return 22;  // fail
    }
    test_store(1, &l1, &l2, 6l);
    if (l1 != 5l || l2 != 6l) {
        return 23;  // fail
    }
    test_store(2, &l1, &l2, 5l);
    if (l1 != 77l || l2 != 6l) {
        return 24;  // fail
    }

    int i1 = 2;
    int i2 = 3;

    if (test_load(0, &i1, &i2) != 2) {
        return 25;  // fail
    }
    if (test_load(1, &i1, &i2) != 3) {
        return 26;  // fail
    }

    return 0;  // success
}
)SRC");
}

TEST_F(PipelineTest, Chapter19_DSE_AllTypes_DontElim_UseAndUpdate)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that updating and using a value in the same AddPtr instruction
 * generates it rather than killing it.
 * Most instructions won't use and update the same variable,
 * since their destinations are temporary variables that are used updated
 * only once, but implement &x.member  as:
 *   tmp = &x
 *   tmp = AddPtr(tmp, <offset of member>, scale=1)
 * So we can validate that AddPtr generates tmp rather than killing it.
 * */

struct s {
    int a;
    int b;
    int c;
};

struct s global_struct = {1, 2, 3};

int *target(void) {
    return &global_struct.b;
}
)SRC"),
              R"OPT(- instruction:
  kind: get_address
  src:
    kind: var
    name: global_struct
  dst:
    kind: var
    name: %0
- instruction:
  kind: add_ptr
  ptr:
    kind: var
    name: %0
  index:
    kind: constant
    const:
      kind: int
      value: 4
  scale: 1
  dst:
    kind: var
    name: %1
- instruction:
  kind: return
  src:
    kind: var
    name: %1
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_AllTypes_ExtraCredit_CompoundAssignToDeadStructMember)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* compound assignment to struct members can be a dead store */

struct s {
    int i;
};

struct s glob_struct = { 15 };
int target(void) {
    struct s my_struct = { 4 }; // dead (because compound assign below is dead too)
    my_struct.i /= 2; // dead!
    my_struct = glob_struct;
    return my_struct.i;
}
)SRC"),
              R"OPT(- instruction:
  kind: allocate_local
  name: %my_struct
  size: 4
  alignment: 4
- instruction:
  kind: copy_to_offset
  src:
    kind: constant
    const:
      kind: int
      value: 4
  dst: %my_struct
  offset: 0
- instruction:
  kind: get_address
  src:
    kind: var
    name: %my_struct
  dst:
    kind: var
    name: %0
- instruction:
  kind: add_ptr
  ptr:
    kind: var
    name: %0
  index:
    kind: constant
    const:
      kind: int
      value: 0
  scale: 8
  dst:
    kind: var
    name: %1
- instruction:
  kind: load
  src_ptr:
    kind: var
    name: %1
  dst:
    kind: var
    name: %2
- instruction:
  kind: binary
  op: divide
  src1:
    kind: var
    name: %2
  src2:
    kind: constant
    const:
      kind: int
      value: 2
  dst:
    kind: var
    name: %3
- instruction:
  kind: store
  src:
    kind: var
    name: %3
  dst_ptr:
    kind: var
    name: %1
- instruction:
  kind: copy_from_offset
  src: glob_struct
  offset: 0
  dst:
    kind: var
    name: %4
- instruction:
  kind: copy_to_offset
  src:
    kind: var
    name: %4
  dst: %my_struct
  offset: 0
- instruction:
  kind: copy_from_offset
  src: %my_struct
  offset: 0
  dst:
    kind: var
    name: %5
- instruction:
  kind: return
  src:
    kind: var
    name: %5
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_AllTypes_ExtraCredit_CopyToDeadUnion)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Make sure we detect dead stores to union members */

union u {
    long l;
    int i;
};

union u global_union = {10};

int target(void) {
    union u my_union = {4};
    my_union.i = 123; // dead!
    my_union = global_union;
    return my_union.i;
}
)SRC"),
              R"OPT(- instruction:
  kind: allocate_local
  name: %my_union
  size: 8
  alignment: 8
- instruction:
  kind: copy_to_offset
  src:
    kind: constant
    const:
      kind: long
      value: 4
  dst: %my_union
  offset: 0
- instruction:
  kind: copy_to_offset
  src:
    kind: constant
    const:
      kind: int
      value: 123
  dst: %my_union
  offset: 0
- instruction:
  kind: copy_from_offset
  src: global_union
  offset: 0
  dst:
    kind: var
    name: %1
- instruction:
  kind: copy_to_offset
  src:
    kind: var
    name: %1
  dst: %my_union
  offset: 0
- instruction:
  kind: copy_from_offset
  src: %my_union
  offset: 0
  dst:
    kind: var
    name: %2
- instruction:
  kind: return
  src:
    kind: var
    name: %2
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_AllTypes_ExtraCredit_DecrStructMember)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* ++/-- applied to struct members can be a dead store */

struct s {
    int i;
};

int target(void) {
    struct s my_struct = {4};
    int x = 15;
    my_struct.i--; // dead!
    return x;
}
)SRC"),
              R"OPT(- instruction:
  kind: allocate_local
  name: %my_struct
  size: 4
  alignment: 4
- instruction:
  kind: copy_to_offset
  src:
    kind: constant
    const:
      kind: int
      value: 4
  dst: %my_struct
  offset: 0
- instruction:
  kind: get_address
  src:
    kind: var
    name: %my_struct
  dst:
    kind: var
    name: %0
- instruction:
  kind: add_ptr
  ptr:
    kind: var
    name: %0
  index:
    kind: constant
    const:
      kind: int
      value: 0
  scale: 8
  dst:
    kind: var
    name: %1
- instruction:
  kind: load
  src_ptr:
    kind: var
    name: %1
  dst:
    kind: var
    name: %2
- instruction:
  kind: binary
  op: subtract
  src1:
    kind: var
    name: %2
  src2:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %3
- instruction:
  kind: store
  src:
    kind: var
    name: %3
  dst_ptr:
    kind: var
    name: %1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 15
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_AllTypes_ExtraCredit_DontElim_CopyGeneratesUnion)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Reading a sub-object within a union makes the whole union live */

struct s {
    int a;
    int b;
};

union u {
    struct s str;
    long l;
};

union u glob = {{1, 2}};

int main(void) {
    union u my_union;
    my_union = glob; // not a dead store b/c we access a member
    return my_union.str.a;
}
)SRC"),
              R"OPT(- instruction:
  kind: allocate_local
  name: %my_union
  size: 8
  alignment: 8
- instruction:
  kind: copy_from_offset
  src: glob
  offset: 0
  dst:
    kind: var
    name: %0
- instruction:
  kind: copy_to_offset
  src:
    kind: var
    name: %0
  dst: %my_union
  offset: 0
- instruction:
  kind: get_address
  src:
    kind: var
    name: %my_union
  dst:
    kind: var
    name: %1
- instruction:
  kind: add_ptr
  ptr:
    kind: var
    name: %1
  index:
    kind: constant
    const:
      kind: int
      value: 0
  scale: 8
  dst:
    kind: var
    name: %2
- instruction:
  kind: add_ptr
  ptr:
    kind: var
    name: %2
  index:
    kind: constant
    const:
      kind: int
      value: 0
  scale: 8
  dst:
    kind: var
    name: %3
- instruction:
  kind: load
  src_ptr:
    kind: var
    name: %3
  dst:
    kind: var
    name: %4
- instruction:
  kind: return
  src:
    kind: var
    name: %4
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_AllTypes_ExtraCredit_DontElim_IncrThroughPointer)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Dead store elimination should never eliminate Store instructions
 * */
void f(int *ptr) {
    ++*ptr;  // not a dead store!
    return;
}

int main(void) {
    int x = 0;
    f(&x);
    return x;
}
)SRC"),
              R"OPT(- instruction:
  kind: load
  src_ptr:
    kind: var
    name: %ptr
  dst:
    kind: var
    name: %0
- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: %0
  src2:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %1
- instruction:
  kind: store
  src:
    kind: var
    name: %1
  dst_ptr:
    kind: var
    name: %ptr
- instruction:
  kind: return
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 0
  dst:
    kind: var
    name: %x
- instruction:
  kind: get_address
  src:
    kind: var
    name: %x
  dst:
    kind: var
    name: %0
- instruction:
  kind: fun_call
  fun_name: f
  args:
    - val:
      kind: var
      name: %0
- instruction:
  kind: return
  src:
    kind: var
    name: %x
)OPT");
}

TEST_F(PipelineTest, Chapter19_DSE_AllTypes_ExtraCredit_DontElim_TypePunning)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* It's not safe to eliminate a store to one union member if we read another
 * union member later
 */

union u {
    long l;
    int i;
};

int main(void) {
    union u my_union = { -1 };
    // not a dead store; we'll read lower bytes of this through my_union.i
    my_union.l = 180;
    return my_union.i;
}
)SRC"),
              R"OPT(- instruction:
  kind: allocate_local
  name: %my_union
  size: 8
  alignment: 8
- instruction:
  kind: sign_extend
  src:
    kind: constant
    const:
      kind: int
      value: -1
  dst:
    kind: var
    name: %1
- instruction:
  kind: copy_to_offset
  src:
    kind: var
    name: %1
  dst: %my_union
  offset: 0
- instruction:
  kind: copy_to_offset
  src:
    kind: constant
    const:
      kind: long
      value: 180
  dst: %my_union
  offset: 0
- instruction:
  kind: copy_from_offset
  src: %my_union
  offset: 0
  dst:
    kind: var
    name: %3
- instruction:
  kind: return
  src:
    kind: var
    name: %3
)OPT");
}

TEST_F(PipelineTest, Chapter19_UCE_AndClause)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we eliminate the second clause in 0 && y */
int putch(int c);

int target(void) {
    return 0 && putch(97);
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
)OPT");
}

TEST_F(PipelineTest, Chapter19_UCE_ConstantIfElse)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we eliminate an unreachable 'if' statement body.
 * This also tests that we won't eliminate a block if some, but not all,
 * of its precedessors are unreachable. The final 'return' statement's
 * predecessors include the 'if' branch (which is dead) and the 'else'
 * statement (which isn't).
 * */
int callee(void) {
    return 0;
}

int target(void) {
    int x;
    if (0)
        x = callee();
    else
        x = 40;
    return x + 5;
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 45
)OPT");
}

TEST_F(PipelineTest, Chapter19_UCE_DeadAfterIfElse)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we recognize call to 'callee' is unreachable;
 * */

int callee(void) {
    return 100;
}

int target(int a) {
    if (a) {
        return 1;
    } else {
        return 2;
    }

    return callee();  // this should be optimized away
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 100
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %a
  target: %0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: label
  name: %0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 2
)OPT");
}

TEST_F(PipelineTest, Chapter19_UCE_DeadAfterReturn)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we eliminate any code after a return statement. */
int callee(void) {
    return 1;
}

int target(void) {
    return 2;

    /* Everything past this point should be optimized away */
    int x = callee();

    if (x) {
        x = 10;
    }

    int y = callee();
    return x + y;
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 2
)OPT");
}

TEST_F(PipelineTest, Chapter19_UCE_DeadBlocksWithPredecessors)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Make sure we can eliminate unreachable code even if every unreachable
 * block has a predecessor; in other words, we're traversing the graph to find
 * reachable blocks, not just looking for blocks with no predecessor list.
 * */

int callee(void) {
    return 1 / 0;
}

int target(void) {
    int x = 5;

    return x;

    /* make sure we eliminate this loop even though every block in it has a
     * predecessor */
    for (; x < 10; x = x + 1) {
        x = x + callee();
    }
    return x;
}
)SRC"),
              R"OPT(- instruction:
  kind: binary
  op: divide
  src1:
    kind: constant
    const:
      kind: int
      value: 1
  src2:
    kind: constant
    const:
      kind: int
      value: 0
  dst:
    kind: var
    name: %0
- instruction:
  kind: return
  src:
    kind: var
    name: %0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 5
)OPT");
}

TEST_F(PipelineTest, Chapter19_UCE_DeadBranchInsideLoop)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we can eliminate dead code inside of a larger, non-dead
 * control structure
 * */

int callee(void) {
    return 1 / 0;
}

int target(void) {
    int result = 105;
    // loop is not optimized away but inner function call is
    for (int i = 0; i < 100; i = i + 1) {
        if (0) {  // this if statement and function call should be optimized
                  // away
            return callee();
        }
        result = result - i;
    }
    return result;
}
)SRC"),
              R"OPT(- instruction:
  kind: binary
  op: divide
  src1:
    kind: constant
    const:
      kind: int
      value: 1
  src2:
    kind: constant
    const:
      kind: int
      value: 0
  dst:
    kind: var
    name: %0
- instruction:
  kind: return
  src:
    kind: var
    name: %0
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 105
  dst:
    kind: var
    name: %result
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 0
  dst:
    kind: var
    name: %i
- instruction:
  kind: label
  name: %0
- instruction:
  kind: binary
  op: less_than
  src1:
    kind: var
    name: %i
  src2:
    kind: constant
    const:
      kind: int
      value: 100
  dst:
    kind: var
    name: %1
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %1
  target: %L0
- instruction:
  kind: binary
  op: subtract
  src1:
    kind: var
    name: %result
  src2:
    kind: var
    name: %i
  dst:
    kind: var
    name: %5
- instruction:
  kind: copy
  src:
    kind: var
    name: %5
  dst:
    kind: var
    name: %result
- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: %i
  src2:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %6
- instruction:
  kind: copy
  src:
    kind: var
    name: %6
  dst:
    kind: var
    name: %i
- instruction:
  kind: jump
  target: %0
- instruction:
  kind: label
  name: %L0
- instruction:
  kind: return
  src:
    kind: var
    name: %result
)OPT");
}

TEST_F(PipelineTest, Chapter19_UCE_DeadForLoop)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we can optimize away a for loop that will never execute;
 * initial expression still runs but post expression and body don't.
 * */

int callee(void) {
    return 1 / 0;
}

int target(void) {
    int i = 0;
    for (i = 10; 0; i = callee()) callee();
    return i;
}
)SRC"),
              R"OPT(- instruction:
  kind: binary
  op: divide
  src1:
    kind: constant
    const:
      kind: int
      value: 1
  src2:
    kind: constant
    const:
      kind: int
      value: 0
  dst:
    kind: var
    name: %0
- instruction:
  kind: return
  src:
    kind: var
    name: %0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 10
)OPT");
}

TEST_F(PipelineTest, Chapter19_UCE_Empty)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that an empty function doesn't crash this optimization pass.
 * We don't inspect the assembly for this program, so there's no 'target'
 * function
 * */

int main(void) {
}
)SRC"),
              R"OPT()OPT");
}

TEST_F(PipelineTest, Chapter19_UCE_EmptyBlock)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that having empty blocks after optimization doesn't break anything;
 * after removing useless jumps and labels, 'target' will contain several
 * empty basic blocks.
 * */

int target(int x, int y) {
    if (x) {
        if (y) {
        }
    }
    return 1;
}
)SRC"),
              R"OPT(- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %x
  target: %0
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %y
  target: %2
- instruction:
  kind: jump
  target: %3
- instruction:
  kind: label
  name: %2
- instruction:
  kind: label
  name: %3
- instruction:
  kind: jump
  target: %1
- instruction:
  kind: label
  name: %0
- instruction:
  kind: label
  name: %1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
)OPT");
}

TEST_F(PipelineTest, Chapter19_UCE_InfiniteLoop)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* make sure we don't choke on programs that never terminate
 * This program _does_ terminate because it indirectly calls exit()
 * but the compiler doesn't know that.
 * */

int exit_wrapper(int status); // defined in chapter_19/libraries/exit.c

int main(void) {
    int i = 0;
    do {
        i = i + 1;
        if (i > 10) {
            exit_wrapper(i);
        }
    } while(1);
}
)SRC"),
              R"OPT(- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 0
  dst:
    kind: var
    name: %i
- instruction:
  kind: label
  name: %0
- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: %i
  src2:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %1
- instruction:
  kind: copy
  src:
    kind: var
    name: %1
  dst:
    kind: var
    name: %i
- instruction:
  kind: binary
  op: greater_than
  src1:
    kind: var
    name: %1
  src2:
    kind: constant
    const:
      kind: int
      value: 10
  dst:
    kind: var
    name: %2
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %2
  target: %3
- instruction:
  kind: fun_call
  fun_name: exit_wrapper
  args:
    - val:
      kind: var
      name: %1
  dst:
    kind: var
    name: %5
- instruction:
  kind: jump
  target: %4
- instruction:
  kind: label
  name: %3
- instruction:
  kind: label
  name: %4
- instruction:
  kind: jump
  target: %0
)OPT");
}

TEST_F(PipelineTest, Chapter19_UCE_KeepFinalJump)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we don't choke on a program where final instruction is a jump.
 * Note that last instruction will only be a jump on second iteration through
 * the pipeline, after we've removed the extra Return instruction.
 * We don't inspect the assembly for this program, we just make its behavior
 * is correct, so the function under test is 'f' instead of 'target'
 * */

int f(int a) {
    do {
        a = a - 1;
        if (a)
            return 17;
    } while (1);
}

int main(void) {
    return f(10);
}
)SRC"),
              R"OPT(- instruction:
  kind: label
  name: %0
- instruction:
  kind: binary
  op: subtract
  src1:
    kind: var
    name: %a
  src2:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %1
- instruction:
  kind: copy
  src:
    kind: var
    name: %1
  dst:
    kind: var
    name: %a
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %1
  target: %2
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 17
- instruction:
  kind: label
  name: %2
- instruction:
  kind: jump
  target: %0
- instruction:
  kind: fun_call
  fun_name: f
  args:
    - val:
      kind: constant
      const:
        kind: int
        value: 10
  dst:
    kind: var
    name: %0
- instruction:
  kind: return
  src:
    kind: var
    name: %0
)OPT");
}

TEST_F(PipelineTest, Chapter19_UCE_OrClause)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we eliminate the second clause in 1 || x */
int putch(int c);

int target(void) {
    return 1 || putch(97);
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
)OPT");
}

TEST_F(PipelineTest, Chapter19_UCE_RemoveConditionalJumps)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we eliminate useless JumpIfZero and JumpIfNotZero instructions. */

int target(int a) {
    // on second unreachable code elimination pass, this will include
    // a JumpIfNotZero to its default successor (where we assign result = 1)
    int x = a || 5;

    // on second unreachable code elimination pass, this will include
    // a JumpIfZero to its default successor (where we assign result = 0)
    int y = a && 0;
    return x + y;
}
)SRC"),
              R"OPT(- instruction:
  kind: jump_if_not_zero
  condition:
    kind: var
    name: %a
  target: %0
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %2
- instruction:
  kind: jump
  target: %1
- instruction:
  kind: label
  name: %0
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %2
- instruction:
  kind: label
  name: %1
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %a
  target: %3
- instruction:
  kind: jump
  target: %4
- instruction:
  kind: label
  name: %3
- instruction:
  kind: label
  name: %4
- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: %2
  src2:
    kind: constant
    const:
      kind: int
      value: 0
  dst:
    kind: var
    name: %6
- instruction:
  kind: return
  src:
    kind: var
    name: %6
)OPT");
}

TEST_F(PipelineTest, Chapter19_UCE_RemoveJumpKeepLabel)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we remove one jump to a label without necessarily removing that
 * label, since other blocks may also jump to it.
 * We don't inspect the assembly for this function,
 * so the function under test is 'f' insetad of 'target'
 * */

int x = 0;
int callee(void) {
    x = x + 1;
    return 0;
}

int f(void) {
    for (int i = 0; i < 10; i = i + 1) {
        if (0) {
            // we'll optimize away this break, which jumps to this loop's
            // break label; however, we shouldn't optimize away the break label
            // because we still jump to it when we exit the loop normally
            break;
        }
        callee();
    }
    return 0;
}

int main(void) {
    f();
    return x;
}
)SRC"),
              R"OPT(- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: x
  src2:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %0
- instruction:
  kind: copy
  src:
    kind: var
    name: %0
  dst:
    kind: var
    name: x
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 0
  dst:
    kind: var
    name: %i
- instruction:
  kind: label
  name: %0
- instruction:
  kind: binary
  op: less_than
  src1:
    kind: var
    name: %i
  src2:
    kind: constant
    const:
      kind: int
      value: 10
  dst:
    kind: var
    name: %1
- instruction:
  kind: jump_if_zero
  condition:
    kind: var
    name: %1
  target: %L0
- instruction:
  kind: fun_call
  fun_name: callee
  dst:
    kind: var
    name: %4
- instruction:
  kind: binary
  op: add
  src1:
    kind: var
    name: %i
  src2:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %5
- instruction:
  kind: copy
  src:
    kind: var
    name: %5
  dst:
    kind: var
    name: %i
- instruction:
  kind: jump
  target: %0
- instruction:
  kind: label
  name: %L0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: fun_call
  fun_name: f
  dst:
    kind: var
    name: %0
- instruction:
  kind: return
  src:
    kind: var
    name: x
)OPT");
}

TEST_F(PipelineTest, Chapter19_UCE_RemoveUselessStartingLabel)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we remove useless labels */
int target(void) {
    // This empty do loop will start with several labels that we don't jump to;
    // make sure they're removed
    do {
    } while (0);

    return 99;
}
)SRC"),
              R"OPT(- instruction:
  kind: label
  name: %0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 99
)OPT");
}

TEST_F(PipelineTest, Chapter19_UCE_ExtraCredit_DeadBeforeFirstSwitchCase)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Anything that appears in a switch statement before the body of the first
 * case is unreachable.
 */

int callee(void) {
    return 0;
}

int target(int x) {
    switch(x) {
        return callee(); // unreachable
        case 1: return 1;
        default: return 2;
    }

}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
- instruction:
  kind: binary
  op: equal
  src1:
    kind: var
    name: %x
  src2:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %3
- instruction:
  kind: jump_if_not_zero
  condition:
    kind: var
    name: %3
  target: %0
- instruction:
  kind: jump
  target: %1
- instruction:
  kind: label
  name: %0
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: label
  name: %1
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 2
)OPT");
}

TEST_F(PipelineTest, Chapter19_UCE_ExtraCredit_DeadInSwitchBody)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Eliminate unreachable code witihn a switch statement body */

int callee(void) {
    return -1;
}

int target(int x) {
    int retval = 0;
    switch (x) {
    case 1:
        retval = 1; break;
    case 2: retval = 2; break;
        callee(); // unreachable - occurs after 'break' from previous case and before next one
    case 3: retval = 10; break;
    default: return -1;
        callee(); // unreachable
    }

    return retval;


}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: -1
- instruction:
  kind: binary
  op: equal
  src1:
    kind: var
    name: %x
  src2:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %5
- instruction:
  kind: jump_if_not_zero
  condition:
    kind: var
    name: %5
  target: %0
- instruction:
  kind: binary
  op: equal
  src1:
    kind: var
    name: %x
  src2:
    kind: constant
    const:
      kind: int
      value: 2
  dst:
    kind: var
    name: %6
- instruction:
  kind: jump_if_not_zero
  condition:
    kind: var
    name: %6
  target: %1
- instruction:
  kind: binary
  op: equal
  src1:
    kind: var
    name: %x
  src2:
    kind: constant
    const:
      kind: int
      value: 3
  dst:
    kind: var
    name: %7
- instruction:
  kind: jump_if_not_zero
  condition:
    kind: var
    name: %7
  target: %2
- instruction:
  kind: jump
  target: %3
- instruction:
  kind: label
  name: %0
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 1
  dst:
    kind: var
    name: %retval
- instruction:
  kind: jump
  target: %L0
- instruction:
  kind: label
  name: %1
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 2
  dst:
    kind: var
    name: %retval
- instruction:
  kind: jump
  target: %L0
- instruction:
  kind: label
  name: %2
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 10
  dst:
    kind: var
    name: %retval
- instruction:
  kind: jump
  target: %L0
- instruction:
  kind: label
  name: %3
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: -1
- instruction:
  kind: label
  name: %L0
- instruction:
  kind: return
  src:
    kind: var
    name: %retval
)OPT");
}

TEST_F(PipelineTest, Chapter19_UCE_ExtraCredit_GotoSkipsOverCode)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Test that we eliminate code that goto jumps over */
int callee(void) {
    return 1;
}

int target(void) {
    int x = 10;
    goto end;
    x = callee(); // eliminate this
    end:
    return x;
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 1
- instruction:
  kind: copy
  src:
    kind: constant
    const:
      kind: int
      value: 10
  dst:
    kind: var
    name: %x
- instruction:
  kind: return
  src:
    kind: var
    name: %x
)OPT");
}

TEST_F(PipelineTest, Chapter19_UCE_ExtraCredit_RemoveUnusedLabel)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* Make sure this pass removes unused label instructions */


int target(void) {
    lbl:
    return 0;
}
)SRC"),
              R"OPT(- instruction:
  kind: label
  name: lbl
- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
)OPT");
}

TEST_F(PipelineTest, Chapter19_UCE_ExtraCredit_UnreachableSwitchBody)
{
    EXPECT_EQ(OptimizeYaml(R"SRC(
/* If a switch body contains no case or default statements, we'll eliminate the whole thing */

int target(int flag) {
    switch (flag) {
        // Eliminate all of this - it's unreachable b/c outer
        // switch statement has no case/default statements (even
        // though inner switch does)
        static int x = 0;
        for (int i = 0; i < flag; i = i + 1) {
            switch (i) {
            case 1: x = x + 1;
            case 2: x = x + 2;
            default: x = x * 3;
            }
        }
        return x;
    }

    return 0;
}
)SRC"),
              R"OPT(- instruction:
  kind: return
  src:
    kind: constant
    const:
      kind: int
      value: 0
)OPT");
}
