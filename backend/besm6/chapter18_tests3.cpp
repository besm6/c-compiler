#include "book_run.h"

// strcmp + local char-array string init + 64-bit constants.
TEST_F(CodegenTest, DISABLED_Chapter18_ClassifyParams)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"PROG(
/* Test that we classify structure parameters correctly,
 * by passing a variety of structures as arguments.
 * Each test function takes only one argument.
 * */


int strcmp(char *s1, char *s2);

// from Listing 18-39
struct twelve_bytes {
    int i;
    char arr[8];
};  // two INTEGER eightbytes

// from Listing 18-40
struct inner {
    int i;
    char ch2;
};

struct nested_ints {
    char ch1;
    struct inner nested;
};  // two INTEGER eightbytes

// from Listing 18-41
struct flattened_ints {
    char c;
    int i;
    char a;
};  // two INTEGER eightbytes

// From uncaptioned listing in "Classifying Eightbytes" section
struct large {
    int i;
    double d;
    char arr[10];
};  // four MEMORY eightbytes

// Three structure declarations from Listing 18-42
struct two_ints {
    int i;
    int i2;
};  // one INTEGER eightbyte

struct nested_double {
    double array[1];
};  // one SSE eightbyte

struct two_eightbytes {
    double d;
    char c;
};  // one SSE eightbyte, one INTEGER eightbyte

// From Listing 18-47
struct pass_in_memory {
    double w;
    double x;
    int y;
    long z;
};  // four MEMORY eightbytes

// validation functions defined in library
int test_twelve_bytes(struct twelve_bytes s);
int test_nested_ints(struct nested_ints s);
int test_flattened_ints(struct flattened_ints s);
int test_large(struct large s);
int test_two_ints(struct two_ints s);
int test_nested_double(struct nested_double s);
int test_two_eightbytes(struct two_eightbytes s);
int test_pass_in_memory(struct pass_in_memory s);
/* Test that we classify structure parameters correctly,
 * by passing a variety of structures as arguments.
 * Each test function takes only one argument.
 * */


int main(void) {
    struct twelve_bytes s1 = {0, "lmnopqr"};
    if (!test_twelve_bytes(s1)) {
        return 1;
    }

    struct nested_ints s2 = {127, {2147483647, -128}};
    if (!test_nested_ints(s2)) {
        return 2;
    }

    struct flattened_ints s3 = {127, 2147483647, -128};
    if (!test_flattened_ints(s3)) {
        return 3;
    }

    struct large s4 = {200000, 23.25, "abcdefghi"};
    if (!test_large(s4)) {
        return 4;
    }

    struct two_ints s5 = {999, 888};
    if (!test_two_ints(s5)) {
        return 5;
    }

    struct nested_double s6 = {{25.125e3}};
    if (!test_nested_double(s6)) {
        return 6;
    }

    struct two_eightbytes s7 = {1000., 'x'};
    if (!test_two_eightbytes(s7)) {
        return 7;
    }

    struct pass_in_memory s8 = {1.7e308, -1.7e308, -2147483647, -9223372036854775807l};
    if (!test_pass_in_memory(s8)) {
        return 8;
    }

    return 0; // success
}
/* Test that we classify structure parameters correctly,
 * by passing a variety of structures as arguments.
 * Each test function takes only one argument.
 * */


int test_twelve_bytes(struct twelve_bytes s) {
    if (s.i != 0 || strcmp(s.arr, "lmnopqr")) {
        return 0;
    }
    return 1;  // success
}
int test_nested_ints(struct nested_ints s) {
    if (s.ch1 != 127 || s.nested.i != 2147483647 || s.nested.ch2 != -128) {
        return 0;
    }
    return 1;  // success
}
int test_flattened_ints(struct flattened_ints s) {
    if (s.c != 127 || s.i != 2147483647 || s.a != -128) {
        return 0;
    }

    return 1;  // success
}
int test_large(struct large s) {
    if (s.i != 200000 || s.d != 23.25 || strcmp(s.arr, "abcdefghi")) {
        return 0;
    }

    return 1;  // success
}
int test_two_ints(struct two_ints s) {
    if (s.i != 999 || s.i2 != 888) {
        return 0;
    }

    return 1;  // success
}
int test_nested_double(struct nested_double s) {
    if (s.array[0] != 25.125e3) {
        return 0;
    }

    return 1;  // success
}
int test_two_eightbytes(struct two_eightbytes s) {
    if (s.d != 1000. || s.c != 'x') {
        return 0;
    }

    return 1;  // success
}
int test_pass_in_memory(struct pass_in_memory s) {
    if (s.w != 1.7e308 || s.x != -1.7e308 || s.y != -2147483647 ||
        s.z != -9223372036854775807l) {
        return 0;
    }

    return 1;  // success
}
)PROG")));
}

// strcmp + local char-array string init + 64-bit constants.
TEST_F(CodegenTest, DISABLED_Chapter18_ParamCallingConventions)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"PROG(
/* Test that we can pass a mix of struct and non-struct arguments according to
 * the ABI */


int strcmp(char *s1, char *s2);
int strncmp(char *s1, char *s2, unsigned long n);

// This type comes from Listing 18-45
struct two_longs {
    long a;
    long b;
};

struct one_int {
    int i;
    char c;
};

struct one_int_exactly {
    unsigned long l;
};

struct two_ints {
    char c;
    int arr[3];
};

struct two_ints_nested {
    struct one_int a;
    struct one_int b;
};

struct twelve_bytes {
    int i;
    char arr[8];
};

struct one_xmm {
    double d;
};

struct two_xmm {
    double d[2];
};

struct int_and_xmm {
    char c;
    double d;
};

struct xmm_and_int {
    struct one_xmm dbl;
    char c[3];
};

struct odd_size {
    char arr[5];
};

struct memory {
    double d;
    char c[3];
    long l;
    int i;
};

// passing structures as parameters

// all arguments fit in registers
int pass_small_structs(struct two_xmm two_xmm_struct, struct one_int int_struct,
                       struct one_xmm xmm_struct,
                       struct xmm_and_int mixed_struct,
                       struct twelve_bytes int_struct_2,
                       struct one_int_exactly another_int_struct);

// based on example in Listing 18-45
int a_bunch_of_arguments(int i0, int i1, int i2, int i3, int i4,
                         struct two_longs param, int i5);

// use remaining structure types, mix with scalars
int structs_and_scalars(long l, double d, struct odd_size os, struct memory mem,
                        struct one_xmm xmm_struct);

// pass fourth_struct in memory b/c we're out of XMM registers
int struct_in_mem(double a, double b, double c, struct xmm_and_int first_struct,
                  double d, struct two_xmm second_struct, long l,
                  struct int_and_xmm third_struct,
                  struct one_xmm fourth_struct);

// pass two_ints_nested in memory - we have one general-purpose reg left for
// parameter passing but it requires two
int pass_borderline_struct_in_memory(struct two_ints t_i, int c,
                                     struct int_and_xmm i_x, void *ptr,
                                     struct two_ints_nested t_i_n, double d);

// pass a struct in memory that isn't neatly divisible by 8
int pass_uneven_struct_in_mem(struct twelve_bytes struct1, long a, long b,
                              struct twelve_bytes struct2, struct odd_size os,
                              struct memory m);

// pass first struct in memory, later structs in registers
int pass_later_structs_in_regs(struct memory m, struct twelve_bytes struct1, struct one_xmm struct2);
/* Test that we can pass a mix of struct and non-struct arguments according to
 * the ABI */


int main(void) {
    // define a bunch of structures
    struct two_longs two_longs = {1234567l, 89101112l};
    struct one_int one_int = {54320, 'c'};
    struct one_int_exactly one_long = {567890l};
    struct two_ints two_ints = {'_', {5, 6, 7}};
    struct two_ints_nested two_ints_nested = {one_int, one_int};
    struct twelve_bytes xii = {123, "string!"};

    struct one_xmm one_xmm = {5.125};
    struct two_xmm two_xmm = {{55.5, 44.4}};
    struct int_and_xmm int_and_xmm = {'p', 4.56};
    struct xmm_and_int xmm_and_int = {{1.234}, "hi"};

    struct odd_size odd = {"lmno"};
    struct memory mem = {15.75, "rs", 4444, 3333};

    // call validation functions

    if (!pass_small_structs(two_xmm, one_int, one_xmm, xmm_and_int, xii,
                            one_long)) {
        return 1;
    }

    // based on example in Listing 18-45
    if (!a_bunch_of_arguments(0, 1, 2, 3, 4, two_longs, 5)) {
        return 2;
    }

    if (!structs_and_scalars(10, 10.0, odd, mem, one_xmm)) {
        return 2;
    }

    if (!struct_in_mem(10.0, 11.125, 12.0, xmm_and_int, 13.0, two_xmm, 0,
                       int_and_xmm, one_xmm)) {
        return 3;
    }
    if (!pass_borderline_struct_in_memory(two_ints, '!', int_and_xmm, 0,
                                          two_ints_nested, 7.8)) {
        return 4;
    }

    // define some more structs to use in last two test cases
    struct twelve_bytes struct1 = {-1, {127, 126, 125}};
    struct twelve_bytes struct2 = {-5, {100, 101, 102}};
    struct odd_size os = {{100, 99, 98, 97, 96}};
    struct memory m = {5.345, {-1, -2, -3}, 4294967300l, 10000};
    if (!pass_uneven_struct_in_mem(struct1, 9223372036854775805l,
                                   9223372036854775800l, struct2, os, m)) {
        return 5;
    }

    if (!pass_later_structs_in_regs(m, struct1, one_xmm)) {
        return 6;
    }

    // success!
    return 0;
}
/* Test that we can pass a mix of struct and non-struct arguments according to
 * the ABI */


// all arguments fit in registers
int pass_small_structs(struct two_xmm two_xmm_struct, struct one_int int_struct,
                       struct one_xmm xmm_struct,
                       struct xmm_and_int mixed_struct,
                       struct twelve_bytes int_struct_2,
                       struct one_int_exactly another_int_struct) {
    if (two_xmm_struct.d[0] != 55.5 || two_xmm_struct.d[1] != 44.4)
        return 0;

    if (int_struct.c != 'c' || int_struct.i != 54320)
        return 0;
    if (xmm_struct.d != 5.125)
        return 0;
    if (strcmp(mixed_struct.c, "hi") || mixed_struct.dbl.d != 1.234)
        return 0;
    if (strcmp(int_struct_2.arr, "string!") || int_struct_2.i != 123)
        return 0;

    if (another_int_struct.l != 567890)
        return 0;

    return 1;  // success
}

// based on example in Listing 18-45
int a_bunch_of_arguments(int i0, int i1, int i2, int i3, int i4,
                         struct two_longs param, int i5) {
    if (i0 != 0 || i1 != 1 || i2 != 2 || i3 != 3 || i4 != 4 || i5 != 5) {
        return 0;
    }

    if (param.a != 1234567l || param.b != 89101112l) {
        return 0;
    }

    return 1;  // success
}

// use remaining structure types, mix with scalars
int structs_and_scalars(long l, double d, struct odd_size os, struct memory mem,
                        struct one_xmm xmm_struct) {
    if (l != 10)
        return 0;
    if (d != 10.0)
        return 0;
    if (strcmp(os.arr, "lmno"))
        return 0;
    if (strcmp(mem.c, "rs") || mem.d != 15.75 || mem.i != 3333 || mem.l != 4444)
        return 0;
    if (xmm_struct.d != 5.125)
        return 0;

    return 1;  // success
}

// pass fourth_struct in memory b/c we're out of XMM registers
int struct_in_mem(double a, double b, double c, struct xmm_and_int first_struct,
                  double d, struct two_xmm second_struct, long l,
                  struct int_and_xmm third_struct,
                  struct one_xmm fourth_struct) {
    if (a != 10.0 || b != 11.125 || c != 12.0)
        return 0;
    if (strcmp(first_struct.c, "hi") || first_struct.dbl.d != 1.234)
        return 0;
    if (d != 13.0)
        return 0;
    if (second_struct.d[0] != 55.5 || second_struct.d[1] != 44.4)
        return 0;
    if (l)
        return 0;
    if (third_struct.c != 'p' || third_struct.d != 4.56)
        return 0;
    if (fourth_struct.d != 5.125)
        return 0;

    return 1;  // success
}

// pass two_ints_nested in memory - we have one general-purpose reg left for
// parameter passing but it requires two
int pass_borderline_struct_in_memory(struct two_ints t_i, int c,
                                     struct int_and_xmm i_x, void *ptr,
                                     struct two_ints_nested t_i_n, double d) {
    if (t_i.c != '_' || t_i.arr[0] != 5 || t_i.arr[1] != 6 || t_i.arr[2] != 7)
        return 0;
    if (c != '!')
        return 0;
    if (i_x.c != 'p' || i_x.d != 4.56)
        return 0;

    if (ptr)
        return 0;

    if (t_i_n.a.c != 'c' || t_i_n.a.i != 54320)
        return 0;
    if (t_i_n.b.c != 'c' || t_i_n.b.i != 54320)
        return 0;
    if (d != 7.8)
        return 0;
    return 1;  // success
}

// pass a struct in memory that isn't neatly divisible by 8
int pass_uneven_struct_in_mem(struct twelve_bytes struct1, long a, long b,
                              struct twelve_bytes struct2, struct odd_size os,
                              struct memory m) {
    if (struct1.i != -1) {
        return 0;
    }
    if (struct1.arr[0] != 127 || struct1.arr[1] != 126 ||
        struct1.arr[2] != 125) {
        return 0;
    }
    if (a != 9223372036854775805l || b != 9223372036854775800l) {
        return 0;
    }
    if (struct2.i != -5) {
        return 0;
    }
    if (struct2.arr[0] != 100 || struct2.arr[1] != 101 ||
        struct2.arr[2] != 102) {
        return 0;
    }
    for (int i = 0; i < 5; i = i + 1) {
        if (os.arr[i] != 100 - i) {
            return 0;
        }
    }
    if (m.d != 5.345) {
        return 0;
    }
    if (m.c[0] != -1 || m.c[1] != -2 || m.c[2] != -3) {
        return 0;
    }
    if (m.l != 4294967300l) {
        return 0;
    }
    if (m.i != 10000) {
        return 0;
    }
    return 1;  // success
}

int pass_later_structs_in_regs(struct memory m, struct twelve_bytes struct1,
                               struct one_xmm struct2) {
    if (m.d != 5.345) {
        return 0;
    }

    if (m.c[0] != -1 || m.c[1] != -2 || m.c[2] != -3) {
        return 0;
    }

    if (m.l != 4294967300l) {
        return 0;
    }

    if (m.i != 10000) {
        return 0;
    }

    if (struct1.i != -1) {
        return 0;
    }
    if (struct1.arr[0] != 127 || struct1.arr[1] != 126 ||
        struct1.arr[2] != 125) {
        return 0;
    }

    if (struct2.d != 5.125) {
        return 0;
    }
    return 1;  // success
}
)PROG")));
}

// memcmp + packed sub-word char-array layout.
TEST_F(CodegenTest, DISABLED_Chapter18_StructSizes)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"PROG(
/* Test that we can pass static and automatic structs of every size between 1 and 24 bytes.
 * Pass each size both in a register (when possible) and on the stack. */


struct bytesize1 {
    unsigned char arr[1];
};

extern struct bytesize1 globvar_1;

struct bytesize2 {
    unsigned char arr[2];
};

extern struct bytesize2 globvar_2;

struct bytesize3 {
    unsigned char arr[3];
};

extern struct bytesize3 globvar_3;

struct bytesize4 {
    unsigned char arr[4];
};

extern struct bytesize4 globvar_4;

struct bytesize5 {
    unsigned char arr[5];
};

extern struct bytesize5 globvar_5;

struct bytesize6 {
    unsigned char arr[6];
};

extern struct bytesize6 globvar_6;

struct bytesize7 {
    unsigned char arr[7];
};

extern struct bytesize7 globvar_7;

struct bytesize8 {
    unsigned char arr[8];
};

extern struct bytesize8 globvar_8;

struct bytesize9 {
    unsigned char arr[9];
};

extern struct bytesize9 globvar_9;

struct bytesize10 {
    unsigned char arr[10];
};

extern struct bytesize10 globvar_10;

struct bytesize11 {
    unsigned char arr[11];
};

extern struct bytesize11 globvar_11;

struct bytesize12 {
    unsigned char arr[12];
};

extern struct bytesize12 globvar_12;

struct bytesize13 {
    unsigned char arr[13];
};

extern struct bytesize13 globvar_13;

struct bytesize14 {
    unsigned char arr[14];
};

extern struct bytesize14 globvar_14;

struct bytesize15 {
    unsigned char arr[15];
};

extern struct bytesize15 globvar_15;

struct bytesize16 {
    unsigned char arr[16];
};

extern struct bytesize16 globvar_16;

struct bytesize17 {
    unsigned char arr[17];
};

extern struct bytesize17 globvar_17;

struct bytesize18 {
    unsigned char arr[18];
};

extern struct bytesize18 globvar_18;

struct bytesize19 {
    unsigned char arr[19];
};

extern struct bytesize19 globvar_19;

struct bytesize20 {
    unsigned char arr[20];
};

extern struct bytesize20 globvar_20;

struct bytesize21 {
    unsigned char arr[21];
};

extern struct bytesize21 globvar_21;

struct bytesize22 {
    unsigned char arr[22];
};

extern struct bytesize22 globvar_22;

struct bytesize23 {
    unsigned char arr[23];
};

extern struct bytesize23 globvar_23;

struct bytesize24 {
    unsigned char arr[24];
};

extern struct bytesize24 globvar_24;

// Pass sizes 1 - 6 in registers, remainders on the stack
int fun0(struct bytesize1 a, struct bytesize2 b, struct bytesize3 c,
         struct bytesize4 d, struct bytesize5 e, struct bytesize6 f,
         struct bytesize7 g, struct bytesize8 h, struct bytesize9 i,
         struct bytesize10 j, struct bytesize11 k, struct bytesize12 l,
         struct bytesize13 m, struct bytesize14 n, struct bytesize15 o,
         struct bytesize16 p, struct bytesize17 q, struct bytesize18 r,
         struct bytesize19 s, struct bytesize20 t, struct bytesize21 u,
         struct bytesize22 v, struct bytesize23 w, struct bytesize24 x,
         unsigned char *a_expected, unsigned char *b_expected,
         unsigned char *c_expected, unsigned char *d_expected,
         unsigned char *e_expected, unsigned char *f_expected,
         unsigned char *g_expected, unsigned char *h_expected,
         unsigned char *i_expected, unsigned char *j_expected,
         unsigned char *k_expected, unsigned char *l_expected,
         unsigned char *m_expected, unsigned char *n_expected,
         unsigned char *o_expected, unsigned char *p_expected,
         unsigned char *q_expected, unsigned char *r_expected,
         unsigned char *s_expected, unsigned char *t_expected,
         unsigned char *u_expected, unsigned char *v_expected,
         unsigned char *w_expected, unsigned char *x_expected);

// Pass sizes 7-10 bytes in regs, 1-6 on the stack
int fun1(struct bytesize7 a, struct bytesize8 b, struct bytesize9 c,
         struct bytesize10 d, struct bytesize1 e, struct bytesize2 f,
         struct bytesize3 g, struct bytesize4 h, struct bytesize5 i,
         struct bytesize6 j, unsigned char *a_expected,
         unsigned char *b_expected, unsigned char *c_expected,
         unsigned char *d_expected, unsigned char *e_expected,
         unsigned char *f_expected, unsigned char *g_expected,
         unsigned char *h_expected, unsigned char *i_expected,
         unsigned char *j_expected);

// Pass sizes 11-13 in regs, 1 on the stack
int fun2(struct bytesize11 a, struct bytesize12 b, struct bytesize13 c,
         struct bytesize1 d, unsigned char *a_expected,
         unsigned char *b_expected, unsigned char *c_expected,
         unsigned char *d_expected);

// pass sizes 14-16 in regs, 2 on the stack
int fun3(struct bytesize14 a, struct bytesize15 b, struct bytesize16 c,
         struct bytesize2 d, unsigned char *a_expected,
         unsigned char *b_expected, unsigned char *c_expected,
         unsigned char *d_expected);
/* Test that we can pass static and automatic structs of every size between 1 and 24 bytes.
 * Pass each size both in a register (when possible) and on the stack. */

int main(void) {

    // pass global variables of each size
    if (!fun0(globvar_1, globvar_2, globvar_3, globvar_4, globvar_5, globvar_6,
             globvar_7, globvar_8, globvar_9, globvar_10, globvar_11,
             globvar_12, globvar_13, globvar_14, globvar_15, globvar_16,
             globvar_17, globvar_18, globvar_19, globvar_20, globvar_21,
             globvar_22, globvar_23, globvar_24, globvar_1.arr, globvar_2.arr,
             globvar_3.arr, globvar_4.arr, globvar_5.arr, globvar_6.arr,
             globvar_7.arr, globvar_8.arr, globvar_9.arr, globvar_10.arr,
             globvar_11.arr, globvar_12.arr, globvar_13.arr, globvar_14.arr,
             globvar_15.arr, globvar_16.arr, globvar_17.arr, globvar_18.arr,
             globvar_19.arr, globvar_20.arr, globvar_21.arr, globvar_22.arr,
             globvar_23.arr, globvar_24.arr)) {
        return 1;
    }

    if (!fun1(globvar_7, globvar_8, globvar_9, globvar_10, globvar_1, globvar_2,
             globvar_3, globvar_4, globvar_5, globvar_6, globvar_7.arr,
             globvar_8.arr, globvar_9.arr, globvar_10.arr, globvar_1.arr,
             globvar_2.arr, globvar_3.arr, globvar_4.arr, globvar_5.arr,
             globvar_6.arr)) {
        return 2;
    }

    if (!fun2(globvar_11, globvar_12, globvar_13, globvar_1, globvar_11.arr,
             globvar_12.arr, globvar_13.arr, globvar_1.arr)) {
        return 3;
    }

    if (!fun3(globvar_14, globvar_15, globvar_16, globvar_2, globvar_14.arr,
             globvar_15.arr, globvar_16.arr, globvar_2.arr)) {
        return 4;
    }

    // define local variables of each size
    struct bytesize1 locvar_1 = {{0}};

    struct bytesize2 locvar_2 = {{1, 2}};

    struct bytesize3 locvar_3 = {{3, 4, 5}};

    struct bytesize4 locvar_4 = {{6, 7, 8, 9}};

    struct bytesize5 locvar_5 = {{10, 11, 12, 13, 14}};

    struct bytesize6 locvar_6 = {{15, 16, 17, 18, 19, 20}};

    struct bytesize7 locvar_7 = {{21, 22, 23, 24, 25, 26, 27}};

    struct bytesize8 locvar_8 = {{28, 29, 30, 31, 32, 33, 34, 35}};

    struct bytesize9 locvar_9 = {{36, 37, 38, 39, 40, 41, 42, 43, 44}};

    struct bytesize10 locvar_10 = {{45, 46, 47, 48, 49, 50, 51, 52, 53, 54}};

    struct bytesize11 locvar_11 = {
        {55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65}};

    struct bytesize12 locvar_12 = {
        {66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77}};

    struct bytesize13 locvar_13 = {
        {78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90}};

    struct bytesize14 locvar_14 = {
        {91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104}};

    struct bytesize15 locvar_15 = {{105, 106, 107, 108, 109, 110, 111, 112, 113,
                                    114, 115, 116, 117, 118, 119}};

    struct bytesize16 locvar_16 = {{120, 121, 122, 123, 124, 125, 126, 127, 128,
                                    129, 130, 131, 132, 133, 134, 135}};

    struct bytesize17 locvar_17 = {{136, 137, 138, 139, 140, 141, 142, 143, 144,
                                    145, 146, 147, 148, 149, 150, 151, 152}};

    struct bytesize18 locvar_18 = {{153, 154, 155, 156, 157, 158, 159, 160, 161,
                                    162, 163, 164, 165, 166, 167, 168, 169,
                                    170}};

    struct bytesize19 locvar_19 = {{171, 172, 173, 174, 175, 176, 177, 178, 179,
                                    180, 181, 182, 183, 184, 185, 186, 187, 188,
                                    189}};

    struct bytesize20 locvar_20 = {{190, 191, 192, 193, 194, 195, 196,
                                    197, 198, 199, 200, 201, 202, 203,
                                    204, 205, 206, 207, 208, 209}};

    struct bytesize21 locvar_21 = {{210, 211, 212, 213, 214, 215, 216,
                                    217, 218, 219, 220, 221, 222, 223,
                                    224, 225, 226, 227, 228, 229, 230}};

    struct bytesize22 locvar_22 = {{231, 232, 233, 234, 235, 236, 237, 238,
                                    239, 240, 241, 242, 243, 244, 245, 246,
                                    247, 248, 249, 250, 251, 252}};

    struct bytesize23 locvar_23 = {{253, 254, 255, 0,  1,  2,  3,  4,
                                    5,   6,   7,   8,  9,  10, 11, 12,
                                    13,  14,  15,  16, 17, 18, 19}};

    struct bytesize24 locvar_24 = {{20, 21, 22, 23, 24, 25, 26, 27,
                                    28, 29, 30, 31, 32, 33, 34, 35,
                                    36, 37, 38, 39, 40, 41, 42, 43}};

    // pass local variables of each size
    if (!fun0(locvar_1, locvar_2, locvar_3, locvar_4, locvar_5, locvar_6,
             locvar_7, locvar_8, locvar_9, locvar_10, locvar_11, locvar_12,
             locvar_13, locvar_14, locvar_15, locvar_16, locvar_17, locvar_18,
             locvar_19, locvar_20, locvar_21, locvar_22, locvar_23, locvar_24,
             locvar_1.arr, locvar_2.arr, locvar_3.arr, locvar_4.arr,
             locvar_5.arr, locvar_6.arr, locvar_7.arr, locvar_8.arr,
             locvar_9.arr, locvar_10.arr, locvar_11.arr, locvar_12.arr,
             locvar_13.arr, locvar_14.arr, locvar_15.arr, locvar_16.arr,
             locvar_17.arr, locvar_18.arr, locvar_19.arr, locvar_20.arr,
             locvar_21.arr, locvar_22.arr, locvar_23.arr, locvar_24.arr)) {
        return 5;
    }

    if (!fun1(locvar_7, locvar_8, locvar_9, locvar_10, locvar_1, locvar_2,
             locvar_3, locvar_4, locvar_5, locvar_6, locvar_7.arr, locvar_8.arr,
             locvar_9.arr, locvar_10.arr, locvar_1.arr, locvar_2.arr,
             locvar_3.arr, locvar_4.arr, locvar_5.arr, locvar_6.arr)) {
        return 6;
    }

    if (!fun2(locvar_11, locvar_12, locvar_13, locvar_1, locvar_11.arr,
             locvar_12.arr, locvar_13.arr, locvar_1.arr)) {
        return 7;
    }

    if (!fun3(locvar_14, locvar_15, locvar_16, locvar_2, locvar_14.arr,
             locvar_15.arr, locvar_16.arr, locvar_2.arr)) {
        return 8;
    }

    return 0;
}

struct bytesize1 globvar_1 = {{0}};

struct bytesize2 globvar_2 = {{1, 2}};

struct bytesize3 globvar_3 = {{3, 4, 5}};

struct bytesize4 globvar_4 = {{6, 7, 8, 9}};

struct bytesize5 globvar_5 = {{10, 11, 12, 13, 14}};

struct bytesize6 globvar_6 = {{15, 16, 17, 18, 19, 20}};

struct bytesize7 globvar_7 = {{21, 22, 23, 24, 25, 26, 27}};

struct bytesize8 globvar_8 = {{28, 29, 30, 31, 32, 33, 34, 35}};

struct bytesize9 globvar_9 = {{36, 37, 38, 39, 40, 41, 42, 43, 44}};

struct bytesize10 globvar_10 = {{45, 46, 47, 48, 49, 50, 51, 52, 53, 54}};

struct bytesize11 globvar_11 = {{55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65}};

struct bytesize12 globvar_12 = {
    {66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77}};

struct bytesize13 globvar_13 = {
    {78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90}};

struct bytesize14 globvar_14 = {
    {91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104}};

struct bytesize15 globvar_15 = {{105, 106, 107, 108, 109, 110, 111, 112, 113,
                                 114, 115, 116, 117, 118, 119}};

struct bytesize16 globvar_16 = {{120, 121, 122, 123, 124, 125, 126, 127, 128,
                                 129, 130, 131, 132, 133, 134, 135}};

struct bytesize17 globvar_17 = {{136, 137, 138, 139, 140, 141, 142, 143, 144,
                                 145, 146, 147, 148, 149, 150, 151, 152}};

struct bytesize18 globvar_18 = {{153, 154, 155, 156, 157, 158, 159, 160, 161,
                                 162, 163, 164, 165, 166, 167, 168, 169, 170}};

struct bytesize19 globvar_19 = {{171, 172, 173, 174, 175, 176, 177, 178, 179,
                                 180, 181, 182, 183, 184, 185, 186, 187, 188,
                                 189}};

struct bytesize20 globvar_20 = {{190, 191, 192, 193, 194, 195, 196,
                                 197, 198, 199, 200, 201, 202, 203,
                                 204, 205, 206, 207, 208, 209}};

struct bytesize21 globvar_21 = {{210, 211, 212, 213, 214, 215, 216,
                                 217, 218, 219, 220, 221, 222, 223,
                                 224, 225, 226, 227, 228, 229, 230}};

struct bytesize22 globvar_22 = {{231, 232, 233, 234, 235, 236, 237, 238,
                                 239, 240, 241, 242, 243, 244, 245, 246,
                                 247, 248, 249, 250, 251, 252}};

struct bytesize23 globvar_23 = {{253, 254, 255, 0,  1,  2,  3,  4,
                                 5,   6,   7,   8,  9,  10, 11, 12,
                                 13,  14,  15,  16, 17, 18, 19}};

struct bytesize24 globvar_24 = {{20, 21, 22, 23, 24, 25, 26, 27,
                                 28, 29, 30, 31, 32, 33, 34, 35,
                                 36, 37, 38, 39, 40, 41, 42, 43}};
/* Test that we can pass static and automatic structs of every size between 1 and 24 bytes.
 * Pass each size both in a register (when possible) and on the stack. */

int memcmp(void *s1, void *s2, unsigned long n);

// Pass sizes 1 - 6 in registers, remainders on the stack
int fun0(struct bytesize1 a, struct bytesize2 b, struct bytesize3 c,
         struct bytesize4 d, struct bytesize5 e, struct bytesize6 f,
         struct bytesize7 g, struct bytesize8 h, struct bytesize9 i,
         struct bytesize10 j, struct bytesize11 k, struct bytesize12 l,
         struct bytesize13 m, struct bytesize14 n, struct bytesize15 o,
         struct bytesize16 p, struct bytesize17 q, struct bytesize18 r,
         struct bytesize19 s, struct bytesize20 t, struct bytesize21 u,
         struct bytesize22 v, struct bytesize23 w, struct bytesize24 x,
         unsigned char *a_expected, unsigned char *b_expected,
         unsigned char *c_expected, unsigned char *d_expected,
         unsigned char *e_expected, unsigned char *f_expected,
         unsigned char *g_expected, unsigned char *h_expected,
         unsigned char *i_expected, unsigned char *j_expected,
         unsigned char *k_expected, unsigned char *l_expected,
         unsigned char *m_expected, unsigned char *n_expected,
         unsigned char *o_expected, unsigned char *p_expected,
         unsigned char *q_expected, unsigned char *r_expected,
         unsigned char *s_expected, unsigned char *t_expected,
         unsigned char *u_expected, unsigned char *v_expected,
         unsigned char *w_expected, unsigned char *x_expected) {
    if (memcmp(&a, a_expected, sizeof a)) {
        return 0;
    }

    if (memcmp(&b, b_expected, sizeof b)) {
        return 0;
    }

    if (memcmp(&c, c_expected, sizeof c)) {
        return 0;
    }

    if (memcmp(&d, d_expected, sizeof d)) {
        return 0;
    }

    if (memcmp(&e, e_expected, sizeof e)) {
        return 0;
    }

    if (memcmp(&f, f_expected, sizeof f)) {
        return 0;
    }

    if (memcmp(&g, g_expected, sizeof g)) {
        return 0;
    }

    if (memcmp(&h, h_expected, sizeof h)) {
        return 0;
    }

    if (memcmp(&i, i_expected, sizeof i)) {
        return 0;
    }

    if (memcmp(&j, j_expected, sizeof j)) {
        return 0;
    }

    if (memcmp(&k, k_expected, sizeof k)) {
        return 0;
    }

    if (memcmp(&l, l_expected, sizeof l)) {
        return 0;
    }

    if (memcmp(&m, m_expected, sizeof m)) {
        return 0;
    }

    if (memcmp(&n, n_expected, sizeof n)) {
        return 0;
    }

    if (memcmp(&o, o_expected, sizeof o)) {
        return 0;
    }

    if (memcmp(&p, p_expected, sizeof p)) {
        return 0;
    }

    if (memcmp(&q, q_expected, sizeof q)) {
        return 0;
    }

    if (memcmp(&r, r_expected, sizeof r)) {
        return 0;
    }

    if (memcmp(&s, s_expected, sizeof s)) {
        return 0;
    }

    if (memcmp(&t, t_expected, sizeof t)) {
        return 0;
    }

    if (memcmp(&u, u_expected, sizeof u)) {
        return 0;
    }

    if (memcmp(&v, v_expected, sizeof v)) {
        return 0;
    }

    if (memcmp(&w, w_expected, sizeof w)) {
        return 0;
    }

    if (memcmp(&x, x_expected, sizeof x)) {
        return 0;
    }

    return 1; // success
}

// Pass sizes 7-10 bytes in regs, 1-6 on the stack
int fun1(struct bytesize7 a, struct bytesize8 b, struct bytesize9 c,
         struct bytesize10 d, struct bytesize1 e, struct bytesize2 f,
         struct bytesize3 g, struct bytesize4 h, struct bytesize5 i,
         struct bytesize6 j, unsigned char *a_expected,
         unsigned char *b_expected, unsigned char *c_expected,
         unsigned char *d_expected, unsigned char *e_expected,
         unsigned char *f_expected, unsigned char *g_expected,
         unsigned char *h_expected, unsigned char *i_expected,
         unsigned char *j_expected) {
    if (memcmp(&a, a_expected, sizeof a)) {
        return 0;
    }

    if (memcmp(&b, b_expected, sizeof b)) {
        return 0;
    }

    if (memcmp(&c, c_expected, sizeof c)) {
        return 0;
    }

    if (memcmp(&d, d_expected, sizeof d)) {
        return 0;
    }

    if (memcmp(&e, e_expected, sizeof e)) {
        return 0;
    }

    if (memcmp(&f, f_expected, sizeof f)) {
        return 0;
    }

    if (memcmp(&g, g_expected, sizeof g)) {
        return 0;
    }

    if (memcmp(&h, h_expected, sizeof h)) {
        return 0;
    }

    if (memcmp(&i, i_expected, sizeof i)) {
        return 0;
    }

    if (memcmp(&j, j_expected, sizeof j)) {
        return 0;
    }

    return 1; // success
}

// Pass sizes 11-13 in regs, 1 on the stack
int fun2(struct bytesize11 a, struct bytesize12 b, struct bytesize13 c,
         struct bytesize1 d, unsigned char *a_expected,
         unsigned char *b_expected, unsigned char *c_expected,
         unsigned char *d_expected) {
    if (memcmp(&a, a_expected, sizeof a)) {
        return 0;
    }

    if (memcmp(&b, b_expected, sizeof b)) {
        return 0;
    }

    if (memcmp(&c, c_expected, sizeof c)) {
        return 0;
    }

    if (memcmp(&d, d_expected, sizeof d)) {
        return 0;
    }

    return 1; // success
}

// pass sizes 14-16 in regs, 2 on the stack
int fun3(struct bytesize14 a, struct bytesize15 b, struct bytesize16 c,
         struct bytesize2 d, unsigned char *a_expected,
         unsigned char *b_expected, unsigned char *c_expected,
         unsigned char *d_expected) {
    if (memcmp(&a, a_expected, sizeof a)) {
        return 0;
    }

    if (memcmp(&b, b_expected, sizeof b)) {
        return 0;
    }

    if (memcmp(&c, c_expected, sizeof c)) {
        return 0;
    }

    if (memcmp(&d, d_expected, sizeof d)) {
        return 0;
    }

    return 1; // success
}
)PROG")));
}

// calloc + block-scope static.
TEST_F(CodegenTest, DISABLED_Chapter18_AccessRetvalMembers)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"PROG(
/* Test for accessing the members in a return value of structure type */
struct inner {
    char x;
    long y;
};

struct outer {
    double d;
    struct inner *ptr;
    struct inner s;
};

void *calloc(unsigned long nmemb, unsigned long size);

struct inner return_small_struct(void);
struct outer return_nested_struct(void);
/* Test for accessing the members in a return value of structure type */

int main(void) {
    // get member in a non-nested struct
    if (return_small_struct().y != 102) {
        return 1;
    }

    // get members in nested struct
    if (return_nested_struct().d != 2.0 || return_nested_struct().s.x != 10 ||
        return_nested_struct().s.y != 11) {
        return 3;
    }

    // get members thru pointer in nested struct
    if (return_nested_struct().ptr->x != 12 ||
        return_nested_struct().ptr->y != 13) {
        return 4;
    }

    // update members through pointer in nested struct
    return_nested_struct().ptr->x = 70;
    return_nested_struct().ptr->y = 71;

    // validate updated values
    if (return_nested_struct().ptr->x != 70 ||
        return_nested_struct().ptr->y != 71) {
        return 5;
    }

    return 0;  // success
}
/* Test for accessing the members in a return value of structure type */

struct inner return_small_struct(void) {
    struct inner i = {101, 102};
    return i;
}

struct outer return_nested_struct(void) {
    static struct outer ret = {2.0, 0, {10, 11}};

    // on first call to this function, initializer ret.ptr
    if (!ret.ptr) {
        ret.ptr = calloc(1, sizeof(struct inner));
        ret.ptr->x = 12;
        ret.ptr->y = 13;
    }

    return ret;
}
)PROG")));
}

// strcmp + local char-array string init + 64-bit constants.
TEST_F(CodegenTest, DISABLED_Chapter18_ReturnCallingConventions)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"PROG(
/* Test that we return a wide range of struct types according to the ABI */

int strcmp(char *s1, char *s2);
int strncmp(char *s1, char *s2, unsigned long n);

struct one_int {
    int i;
    char c;
};

struct one_int_exactly {
    unsigned long l;
};

struct two_ints {
    char c;
    int arr[3];
};

struct two_ints_nested {
    struct one_int a;
    struct one_int b;
};

struct twelve_bytes {
    int i;
    char arr[8];
};

struct one_xmm {
    double d;
};

struct two_xmm {
    double d[2];
};

struct int_and_xmm {
    char c;
    double d;
};

struct xmm_and_int {
    struct one_xmm dbl;
    char c[3];
};

struct odd_size {
    char arr[5];
};

struct memory {
    double d;
    char c[3];
    long l;
    int i;
};

// returning structures

struct one_int return_int_struct(void);
struct twelve_bytes return_two_int_struct(void);
struct one_xmm return_double_struct(void);
struct two_xmm return_two_double_struct(void);
struct xmm_and_int return_mixed(void);
struct int_and_xmm return_mixed2(void);
struct memory return_on_stack(void);

// return on stack + pass other int params
struct memory pass_and_return_regs(int i, double d, struct int_and_xmm strct,
                                   int c, struct two_ints t_i, long l,
                                   struct one_int_exactly o_i_e, int c2);
/* Test that we return a wide range of struct types according to the ABI */

int main(void) {
    struct one_int_exactly one_long = {567890l};
    struct two_ints two_ints = {'_', {5, 6, 7}};
    struct int_and_xmm int_and_xmm = {'p', 4.56};

    // returning structures

    struct one_int s1 = return_int_struct();
    if (s1.i != 1 || s1.c != 2) {
        return 1;
    }

    struct twelve_bytes s2 = return_two_int_struct();
    if (s2.i != 10 || strncmp(s2.arr, "12345678", sizeof s2.arr))
        return 2;

    struct one_xmm s3 = return_double_struct();
    if (s3.d != 100.625)
        return 3;
    struct two_xmm s4 = return_two_double_struct();
    if (s4.d[0] != 8.8 || s4.d[1] != 7.8)
        return 4;

    struct xmm_and_int s5 = return_mixed();
    if (s5.dbl.d != 10.0 || strcmp(s5.c, "ab"))
        return 5;

    struct int_and_xmm s6 = return_mixed2();
    if (s6.c != 127 || s6.d != 34e43)
        return 6;

    struct memory s7 = return_on_stack();
    if (s7.d != 1.25 || strcmp(s7.c, "xy") || s7.l != 100l || s7.i != 44)
        return 7;

    s7 = pass_and_return_regs(6, 4.0, int_and_xmm, 5, two_ints, 77, one_long,
                              99);
    // something was clobbered or set incorrectly in retval
    if (s7.d || s7.c[0] || s7.c[1] || s7.c[2])
        return 8;

    // i was set to indicate problem w/ parameter passing
    if (s7.i)
        return 9;

    if (s7.l != 100)
        return 10;  // l field was clobbered or set incorrectly

    // success!
    return 0;
}
/* Test that we return a wide range of struct types according to the ABI */

struct one_int return_int_struct(void) {
    struct one_int retval = {1, 2};
    return retval;
}

struct twelve_bytes return_two_int_struct(void) {
    struct twelve_bytes retval = {10, "12345678"};
    return retval;
}

struct one_xmm return_double_struct(void) {
    struct one_xmm retval = {100.625};
    return retval;
}
struct two_xmm return_two_double_struct(void) {
    struct two_xmm retval = {{8.8, 7.8}};
    return retval;
}
struct xmm_and_int return_mixed(void) {
    struct xmm_and_int retval = {{10.0}, "ab"};
    return retval;
}
struct int_and_xmm return_mixed2(void) {
    struct int_and_xmm retval = {127, 34e43};
    return retval;
}
struct memory return_on_stack(void) {
    struct memory retval = {1.25, "xy", 100l, 44};
    return retval;
}

int leaf_call(struct two_ints t_i, int c, double d) {
    // validate t_i
    if (t_i.c != '_' || t_i.arr[0] != 5 || t_i.arr[1] != 6 || t_i.arr[2] != 7) {
        return 0;
    }

    // validate c1 and d1 (originally passed in a struct int_and_xmm)
    if (c != 'p' || d != 4.56) {
        return 0;
    }
    return 1;  // success
}

struct memory pass_and_return_regs(int i, double d, struct int_and_xmm strct,
                                   int c, struct two_ints t_i, long l,
                                   struct one_int_exactly o_i_e, int c2) {
    // include a stack variable to make sure it doen't overwrite return value
    // pointer or vice versa
    char stackbytes[8] = "zyxwvut";
    struct memory retval = {0, {0, 0, 0}, 0, 0};

    // make another function call to ensure that passing parameters
    // doesn't overwrite return address in RDI or other struct eightybtes
    // passed in registers; validate t_i and strct while we're at it
    if (!leaf_call(t_i, strct.c, strct.d)) {
        retval.i = 1;
        return retval;
    }
    // validate scalar params
    if (i != 6 || d != 4.0 || c != 5 || l != 77 || c2 != 99) {
        retval.i = 2;
        return retval;
    }
    // validate remainign struct
    if (o_i_e.l != 567890) {
        retval.i = 3;
        return retval;
    }

    // validate stackbytes
    if (strcmp(stackbytes, "zyxwvut")) {
        retval.i = 4;
        return retval;
    }
    retval.l = 100;
    return retval;  // success
}
)PROG")));
}

// memcmp + packed layout.
TEST_F(CodegenTest, DISABLED_Chapter18_RetvalStructSizes)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"PROG(
/* Test that we can return structs of every size between 1 and 24 bytes. */

struct bytesize1 {
    unsigned char arr[1];
};

extern struct bytesize1 globvar_1;
struct bytesize1 fun1(void);

struct bytesize2 {
    unsigned char arr[2];
};

extern struct bytesize2 globvar_2;
struct bytesize2 fun2(void);

struct bytesize3 {
    unsigned char arr[3];
};

extern struct bytesize3 globvar_3;
struct bytesize3 fun3(void);

struct bytesize4 {
    unsigned char arr[4];
};

extern struct bytesize4 globvar_4;
struct bytesize4 fun4(void);

struct bytesize5 {
    unsigned char arr[5];
};

extern struct bytesize5 globvar_5;
struct bytesize5 fun5(void);

struct bytesize6 {
    unsigned char arr[6];
};

extern struct bytesize6 globvar_6;
struct bytesize6 fun6(void);

struct bytesize7 {
    unsigned char arr[7];
};

extern struct bytesize7 globvar_7;
struct bytesize7 fun7(void);

struct bytesize8 {
    unsigned char arr[8];
};

extern struct bytesize8 globvar_8;
struct bytesize8 fun8(void);

struct bytesize9 {
    unsigned char arr[9];
};

extern struct bytesize9 globvar_9;
struct bytesize9 fun9(void);

struct bytesize10 {
    unsigned char arr[10];
};

extern struct bytesize10 globvar_10;
struct bytesize10 fun10(void);

struct bytesize11 {
    unsigned char arr[11];
};

extern struct bytesize11 globvar_11;
struct bytesize11 fun11(void);

struct bytesize12 {
    unsigned char arr[12];
};

extern struct bytesize12 globvar_12;
struct bytesize12 fun12(void);

struct bytesize13 {
    unsigned char arr[13];
};

extern struct bytesize13 globvar_13;
struct bytesize13 fun13(void);

struct bytesize14 {
    unsigned char arr[14];
};

extern struct bytesize14 globvar_14;
struct bytesize14 fun14(void);

struct bytesize15 {
    unsigned char arr[15];
};

extern struct bytesize15 globvar_15;
struct bytesize15 fun15(void);

struct bytesize16 {
    unsigned char arr[16];
};

extern struct bytesize16 globvar_16;
struct bytesize16 fun16(void);

struct bytesize17 {
    unsigned char arr[17];
};

extern struct bytesize17 globvar_17;
struct bytesize17 fun17(void);

struct bytesize18 {
    unsigned char arr[18];
};

extern struct bytesize18 globvar_18;
struct bytesize18 fun18(void);

struct bytesize19 {
    unsigned char arr[19];
};

extern struct bytesize19 globvar_19;
struct bytesize19 fun19(void);

struct bytesize20 {
    unsigned char arr[20];
};

extern struct bytesize20 globvar_20;
struct bytesize20 fun20(void);

struct bytesize21 {
    unsigned char arr[21];
};

extern struct bytesize21 globvar_21;
struct bytesize21 fun21(void);

struct bytesize22 {
    unsigned char arr[22];
};

extern struct bytesize22 globvar_22;
struct bytesize22 fun22(void);

struct bytesize23 {
    unsigned char arr[23];
};

extern struct bytesize23 globvar_23;
struct bytesize23 fun23(void);

struct bytesize24 {
    unsigned char arr[24];
};

extern struct bytesize24 globvar_24;
struct bytesize24 fun24(void);
/* Test that we can return structs of every size between 1 and 24 bytes. */
int memcmp(void *s1, void *s2, unsigned long n);

int main(void) {
    struct bytesize1 s1 = fun1();
    if (memcmp(&s1, &globvar_1, sizeof s1)) {
        return 1;
    }

    struct bytesize2 s2 = fun2();
    if (memcmp(&s2, &globvar_2, sizeof s2)) {
        return 2;
    }

    struct bytesize3 s3 = fun3();
    if (memcmp(&s3, &globvar_3, sizeof s3)) {
        return 3;
    }

    struct bytesize4 s4 = fun4();
    if (memcmp(&s4, &globvar_4, sizeof s4)) {
        return 4;
    }

    struct bytesize5 s5 = fun5();
    if (memcmp(&s5, &globvar_5, sizeof s5)) {
        return 5;
    }

    struct bytesize6 s6 = fun6();
    if (memcmp(&s6, &globvar_6, sizeof s6)) {
        return 6;
    }

    struct bytesize7 s7 = fun7();
    if (memcmp(&s7, &globvar_7, sizeof s7)) {
        return 7;
    }

    struct bytesize8 s8 = fun8();
    if (memcmp(&s8, &globvar_8, sizeof s8)) {
        return 8;
    }

    struct bytesize9 s9 = fun9();
    if (memcmp(&s9, &globvar_9, sizeof s9)) {
        return 9;
    }

    struct bytesize10 s10 = fun10();
    if (memcmp(&s10, &globvar_10, sizeof s10)) {
        return 10;
    }

    struct bytesize11 s11 = fun11();
    if (memcmp(&s11, &globvar_11, sizeof s11)) {
        return 11;
    }

    struct bytesize12 s12 = fun12();
    if (memcmp(&s12, &globvar_12, sizeof s12)) {
        return 12;
    }

    struct bytesize13 s13 = fun13();
    if (memcmp(&s13, &globvar_13, sizeof s13)) {
        return 13;
    }

    struct bytesize14 s14 = fun14();
    if (memcmp(&s14, &globvar_14, sizeof s14)) {
        return 14;
    }

    struct bytesize15 s15 = fun15();
    if (memcmp(&s15, &globvar_15, sizeof s15)) {
        return 15;
    }

    struct bytesize16 s16 = fun16();
    if (memcmp(&s16, &globvar_16, sizeof s16)) {
        return 16;
    }

    struct bytesize17 s17 = fun17();
    if (memcmp(&s17, &globvar_17, sizeof s17)) {
        return 17;
    }

    struct bytesize18 s18 = fun18();
    if (memcmp(&s18, &globvar_18, sizeof s18)) {
        return 18;
    }

    struct bytesize19 s19 = fun19();
    if (memcmp(&s19, &globvar_19, sizeof s19)) {
        return 19;
    }

    struct bytesize20 s20 = fun20();
    if (memcmp(&s20, &globvar_20, sizeof s20)) {
        return 20;
    }

    struct bytesize21 s21 = fun21();
    if (memcmp(&s21, &globvar_21, sizeof s21)) {
        return 21;
    }

    struct bytesize22 s22 = fun22();
    if (memcmp(&s22, &globvar_22, sizeof s22)) {
        return 22;
    }

    struct bytesize23 s23 = fun23();
    if (memcmp(&s23, &globvar_23, sizeof s23)) {
        return 23;
    }

    struct bytesize24 s24 = fun24();
    if (memcmp(&s24, &globvar_24, sizeof s24)) {
        return 24;
    }

    return 0;
}

struct bytesize1 globvar_1 = {{0}};

struct bytesize2 globvar_2 = {{1, 2}};

struct bytesize3 globvar_3 = {{3, 4, 5}};

struct bytesize4 globvar_4 = {{6, 7, 8, 9}};

struct bytesize5 globvar_5 = {{10, 11, 12, 13, 14}};

struct bytesize6 globvar_6 = {{15, 16, 17, 18, 19, 20}};

struct bytesize7 globvar_7 = {{21, 22, 23, 24, 25, 26, 27}};

struct bytesize8 globvar_8 = {{28, 29, 30, 31, 32, 33, 34, 35}};

struct bytesize9 globvar_9 = {{36, 37, 38, 39, 40, 41, 42, 43, 44}};

struct bytesize10 globvar_10 = {{45, 46, 47, 48, 49, 50, 51, 52, 53, 54}};

struct bytesize11 globvar_11 = {{55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65}};

struct bytesize12 globvar_12 = {
    {66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77}};

struct bytesize13 globvar_13 = {
    {78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90}};

struct bytesize14 globvar_14 = {
    {91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104}};

struct bytesize15 globvar_15 = {{105, 106, 107, 108, 109, 110, 111, 112, 113,
                                 114, 115, 116, 117, 118, 119}};

struct bytesize16 globvar_16 = {{120, 121, 122, 123, 124, 125, 126, 127, 128,
                                 129, 130, 131, 132, 133, 134, 135}};

struct bytesize17 globvar_17 = {{136, 137, 138, 139, 140, 141, 142, 143, 144,
                                 145, 146, 147, 148, 149, 150, 151, 152}};

struct bytesize18 globvar_18 = {{153, 154, 155, 156, 157, 158, 159, 160, 161,
                                 162, 163, 164, 165, 166, 167, 168, 169, 170}};

struct bytesize19 globvar_19 = {{171, 172, 173, 174, 175, 176, 177, 178, 179,
                                 180, 181, 182, 183, 184, 185, 186, 187, 188,
                                 189}};

struct bytesize20 globvar_20 = {{190, 191, 192, 193, 194, 195, 196,
                                 197, 198, 199, 200, 201, 202, 203,
                                 204, 205, 206, 207, 208, 209}};

struct bytesize21 globvar_21 = {{210, 211, 212, 213, 214, 215, 216,
                                 217, 218, 219, 220, 221, 222, 223,
                                 224, 225, 226, 227, 228, 229, 230}};

struct bytesize22 globvar_22 = {{231, 232, 233, 234, 235, 236, 237, 238,
                                 239, 240, 241, 242, 243, 244, 245, 246,
                                 247, 248, 249, 250, 251, 252}};

struct bytesize23 globvar_23 = {{253, 254, 255, 0,  1,  2,  3,  4,
                                 5,   6,   7,   8,  9,  10, 11, 12,
                                 13,  14,  15,  16, 17, 18, 19}};

struct bytesize24 globvar_24 = {{20, 21, 22, 23, 24, 25, 26, 27,
                                 28, 29, 30, 31, 32, 33, 34, 35,
                                 36, 37, 38, 39, 40, 41, 42, 43}};
/* Test that we can return structs of every size between 1 and 24 bytes. */

struct bytesize1 fun1(void) {
    return globvar_1;
}
struct bytesize2 fun2(void) {
    return globvar_2;
}
struct bytesize3 fun3(void) {
    return globvar_3;
}
struct bytesize4 fun4(void) {
    return globvar_4;
}
struct bytesize5 fun5(void) {
    return globvar_5;
}
struct bytesize6 fun6(void) {
    return globvar_6;
}
struct bytesize7 fun7(void) {
    return globvar_7;
}
struct bytesize8 fun8(void) {
    return globvar_8;
}
struct bytesize9 fun9(void) {
    return globvar_9;
}
struct bytesize10 fun10(void) {
    return globvar_10;
}
struct bytesize11 fun11(void) {
    return globvar_11;
}
struct bytesize12 fun12(void) {
    return globvar_12;
}
struct bytesize13 fun13(void) {
    return globvar_13;
}
struct bytesize14 fun14(void) {
    return globvar_14;
}
struct bytesize15 fun15(void) {
    return globvar_15;
}
struct bytesize16 fun16(void) {
    return globvar_16;
}
struct bytesize17 fun17(void) {
    return globvar_17;
}
struct bytesize18 fun18(void) {
    return globvar_18;
}
struct bytesize19 fun19(void) {
    return globvar_19;
}
struct bytesize20 fun20(void) {
    return globvar_20;
}
struct bytesize21 fun21(void) {
    return globvar_21;
}
struct bytesize22 fun22(void) {
    return globvar_22;
}
struct bytesize23 fun23(void) {
    return globvar_23;
}
struct bytesize24 fun24(void) {
    return globvar_24;
}
)PROG")));
}

// block-scope static + calloc + union punning.
TEST_F(CodegenTest, DISABLED_Chapter18_NestedUnionAccess)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"PROG(
void *calloc(unsigned long nmemb, unsigned long size);
void *malloc(unsigned long size);

union simple {
    int i;
    long l;
    char c;
    unsigned char uc_arr[3];
};

union has_union {
    double d;
    union simple u;
    union simple *u_ptr;
};

struct simple_struct {
    long l;
    double d;
    unsigned int u;
};

union has_struct {
    long l;
    struct simple_struct s;
};

struct struct_with_union {
    union simple u;
    unsigned long ul;
};

union complex_union {
    double d_arr[2];
    struct struct_with_union s;
    union has_union *u_ptr;
};
/* Test access to nested union members through dot, arrow, and subscript operators */


int test_auto_dot(void) {
    // Test nested access with . in unions/structs containing unions
    // with automatic storage duration

    // access union in union
    union has_union x;
    x.u.l = 200000u;
    if (x.u.i != 200000) {
        return 0; // fail
    }

    // access struct in union
    union has_struct y;
    y.s.l = -5555l;
    y.s.d = 10.0;
    y.s.u = 100;

    if (y.l != -5555l) {
        return 0; // fail
    }

    // access union in struct in union
    union complex_union z;
    z.s.u.i = 12345;
    z.s.ul = 0;

    if (z.s.u.c != 57) { // lowest byte of 12345
        return 0; // fail
    }

    if (z.d_arr[1]) { // bytes 8-15 of  union; same spot as z.s.ul
        return 0; // fail
    }

    // get/derefrence address of various members
    unsigned int *some_int_ptr = &y.s.u;
    union simple *some_union_ptr = &z.s.u;

    if (*some_int_ptr != 100 || (*some_union_ptr).i != 12345) {
        return 0; // fail
    }

    return 1; // success
}

int test_static_dot(void) {
    // identical to test_auto_dot but using objects
    // with static storage duration

    // access union in union
    static union has_union x;
    x.u.l = 200000u;
    if (x.u.i != 200000) {
        return 0; // fail
    }

    // access struct in union
    static union has_struct y;
    y.s.l = -5555l;
    y.s.d = 10.0;
    y.s.u = 100;

    if (y.l != -5555l) {
        return 0; // fail
    }

    // access union in struct in union
    static union complex_union z;
    z.s.u.i = 12345;
    z.s.ul = 0;

    if (z.s.u.c != 57) { // lowest byte of 12345
        return 0; // fail
    }

    if (z.d_arr[1]) { // bytes 8-15 of  union; same spot as z.s.ul
        return 0; // fail
    }

    return 1; // success
}

int test_auto_arrow(void) {
    // Test nested access in unions w/ automatic storage duration,
    // using only -> operator
    union simple inner = {100};
    union has_union outer;
    union has_union *outer_ptr = &outer;
    outer_ptr->u_ptr = &inner;
    if (outer_ptr->u_ptr->i != 100) {
        return 0; // fail
    }

    // write through nested access
    outer_ptr->u_ptr->l = -10;

    // read through other members that should have same value
    if (outer_ptr->u_ptr->c != -10 || outer_ptr->u_ptr->i != -10 || outer_ptr->u_ptr->l != -10) {
        return 0; // fail
    }

    // read through members of uc_arr
    if (outer_ptr->u_ptr->uc_arr[0] != 246 || outer_ptr->u_ptr->uc_arr[1] != 255 || outer_ptr->u_ptr->uc_arr[2] != 255) {
        return 0; // fail
    }

    return 1; // success
}

int test_static_arrow(void) {
    // identical to test_auto_arrow but with objects of static storage duration
    static union simple inner = {100};
    static union has_union outer;
    static union has_union *outer_ptr;
    outer_ptr = &outer;
    outer_ptr->u_ptr = &inner;
    if (outer_ptr->u_ptr->i != 100) {
        return 0; // fail
    }

    // write through nested access
    outer_ptr->u_ptr->l = -10;

    // read through other members that should have same value
    if (outer_ptr->u_ptr->c != -10 || outer_ptr->u_ptr->i != -10 || outer_ptr->u_ptr->l != -10) {
        return 0; // fail
    }

    // read through members of uc_arr
    if (outer_ptr->u_ptr->uc_arr[0] != 246 || outer_ptr->u_ptr->uc_arr[1] != 255 || outer_ptr->u_ptr->uc_arr[2] != 255) {
        return 0; // fail
    }

    return 1; // success
}

int test_array_of_unions(void) {
    // test access to array of unions
    union has_union arr[3];
    arr[0].u.l = -10000;
    arr[1].u.i = 200;
    arr[2].u.c = -120;

    if (arr[0].u.l != -10000 || arr[1].u.c != -56 || arr[2].u.uc_arr[0] != 136) {
        return 0; // fail
    }

    return 1; // success
}

int test_array_of_union_pointers(void) {
    // test access to array of union pointers
    union has_union *ptr_arr[3];
    for (int i = 0; i < 3; i = i + 1) {
        ptr_arr[i] = calloc(1, sizeof(union has_union));
        ptr_arr[i]->u_ptr = calloc(1, sizeof (union simple));
        ptr_arr[i]->u_ptr->l = i;
    }

    if (ptr_arr[0]->u_ptr->l != 0 || ptr_arr[1]->u_ptr->l != 1 || ptr_arr[2]->u_ptr->l != 2) {
        return 0; // fail
    }

    return 1;
}


int main(void) {
    if (!test_auto_dot()) {
        return 1;
    }

    if (!test_static_dot()) {
        return 2;
    }

    if (!test_auto_arrow()) {
        return 3;
    }

    if (!test_static_arrow()) {
        return 4;
    }

    if (!test_array_of_unions()) {
        return 5;
    }

    if (!test_array_of_union_pointers()) {
        return 6;
    }

    return 0;
}
)PROG")));
}

// block-scope static + 64-bit + union punning.
TEST_F(CodegenTest, DISABLED_Chapter18_StaticUnionAccess)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"PROG(
// Test access to static union members with . and ->
union u {
    unsigned long l;
    double d;
    char arr[8];
};

static union u my_union = { 18446744073709551615UL };
static union u* union_ptr = 0;

int main(void) {
    union_ptr = &my_union;
    if (my_union.l != 18446744073709551615UL) {
        return 1; // fail
    }

    for (int i = 0; i < 8; i = i + 1) {
        if (my_union.arr[i] != -1) {
            return 2; // fail
        }
    }

    union_ptr->d = -1.0;

    if (union_ptr->l != 13830554455654793216ul) {
        return 3; // fail
    }

    for (int i = 0; i < 6; i = i + 1) {
        // lower 6 bytes are 0
        if (my_union.arr[i]) {
            return 4; // fail
        }
    }
    if (union_ptr->arr[6] != -16) {
        return 5; // fail
    }

    if (union_ptr->arr[7] != -65) {
        return 6; // fail
    }

    return 0; // success
}
)PROG")));
}

// block-scope static + temporary lifetime + union punning.
TEST_F(CodegenTest, DISABLED_Chapter18_UnionTempLifetime)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"PROG(
// We can implicitly get the address of a union with temporary lifetime
// (and subscript it)

struct has_char_array {
    char arr[8];
};

union has_array {
    long l;
    struct has_char_array s;
};

int get_flag(void) {
    static int flag = 0;
    flag = !flag;
    return flag;
}

int main(void) {
    union has_array union1 = {9876543210l};
    union has_array union2 = {1234567890l};

    // first access member in union1
    if ((get_flag() ? union1 : union2).s.arr[0] != -22) {
        return 1; // fail
    }

    // then access member in union2
    if ((get_flag() ? union1 : union2).s.arr[0] != -46) {
        return 2; // fail
    }

    return 0; // success
}
)PROG")));
}

// 64-bit constants exceed BESM-6 41-bit integer range.
TEST_F(CodegenTest, DISABLED_Chapter18_BitwiseOpsStructMembers)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"PROG(
// Bitwise operations with structure members

struct inner {
    char b;
    unsigned int u;
};

struct outer {
    unsigned long l;
    struct inner *in_ptr;
    int bar;
    struct inner in;
};

int main(void) {
    struct inner i = {'a', 100000u};
    struct outer o = {9223372036854775810ul, &i, 100, {-80, 4294967295U}};

    if ((i.b | o.l) != 9223372036854775907ul) {
        return 1;  // fail
    }

    if ((o.bar ^ i.u) != 100036u) {
        return 2;  // fail
    }

    if ((o.in_ptr->b & o.in.b) != 32) {
        return 3;  // fail
    }

    if ((o.l >> 26) != 137438953472ul) {
        return 4;  // fail
    }

    o.bar = 12;
    if ((i.b << o.bar) != 397312) {
        return 5;
    }

    return 0;
}
)PROG")));
}

// 64-bit constants exceed BESM-6 41-bit integer range.
TEST_F(CodegenTest, DISABLED_Chapter18_CompoundAssignStructMembers)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"PROG(
// Compound assignment operations with structure members
struct inner {
    double a;
    char b;
    int *ptr;
};

struct outer {
    unsigned long l;
    struct inner *in_ptr;
    struct inner in_array[4];
    int bar;
};

int main(void) {
    int i = -1;
    int i2 = -2;
    struct inner si = {150., -12, &i};
    struct outer o = {// l
                      18446744073709551615UL,
                      // in_ptr
                      &si,
                      // in_array
                      {si, {-20e20, 120, 0}, {0, 0, 0}, {1, 1, &i2}},
                      // bar
                      2000};

    // +=
    si.a += 10;  // 150. + 10 = 160
    if (si.a != 160) {
        return 1;  // fail
    }

    // -=
    // no overflow b/c of integer promotion
    o.in_array[0].b -= 460;  //  -12 - 460 = -472, reduces to 40
    if (o.in_array[0].b != 40) {
        return 2;  // fail
    }

    // *=
    o.in_array[1].a *= -4;  // -20e20 * -4 = 80e20
    if (o.in_array[1].a != 80e20) {
        return 4;  // fail
    }

    // /=
    o.in_ptr->a /= 5;  // 160. / 5 = 32
    // o.in_ptr points to si
    if (si.a != 32) {
        return 5;  // fail
    }

    // %=
    (&o)->l %= o.bar;  // 18446744073709551615 % 2000 = 1615
    if (o.l != 1615) {
        return 6;  // fail
    }

    // pointer +=
    o.in_ptr = o.in_array;
    if ((o.in_ptr += 3)->a != 1) {
        return 7;  // fail
    }
    if (*o.in_ptr->ptr != -2) {
        return 8;  // fail
    }

    // pointer -=
    o.in_ptr -= 1u;
    if (o.in_ptr->a || o.in_ptr->b || o.in_ptr->ptr) {
        return 9;  // fail
    }

    // validate everything! (make sure nothing was clobbered)
    if (si.a != 32 || si.b != -12 || si.ptr != &i) {
        return 10;  // fail
    }

    if (o.l != 1615) {
        return 11;  // fail
    }

    if (o.in_ptr != &o.in_array[2]) {
        return 12;  // fail
    }

    if (o.in_array[0].a != 150. || o.in_array[0].b != 40 ||
        o.in_array[0].ptr != &i) {
        return 13;  // fail
    }

    if (o.in_array[1].a != 80e20 || o.in_array[1].b != 120 ||
        o.in_array[1].ptr) {
        return 14;  // fail
    }

    if (o.in_array[2].a || o.in_array[2].b || o.in_array[2].ptr) {
        return 15;  // fail
    }

    if (o.in_array[3].a != 1 || o.in_array[3].b != 1 ||
        o.in_array[3].ptr != &i2) {
        return 16;  // fail
    }

    if (o.bar != 2000) {
        return 17;
    }

    return 0;
}
)PROG")));
}

// calloc + 64-bit constants.
TEST_F(CodegenTest, DISABLED_Chapter18_IncrStructMembers)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"PROG(
// Test prefix and postfix ++ and -- with structure members

struct inner {
    char c;
    unsigned int u;
};

struct outer {
    unsigned long l;
    struct inner *in_ptr;
    int array[3];
};

void *calloc(unsigned long nmemb, unsigned long size);

int main(void) {
    struct outer my_struct = {
        // l
        9223372036854775900ul,
        // in_ptr
        calloc(3, sizeof (struct inner)),
        // array
        {-1000, -2000, -3000},
    };
    struct outer *my_struct_ptr = &my_struct;

    // prefix ++
    if (++my_struct.l != 9223372036854775901ul) {
        return 1; // fail
    }

    // prefix --
    if (--my_struct.in_ptr[0].u != 4294967295U) { // unsigned wraparound
        return 2; // fail
    }

    // postfix ++
    if (my_struct.in_ptr->c++) {
        return 3; // fail
    }

    // postfix --
    if (my_struct_ptr->array[1]-- != -2000) {
        return 4; // fail
    }

    // validate current state of my_struct - make sure we performed updates
    // and didn't clobber anything
    if (my_struct_ptr->l != 9223372036854775901ul) {
        return 5; // fail
    }

    if (my_struct.in_ptr->c != 1) {
        return 6; // fail
    }
    if (my_struct_ptr->in_ptr->u !=  4294967295U) {
        return 7; // fail
    }

    if (my_struct_ptr->array[1] != -2001) {
        return 8; // fail
    }

    if (my_struct_ptr->array[0] != -1000 || my_struct_ptr->array[2] != -3000) {
        return 9; // fail
    }

    // ++/-- w/ pointers to structs
    // first let's populate the struct array at my_struct_ptr->in_ptr
    my_struct_ptr->in_ptr[1].c = -1;
    my_struct_ptr->in_ptr[1].u = 1u;
    my_struct_ptr->in_ptr[2].c = 'X';
    my_struct_ptr->in_ptr[2].u = 100000u;

    (++my_struct_ptr->in_ptr)->c--; // decrement struct array[1].c
    my_struct_ptr->in_ptr++->u++; // decrement stuct_array[1].u, increment in_ptr

    // validate - in_ptr currently points to array member at index 2

    // element 0 (now at index -2) should have same values as last time we checked
    if (my_struct_ptr->in_ptr[-2].c != 1 || my_struct_ptr->in_ptr[-2].u != 4294967295U) {
        return 10;
    }

    // we decremented c in element 1 (now at index -1), didn't change u
    if (my_struct_ptr->in_ptr[-1].c != -2) {
        return 11; // fail
    }

    if (my_struct_ptr->in_ptr[-1].u != 2) {
        return 12; // fail
    }

    // didn't change any values in last array element (now at index 0)
    if (my_struct_ptr->in_ptr[0].c != 'X' || my_struct_ptr->in_ptr[0].u != 100000u) {
        return 13; // fail
    }

    return 0;
}
)PROG")));
}

// tag shadowing (no-shadowing design).
TEST_F(CodegenTest, DISABLED_Chapter18_StructDeclInSwitchStatement)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"PROG(
// Declare a structure inside a switch statement (basically just to make sure
// we're resolving structure tags inside switch statements)
struct s {
    int a;
    int b;
};

int main(void) {
    struct s my_struct = {1, 2};
    int result = 0;
    switch (my_struct.a) {
        // even though switch statement jumps over this declaration,
        // it's still in scope, shadowing outer one
        struct s {
            double x;
            double y;
            double z;
        };
        // declare inner variable, shadowing outer one
        struct s my_struct;
        case 1:
            my_struct.x = 20.0;
            my_struct.y = 30.0;
            result = my_struct.x + my_struct.y;
            break;
        case 2:
            my_struct.x = 11.;
            my_struct.y = 12.;
            result = my_struct.x + my_struct.y;
            break;
        default:
            my_struct.x = 0.;
            my_struct.y = 0.;
            result = my_struct.x + my_struct.y;
    }
    return result; // expected result is 50
}
)PROG")));
}

// tag shadowing (no-shadowing design).
TEST_F(CodegenTest, DISABLED_Chapter18_DeclShadowsDecl)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"PROG(
/* A struct type declaration can shadow a union type declaration
 * with the same tag, or vice versa. See
 * chapter_18/invalid_types/extra_credit/struct_and_union_ptrs.c
 * for a similar test case where we verify that the typechecker
 * can distinguish between pointers to these types
 */

int main(void) {
    struct tag; // declare (don't define) a struct type
    struct tag *struct_ptr = 0;
    {
        union tag; // declare (don't define) a union type, shadowing outer declaration
        union tag *union_ptr = 0;

        // both pointers are null
        if (struct_ptr || union_ptr) {
            return 1;// fail
        }
    }
    return 0;
}
)PROG")));
}

// calloc/puts not in libc.
TEST_F(CodegenTest, DISABLED_Chapter18_IncompleteUnionTypes)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"PROG(
/* Test that our typechecker can handle valid declarations and expressions
 * involving incomplete union types
 * */


void *calloc(unsigned long nmemb, unsigned long size);
int puts(char *s);

 // Test 1: you can declare a function that accepts/returns incomplete
 // union types
union never_used;
union never_used incomplete_fun(union never_used x);

// test 2: you can declare an incomplete union type at block scope,
// then complete it.
int test_block_scope_forward_decl(void) {
    union u;             // declare incomplete union type
    union u* u_ptr = 0;  // define a pointer to that union type

    union u {
        long x;
        char y;
    };  // complete the type

    // now you can use s_ptr as a pointer to a completed type
    union u val = { -100000000l };
    u_ptr = &val;
    if (u_ptr->x != -100000000l || u_ptr->y != 0) {
        return 0; // fail
    }

    return 1;  // success
}

// test 3: you can pass and return pointers to incomplete union types
union opaque_union;

union opaque_union* use_union_pointers(union opaque_union* param) {
    if (param == 0) {
        puts("null pointer");
    }

    return 0;
}

int test_use_incomplete_union_pointers(void) {
    // define a couple of pointers to this type
    union opaque_union* ptr1 = calloc(1, 4);
    union opaque_union* ptr2 = calloc(1, 4);

    // can cast to char * and inspect; this is well-defined
    // and all bits should be 0 since we used calloc
    char* ptr1_bytes = (char*)ptr1;
    if (ptr1_bytes[0] || ptr1_bytes[1]) {
        return 0;
    }

    // can compare to 0 or each other
    if (ptr1 == 0 || ptr2 == 0 || ptr1 == ptr2) {
        return 0;
    }

    // can use them in conditionals
    static int flse = 0;
    union opaque_union* ptr3 = flse ? ptr1 : ptr2;
    if (ptr3 != ptr2) {
        return 0;
    }

    // can pass them as parameters
    if (use_union_pointers(ptr3)) {
        return 0;
    }

    return 1;  // success
}

int main(void) {
    if (!test_block_scope_forward_decl()) {
        return 1; // fail
    }

    if (!test_use_incomplete_union_pointers()) {
        return 2; // fail
    }

    return 0; // success
}
)PROG")));
}

// tag shadowing (no-shadowing design) + malloc.
TEST_F(CodegenTest, DISABLED_Chapter18_StructShadowsUnion)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"PROG(
// One type declaration can shadow another with the same tag
void *malloc(unsigned long size);

int main(void) {
    struct s {int a; int b;};
    struct s my_struct = {12, 13};
    {
        // union type declaration shadows declaration of struct s
        union u;
        union u *ptr = malloc(4);
        union u {int i; unsigned int u;};
        ptr->i = 10;
        if (ptr->u != 10) {
            return 1; // fail
        }
        if (my_struct.b != 13) {
            return 2; // fail
        }
    }

    return 0; // success
}
)PROG")));
}

// 64-bit (LONG_MIN) + union punning + block-scope union.
TEST_F(CodegenTest, DISABLED_Chapter18_UnionNamespace)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"PROG(
/* Test that we treat union tags, function/variable names, and each
 * struct/union's member names as separate namespaces.
 */

// Different unions/structs can use same member names
int test_shared_member_names(void) {
    union u1 {
        int a;
    };
    union u2 {
        long l;
        double a;
    };
    struct s {
        char a[2];
    };

    union u1 var1 = {10};
    union u2 var2 = {-9223372036854775807l - 1}; // LONG_MIN
    struct s var3 = {{-1, -2}};
    if (var1.a != 10 || var2.a != -0.0 || var3.a[0] != -1) {
        return 0;
    }

    return 1;  // success
}

// you can use the same identiifer as a struct tag, member name, and variable
// name
int test_same_name_var_member_and_tag(void) {
    union u {
        int u;
    };
    union u u = {100};
    if (u.u != 100) {
        return 0;
    }

    return 1;  // success
}

// you can use the same identifier as a union tag and function name
int f(void) {
    return 10;
}

union f {
    int f;
};

int test_same_name_fun_and_tag(void) {
    union f x;
    x.f = f();
    if (x.f != 10) {
        return 0;  // fail
    }

    return 1;  // success
}

int main(void) {
    if (!test_shared_member_names()) {
        return 1;  // fail
    }

    if (!test_same_name_var_member_and_tag()) {
        return 2;  // fail
    }

    if (!test_same_name_fun_and_tag()) {
        return 3;  // fail
    }

    return 0;  // success
}
)PROG")));
}

// tag shadowing (no-shadowing design).
TEST_F(CodegenTest, DISABLED_Chapter18_UnionShadowsStruct)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"PROG(
// A union type declaration can shadow a struct type declaration with the same tag
struct tag {
    int a;
    int b;
};

struct tag global_struct = {1, 2};

int main(void) {
    // this shadows the declaration of 'struct tag'
    union tag {
        int x;
        long y;
    };
    union tag local_union = {100};
    if (global_struct.a != 1) {
        return 1;  // fail
    }
    if (local_union.x != 100) {
        return 2;  // fail
    }
    return 0;  // success
}
)PROG")));
}

// word/byte pointer punning comparison.
TEST_F(CodegenTest, DISABLED_Chapter18_CompareUnionPointers)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"PROG(
// Pointers to a union object and to its members all compare equal
struct s {
    int i;
};

union u {
    char arr[3];
    double d;
    struct s my_struct;
};

union u my_union; // don't initialize, so it will be initialized to zero

int main(void) {
    union u* u_ptr = &my_union;

    // compare pointer to whole union w/ pointers to members,
    // using both == and !=
    if ((void*)u_ptr != (void*)&(u_ptr->arr)) {
        return 1; // fail
    }

    if (!((void*)u_ptr == (void*)&(u_ptr->d))) {
        return 2; // fail
    }

    if ((void*)&(u_ptr->my_struct) != u_ptr) {
        return 3; // fail
    }

    // compare member pointers
    if (my_union.arr != (char*)&my_union.d) {
        return 4; // fail
    }

    if (!(&my_union.arr[0] >= (char *) &my_union.my_struct.i)) {
        return 5; // fail
    }

    if (! ((char *) (&u_ptr->d) <= (char *) &u_ptr->my_struct)) {
        return 6; // fail
    }

    return 0;
}
)PROG")));
}
