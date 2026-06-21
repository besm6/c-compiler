#include "book_run.h"

// malloc/calloc + block-scope static.
TEST_F(CodegenTest, DISABLED_Chapter18_CopyNonScalarMembers)
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
// Read and assign to non-scalar union members


void* calloc(unsigned long nmemb, unsigned long size);

int test_dot(void) {
    // Test reading/writing whole nested unions/structs w/ . operator
    // first, assign a union to a struct member
    struct struct_with_union my_struct = { {0}, 100000l };
    union simple my_simple_union;
    my_simple_union.l = -1;
    my_struct.u = my_simple_union;

    // now assign to a union mebmer of struct type
    static union complex_union my_union;
    my_union.s = my_struct;

    // validate what we have so far
    if (my_struct.ul != 100000l || my_struct.u.l != -1) {
        return 0; // fail
    }

    if (my_union.s.ul != 100000l) {
        return 0; // fail
    }

    // now copy whole structs/unions from members
    my_union.s.u.i = 45;
    // copy simple_union sub-object from my_union into my_simple_union
    my_simple_union = my_union.s.u;
    if (my_simple_union.i != 45) {
        return 0; // fail
    }

    // copy struct sub-object from my_union into another (static) variable
    static struct struct_with_union another_struct;
    another_struct = my_union.s;
    if (another_struct.ul != 100000l || another_struct.u.i != 45) {
        return 0; // fail
    }

    return 1; // success
}

int test_arrow(void) {
    // allocate some objects
    union complex_union* my_union_ptr = calloc(1, sizeof(union complex_union));
    my_union_ptr->u_ptr = calloc(1, sizeof(union has_union));
    my_union_ptr->u_ptr->u_ptr = calloc(1, sizeof(union simple));
    my_union_ptr->u_ptr->u_ptr->i = 987654321;

    // read thru arrow to assign
    union has_union another_union = *my_union_ptr->u_ptr;

    // compare pointers & pointers' dereferenced values
    if (another_union.u_ptr != my_union_ptr->u_ptr->u_ptr || another_union.u_ptr->c != my_union_ptr->u_ptr->u_ptr->c) {
        return 0; // fail
    }

    // define another object to assign through arrow
    union simple small_union = { -9999 };
    my_union_ptr->u_ptr->u = small_union;
    if (my_union_ptr->u_ptr->u.i != -9999) {
        return 0; // fail
    }

    return 1; // success
}

int main(void) {
    if (!test_dot()) {
        return 1;
    }

    if (!test_arrow()) {
        return 2;
    }

    return 0; // success
}
)PROG")));
}

// malloc + strcmp + local char-array string init + punning.
TEST_F(CodegenTest, DISABLED_Chapter18_CopyThruPointer)
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
// Test copying whole structs/unions through pointers (incl. to/from array members)



int strcmp(char* s1, char* s2);

// case 1: *x = y
int test_copy_to_pointer(void) {
    union simple y;
    y.l = -20;
    union simple* x = malloc(sizeof(union simple));
    *x = y;

    // validate
    if (x->l != -20 || x->i != -20 || x->uc_arr[0] != 236 || x->uc_arr[1] != 255 || x->uc_arr[2] != 255) {
        return 0; // fail
    }

    return 1;  // success
}

// case 2: x = *y
int test_copy_from_pointer(void) {
    // define/initialize a union object containing a struct
    struct simple_struct my_struct = { 8223372036854775807l, 20e3, 2147483650u };
    static union has_struct my_union;
    my_union.s = my_struct;

    // get a pointer to that union
    union has_struct* union_ptr;
    union_ptr = &my_union;

    // copy from pointer to another union
    union has_struct another_union = *union_ptr;

    // validate
    if (another_union.s.l != 8223372036854775807l || another_union.s.d != 20e3 || another_union.s.u != 2147483650u) {
        return 0; // fail
    }

    return 1;
}

// case 3: copies to and from array members (using a union w/ trailing padding)

// size is 12 bytes; take largest member (10 bytes)
// and pad to 4-byte alignment (b/c ui is 4-byte aligned)
union with_padding {
    char arr[10];
    unsigned int ui;
};

int test_copy_array_members(void) {

    // define/initialize an array of unions
    union with_padding union_array[3] = { {"foobar"}, {"hello"}, {"itsaunion"} };

    // copy element out of array
    union with_padding another_union = union_array[0];
    union with_padding yet_another_union = { "blahblah" };

    // copy an element into the array
    union_array[2] = yet_another_union;

    // validate
    if (strcmp(union_array[0].arr, "foobar") || strcmp(union_array[1].arr, "hello") || strcmp(union_array[2].arr, "blahblah")) {
        return 0; // fail
    }

    if (strcmp(another_union.arr, "foobar")) {
        return 0; // fail
    }

    // check yet_another_union too, even though we didn't update it
    if (strcmp(yet_another_union.arr, "blahblah")) {
        return 0; // fail
    }

    return 1; // success

}

int main(void) {
    if (!test_copy_to_pointer()){
        return 1;
    }

    if (!test_copy_from_pointer()) {
        return 2;
    }

    if (!test_copy_array_members()) {
        return 3;
    }

    return 0; // success
}
)PROG")));
}

// strcmp + union punning + 64-bit + malloc.
TEST_F(CodegenTest, DISABLED_Chapter18_ClassifyUnions)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"PROG(
// library functions
int strcmp(char* s1, char* s2);
#include <stdlib.h>
void *malloc(unsigned long size);

// I. unions passed in one register

// Ia. passed in one XMM reg

// union w/ only double-type members
union one_double {
    double d1;
    double d2;
};

// struct containing union
struct has_union_with_double {
    union one_double member;
};

// union containing struct and array
union has_struct_with_double {
    struct has_union_with_double s;
    double arr[1];
};

// Ib. passed in one general-purpose register

// passed in one general-purpose reg b/c it can hold
// either double or char
union one_int {
    double d;
    char c;
};

// may contain double (oi.d, od.d1 or od.d2) or one char
union one_int_nested {
    union one_int oi;
    union one_double od;
};

// could contain one of several types but they're all integer types
union char_int_mixed {
    char arr[7];
    union char_int_mixed* union_ptr;
    unsigned int ui;
};

// struct containing union
union char_int_short {
    char c;
    int i;
};

struct has_union {
    unsigned int i;
    union char_int_short u;
};

// union containing struct
union has_struct_with_ints {
    double d;
    struct has_union s;
    unsigned long ul;
};

// II. Unions passed in two registers

// IIa. two XMM regs

// only double-type members
union two_doubles {
    double arr[2];
    double single;
};

// union contains unions
union has_xmm_union {
    union one_double u;
    union two_doubles u2;
};

// struct contains union
struct dbl_struct {
    union one_double member1; // first eightbyte
    double member2; // second eightbyte
};

// union contains struct
union has_dbl_struct {
    struct dbl_struct member1;
};


// IIb. two general-purpose regs

// first eightbyte could hold chars or int, so it's in INTEGER class
// second must hold chars (and padding) so also in INTEGER class
union char_arr {
    char arr[11];
    int i;
};

// each eightbyte could hold either integers or double and therefore is in
// INTEGER class
union two_arrs {
    double dbl_arr[2];
    long long_arr[2];
};

// union contains struct
union two_eightbyte_has_struct {
    int arr[3]; // includes integers in both eightbytes
    struct dbl_struct member1; // all in the SSE class
};

// union contains structs w/ integer type
struct char_first_eightbyte {
    char c;
    double d;
};

struct int_second_eightbyte {
    double d;
    int i;
};

union two_structs {
    // this puts first eightbyte in INTEGER class
    struct char_first_eightbyte member1;
    // this puts second eightbyte in INTEGER class
    struct int_second_eightbyte member2;
};

// another union-with-struct example - one member is struct that just extends
// into second eightbyte
struct nine_bytes {
    int i;
    char arr[5];
};

union has_nine_byte_struct {
    char c;
    long l;
    struct nine_bytes s;
};

// struct contains union
union uneven {
    char arr[5];
    unsigned char uc;
};

struct has_uneven_union {
    int i;
    union uneven u;
};

// union contains unions
union has_other_unions {
    union uneven u;
    union two_doubles d;
    union has_nine_byte_struct n;
};

// union contains array of unions
union union_array {
    union one_int u_arr[2];
};

union uneven_union_array {
    union uneven u_arr[2];
};


// union contains array of structs
struct small {
    char arr[3];
    signed char sc;
};

union has_small_struct_array {
    struct small arr[3];
};

// IIc. general-purpose & XMM

// scalars and arrays
union gp_and_xmm {
    double d_arr[2]; // doubles in both eightbytes
    char c; // int in first eightbyte
};

// union contains struct

union scalar_and_struct {
    long* ptr; // only takes up first eightbyte
    struct char_first_eightbyte cfe; // second eightbyte is in SSE class
};

// struct contains unions
struct has_two_unions {
    union char_int_mixed member1;
    union one_double member2;
};

// union contains unions

union small_struct_arr_and_dbl {
    struct small arr[2];
    union two_doubles d;
};

// IId. XMM & general-purpose

union xmm_and_gp {
    double d;
    struct int_second_eightbyte ise;
};

// contains union
union xmm_and_gp_nested {
    union xmm_and_gp member1;
    double arr[2];
    union two_doubles d;
};

// III. passed in memory

// contains array of scalars
union lotsa_doubles {
    double arr[3];
    int i;
};

union lotsa_chars {
    char more_chars[18];
    char fewer_chars[5];
};

// contains a struct

// From uncaptioned listing in "Classifying Eightbytes" section
struct large {
    int i;
    double d;
    char arr[10];
};

union contains_large_struct {
    int i;
    unsigned long ul;
    struct large l;
};

// contains array of unions
union contains_union_array {
    union gp_and_xmm arr[2];
};

// validation functions defined in library

// validate one param (for classify_unions test cases)
int test_one_double(union one_double u);
int test_has_union_with_double(struct has_union_with_double s);
int test_has_struct_with_double(union has_struct_with_double u);
int test_one_int(union one_int u);
int test_one_int_nested(union one_int_nested u);
int test_char_int_mixed(union char_int_mixed u);
int test_has_union(struct has_union s);
int test_has_struct_with_ints(union has_struct_with_ints u);
int test_two_doubles(union two_doubles u);
int test_has_xmm_union(union has_xmm_union u);
int test_dbl_struct(struct dbl_struct s);
int test_has_dbl_struct(union has_dbl_struct u);
int test_char_arr(union char_arr u);
int test_two_arrs(union two_arrs u);
int test_two_eightbyte_has_struct(union two_eightbyte_has_struct u);
int test_two_structs(union two_structs u);
int test_has_nine_byte_struct(union has_nine_byte_struct u);
int test_has_uneven_union(struct has_uneven_union s);
int test_has_other_unions(union has_other_unions u);
int test_union_array(union union_array u);
int test_uneven_union_array(union uneven_union_array u);
int test_has_small_struct_array(union has_small_struct_array u);
int test_gp_and_xmm(union gp_and_xmm u);
int test_scalar_and_struct(union scalar_and_struct u);
int test_has_two_unions(struct has_two_unions s);
int test_small_struct_arr_and_dbl(union small_struct_arr_and_dbl u);
int test_xmm_and_gp(union xmm_and_gp u);
int test_xmm_and_gp_nested(union xmm_and_gp_nested u);
int test_lotsa_doubles(union lotsa_doubles u);
int test_lotsa_chars(union lotsa_chars u);
int test_contains_large_struct(union contains_large_struct u);
int test_contains_union_array(union contains_union_array u);

// validate multiple params (for param_passing test cases)
int pass_unions_and_structs(int i1, int i2, struct has_union one_gp_struct,
    double d1, union two_doubles two_xmm, union one_int one_gp, int i3, int i4,
    int i5);
int pass_gp_union_in_memory(union two_doubles two_xmm,
    struct has_union one_gp_struct, int i1, int i2, int i3,
    int i4, int i5, int i6, union one_int one_gp);
int pass_xmm_union_in_memory(double d1, double d2, union two_doubles two_xmm,
    union two_doubles two_xmm_copy, double d3, double d4,
    union two_doubles two_xmm_2);
int pass_borderline_union(int i1, int i2, int i3, int i4, int i5,
    union char_arr two_gp);
int pass_borderline_xmm_union(union two_doubles two_xmm, double d1, double d2,
    double d3, double d4, double d5, union two_doubles two_xmm_2);
int pass_mixed_reg_in_memory(double d1, double d2, double d3, double d4,
    int i1, int i2, int i3, int i4, int i5, int i6,
    union gp_and_xmm mixed_regs);
int pass_uneven_union_in_memory(int i1, int i2, int i3, int i4, int i5,
    union gp_and_xmm mixed_regs, union one_int one_gp, union uneven uneven);
int pass_in_mem_first(union lotsa_doubles mem, union gp_and_xmm mixed_regs,
    union char_arr two_gp, struct has_union one_gp_struct);

// validate return values (for union_retvals test case)
union one_double return_one_double(void);
union one_int_nested return_one_int_nested(void);
union has_dbl_struct return_has_dbl_struct(void);
union two_arrs return_two_arrs(void);
union scalar_and_struct return_scalar_and_struct(void);
union xmm_and_gp return_xmm_and_gp(void);
union contains_union_array return_contains_union_array(void);
union lotsa_chars pass_params_and_return_in_mem(int i1,
    union scalar_and_struct int_and_dbl, union two_arrs two_arrs, int i2,
    union contains_union_array big_union, union one_int_nested oin);
struct has_uneven_union return_struct_with_union(void);
int main(void) {

    // Ia. passed in one XMM reg

    union one_double od = { -2.345e6 };
    if (!test_one_double(od)) {
        return 1;
    }

    struct has_union_with_double huwd = { {9887.54321e44} };
    if (!test_has_union_with_double(huwd)) {
        return 2;
    }

    union has_struct_with_double hswd = { huwd };
    if (!test_has_struct_with_double(hswd)) {
        return 3;
    }

    // IIb. passed in one general-purpose register
    union one_int oi = { -80. };
    if (!test_one_int(oi)) {
        return 4;
    }

    union one_int_nested oin = { {44e55} };
    if (!test_one_int_nested(oin)) {
        return 5;
    }

    union char_int_mixed cim = { "WXYZ" };
    if (!test_char_int_mixed(cim)) {
        return 6;
    }

    struct has_union hu = { 4294954951u, {-60} };
    if (!test_has_union(hu)) {
        return 7;
    }

    union has_struct_with_ints hswi;
    hswi.s = hu;
    if (!test_has_struct_with_ints(hswi)) {
        return 8;
    }

    // IIa. two XMM regs
    union two_doubles td = { {10.0, 11.0} };
    if (!test_two_doubles(td)) {
        return 9;
    }

    union has_xmm_union hxu;
    hxu.u2 = td;
    if (!test_has_xmm_union(hxu)) {
        return 10;
    }

    struct dbl_struct ds = { od, 123.45 };
    if (!test_dbl_struct(ds)) {
        return 11;
    }

    union has_dbl_struct hds = { ds };
    if (!test_has_dbl_struct(hds)) {
        return 12;
    }

    // IIb. two general-purpose regs
    union char_arr ca = { "Chars!" };
    if (!test_char_arr(ca)) {
        return 13;
    }

    union two_arrs two_arr_var = { {13e4, 14.5} };
    if (!test_two_arrs(two_arr_var)) {
        return 14;
    }

    union two_eightbyte_has_struct tehs = { {100, 200, 300} };
    if (!test_two_eightbyte_has_struct(tehs)) {
        return 15;
    }

    union two_structs  ts = { {'x', 55.5e5} };

    if (!test_two_structs(ts)) {
        return 16;
    }

    union has_nine_byte_struct hnbs;
    hnbs.s.i = -16711936;
    for (int i = 0; i < 5; i = i + 1) {
        char byte = i % 2 ? -1 : 0;
        hnbs.s.arr[i] = byte;
    }
    hnbs.s.arr[4] = 0;
    if (!test_has_nine_byte_struct(hnbs)) {
        return 17;
    }

    struct has_uneven_union huu = { -2147483647, {"!@#$"} };
    if (!test_has_uneven_union(huu)) {
        return 18;
    }

    union has_other_unions hou;
    hou.n = hnbs;
    hou.n.s.arr[4] = 0;
    if (!test_has_other_unions(hou)) {
        return 19;
    }

    union union_array ua = { {{-20.}, {-30.}} };
    if (!test_union_array(ua)) {
        return 20;
    }

    union uneven_union_array uua = { {{"QWER"},{"TYUI"}} };
    if (!test_uneven_union_array(uua)) {
        return 21;
    }

    union has_small_struct_array hssa = { {
        {"AS", 10}, {"DF", 11}, {"GH", 12}
    } };
    if (!test_has_small_struct_array(hssa)) {
        return 22;
    }

    // IIc. general-purpose & XMM
    union gp_and_xmm gax = { {11., 12} };
    if (!test_gp_and_xmm(gax)) {
        return 23;
    }

    union scalar_and_struct sas;
    sas.cfe.c = -5;
    sas.cfe.d = -88.8;
    if (!test_scalar_and_struct(sas)) {
        return 24;
    }

    struct has_two_unions htu = {
        cim, od
    };

    if (!test_has_two_unions(htu)) {
        return 25;
    }

    union small_struct_arr_and_dbl ssaad;
    ssaad.d.arr[0] = -22.;
    ssaad.d.arr[1] = -32.;

    if (!test_small_struct_arr_and_dbl(ssaad)) {
        return 26;
    }

    // IId. XMM & general-purpose
    union xmm_and_gp xag;
    xag.ise.d = -8.;
    xag.ise.i = -8;

    if (!test_xmm_and_gp(xag)) {
        return 27;
    }

    union xmm_and_gp_nested xagn = { xag };
    if (!test_xmm_and_gp_nested(xagn)) {
        return 28;
    }

    // III. passed in memory
    union lotsa_doubles dbls = { {99., 98., 97.} };
    if (!test_lotsa_doubles(dbls)) {
        return 29;
    }

    union lotsa_chars chars = { "asflakjsdflkjs" };
    if (!test_lotsa_chars(chars)) {
        return 30;
    }

    struct large large_struct = { 100, 100., "A struct!" };
    union contains_large_struct cls;
    cls.l = large_struct;
    if (!test_contains_large_struct(cls)) {
        return 31;
    }

    union gp_and_xmm gax2 = gax;
    gax2.d_arr[0] = -2.0;
    gax2.d_arr[1] = -1.0;
    union contains_union_array cua = {
        {gax, gax2}
    };
    if (!test_contains_union_array(cua)) {
        return 32;
    }

    return 0; // success
}
int test_one_double(union one_double u) {
    return (u.d1 == -2.345e6 && u.d2 == -2.345e6);
}
int test_has_union_with_double(struct has_union_with_double s) {
    return (s.member.d1 == 9887.54321e44 && s.member.d2 == 9887.54321e44);
}

int test_has_struct_with_double(union has_struct_with_double u) {
    return (u.s.member.d1 == 9887.54321e44
        && u.arr[0] == 9887.54321e44 && u.s.member.d2 == 9887.54321e44);
}
int test_one_int(union one_int u) {
    return (u.d == -80. && u.c == 0);
}
int test_one_int_nested(union one_int_nested u) {
    return u.oi.d == 44e55 && u.oi.c == 109 && u.od.d1 == 44e55
        && u.od.d2 == 44e55;
}
int test_char_int_mixed(union char_int_mixed u) {
    return (strcmp(u.arr, "WXYZ") == 0 && u.ui == 1515804759);
}

int test_has_union(struct has_union s) {
    return (s.i == 4294954951u && s.u.c == -60);
}
int test_has_struct_with_ints(union has_struct_with_ints u) {
    return (u.s.i == 4294954951u && u.s.u.c == -60);
}

int test_two_doubles(union two_doubles u) {
    return (u.arr[0] == 10.0 && u.arr[1] == 11.0 && u.single == 10.0);
}

int test_has_xmm_union(union has_xmm_union u) {
    return u.u.d1 == 10.0 && u.u.d2 == 10.0 && u.u2.single == 10.0
        && u.u2.arr[0] == 10.0 && u.u2.arr[1] == 11.0;
}
int test_dbl_struct(struct dbl_struct s) {
    return s.member1.d1 == -2.345e6 && s.member1.d2 == -2.345e6
        && s.member2 == 123.45;
}

int test_has_dbl_struct(union has_dbl_struct u) {
    return u.member1.member1.d1 == -2.345e6 && u.member1.member1.d2 == -2.345e6
        && u.member1.member2 == 123.45;
}

int test_char_arr(union char_arr u) {
    return (strcmp(u.arr, "Chars!") == 0 && u.i == 1918986307);
}

int test_two_arrs(union two_arrs u) {
    return (u.dbl_arr[0] == 13e4 && u.dbl_arr[1] == 14.5
        && u.long_arr[0] == 4683669945186254848 && u.long_arr[1] == 4624352392379367424);
}

int test_two_eightbyte_has_struct(union two_eightbyte_has_struct u) {
    return (u.arr[0] == 100 && u.arr[1] == 200 && u.arr[2] == 300
        && u.member1.member1.d1 == 4.24399158242461027606e-312);
}
int test_two_structs(union two_structs u) {
    return (u.member1.c == 'x' && u.member1.d == 55.5e5 && u.member2.i == 0);
}
int test_has_nine_byte_struct(union has_nine_byte_struct u) {
    if (u.l != -71777214294589696l || u.c != 0) {
        return 0;
    }
    if (u.s.i != -16711936) {
        return 0;
    }
    for (int i = 0; i < 5; i = i + 1) {
        int expected = i % 2 ? -1 : 0;
        if (u.s.arr[i] != expected) {
            return 0;
        }
    }

    return 1; // success
}
int test_has_uneven_union(struct has_uneven_union s) {
    return s.i == -2147483647 && strcmp(s.u.arr, "!@#$") == 0 && s.u.uc == 33;
}

int test_has_other_unions(union has_other_unions u) {
    if (u.n.l != -71777214294589696l) {
        return 0;
    }
    for (int i = 0; i < 5; i = i + 1) {
        int expected = i % 2 ? -1 : 0;
        if (u.n.s.arr[i] != expected) {
            return 0;
        }
    }

    return 1; // success
}
int test_union_array(union union_array u) {
    return (u.u_arr->d == -20. && u.u_arr[1].d == -30.);
}

int test_uneven_union_array(union uneven_union_array u) {
    return (strcmp(u.u_arr[0].arr, "QWER") == 0 && strcmp(u.u_arr[1].arr, "TYUI") == 0);
}

int test_has_small_struct_array(union has_small_struct_array u) {
    return strcmp(u.arr[0].arr, "AS") == 0 && u.arr[0].sc == 10
        && strcmp(u.arr[1].arr, "DF") == 0 && u.arr[1].sc == 11
        && strcmp(u.arr[2].arr, "GH") == 0 && u.arr[2].sc == 12;
}
int test_gp_and_xmm(union gp_and_xmm u) {
    return u.d_arr[0] == 11. && u.d_arr[1] == 12.;
}

int test_scalar_and_struct(union scalar_and_struct u) {
    return u.cfe.c == -5 && u.cfe.d == -88.8;
}

int test_has_two_unions(struct has_two_unions s) {
    if (strcmp(s.member1.arr, "WXYZ")) {
        return 0;
    }

    if (s.member2.d1 != -2.345e6) {
        return 0;
    }

    return 1;

}

int test_small_struct_arr_and_dbl(union small_struct_arr_and_dbl u) {
    return (u.d.arr[0] == -22. && u.d.arr[1] == -32.);
}

int test_xmm_and_gp(union xmm_and_gp u) {
    return (u.ise.d == -8. && u.ise.i == -8);
}

int test_xmm_and_gp_nested(union xmm_and_gp_nested u) {
    return (u.member1.ise.d == -8. && u.member1.ise.i == -8);
}
int test_lotsa_doubles(union lotsa_doubles u) {
    return u.arr[0] == 99. && u.arr[1] == 98. && u.arr[2] == 97;
}

int test_lotsa_chars(union lotsa_chars u) {
    return !strcmp(u.more_chars, "asflakjsdflkjs");
}

int test_contains_large_struct(union contains_large_struct u) {
    return u.l.i == 100 && u.l.d == 100. && !strcmp(u.l.arr, "A struct!");
}
int test_contains_union_array(union contains_union_array u) {
    union gp_and_xmm a = u.arr[0];
    union gp_and_xmm b = u.arr[1];

    if (a.d_arr[0] != 11. || a.d_arr[1] != 12.) {
        return 0;
    }
    if (b.d_arr[1] != -1 || b.c != 0) {
        return 0;
    }
    return 1;
}
)PROG")));
}

// strcmp + union punning.
TEST_F(CodegenTest, DISABLED_Chapter18_ParamPassing)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"PROG(
// library functions
int strcmp(char* s1, char* s2);
#include <stdlib.h>
void *malloc(unsigned long size);

// I. unions passed in one register

// Ia. passed in one XMM reg

// union w/ only double-type members
union one_double {
    double d1;
    double d2;
};

// struct containing union
struct has_union_with_double {
    union one_double member;
};

// union containing struct and array
union has_struct_with_double {
    struct has_union_with_double s;
    double arr[1];
};

// Ib. passed in one general-purpose register

// passed in one general-purpose reg b/c it can hold
// either double or char
union one_int {
    double d;
    char c;
};

// may contain double (oi.d, od.d1 or od.d2) or one char
union one_int_nested {
    union one_int oi;
    union one_double od;
};

// could contain one of several types but they're all integer types
union char_int_mixed {
    char arr[7];
    union char_int_mixed* union_ptr;
    unsigned int ui;
};

// struct containing union
union char_int_short {
    char c;
    int i;
};

struct has_union {
    unsigned int i;
    union char_int_short u;
};

// union containing struct
union has_struct_with_ints {
    double d;
    struct has_union s;
    unsigned long ul;
};

// II. Unions passed in two registers

// IIa. two XMM regs

// only double-type members
union two_doubles {
    double arr[2];
    double single;
};

// union contains unions
union has_xmm_union {
    union one_double u;
    union two_doubles u2;
};

// struct contains union
struct dbl_struct {
    union one_double member1; // first eightbyte
    double member2; // second eightbyte
};

// union contains struct
union has_dbl_struct {
    struct dbl_struct member1;
};


// IIb. two general-purpose regs

// first eightbyte could hold chars or int, so it's in INTEGER class
// second must hold chars (and padding) so also in INTEGER class
union char_arr {
    char arr[11];
    int i;
};

// each eightbyte could hold either integers or double and therefore is in
// INTEGER class
union two_arrs {
    double dbl_arr[2];
    long long_arr[2];
};

// union contains struct
union two_eightbyte_has_struct {
    int arr[3]; // includes integers in both eightbytes
    struct dbl_struct member1; // all in the SSE class
};

// union contains structs w/ integer type
struct char_first_eightbyte {
    char c;
    double d;
};

struct int_second_eightbyte {
    double d;
    int i;
};

union two_structs {
    // this puts first eightbyte in INTEGER class
    struct char_first_eightbyte member1;
    // this puts second eightbyte in INTEGER class
    struct int_second_eightbyte member2;
};

// another union-with-struct example - one member is struct that just extends
// into second eightbyte
struct nine_bytes {
    int i;
    char arr[5];
};

union has_nine_byte_struct {
    char c;
    long l;
    struct nine_bytes s;
};

// struct contains union
union uneven {
    char arr[5];
    unsigned char uc;
};

struct has_uneven_union {
    int i;
    union uneven u;
};

// union contains unions
union has_other_unions {
    union uneven u;
    union two_doubles d;
    union has_nine_byte_struct n;
};

// union contains array of unions
union union_array {
    union one_int u_arr[2];
};

union uneven_union_array {
    union uneven u_arr[2];
};


// union contains array of structs
struct small {
    char arr[3];
    signed char sc;
};

union has_small_struct_array {
    struct small arr[3];
};

// IIc. general-purpose & XMM

// scalars and arrays
union gp_and_xmm {
    double d_arr[2]; // doubles in both eightbytes
    char c; // int in first eightbyte
};

// union contains struct

union scalar_and_struct {
    long* ptr; // only takes up first eightbyte
    struct char_first_eightbyte cfe; // second eightbyte is in SSE class
};

// struct contains unions
struct has_two_unions {
    union char_int_mixed member1;
    union one_double member2;
};

// union contains unions

union small_struct_arr_and_dbl {
    struct small arr[2];
    union two_doubles d;
};

// IId. XMM & general-purpose

union xmm_and_gp {
    double d;
    struct int_second_eightbyte ise;
};

// contains union
union xmm_and_gp_nested {
    union xmm_and_gp member1;
    double arr[2];
    union two_doubles d;
};

// III. passed in memory

// contains array of scalars
union lotsa_doubles {
    double arr[3];
    int i;
};

union lotsa_chars {
    char more_chars[18];
    char fewer_chars[5];
};

// contains a struct

// From uncaptioned listing in "Classifying Eightbytes" section
struct large {
    int i;
    double d;
    char arr[10];
};

union contains_large_struct {
    int i;
    unsigned long ul;
    struct large l;
};

// contains array of unions
union contains_union_array {
    union gp_and_xmm arr[2];
};

// validation functions defined in library

// validate one param (for classify_unions test cases)
int test_one_double(union one_double u);
int test_has_union_with_double(struct has_union_with_double s);
int test_has_struct_with_double(union has_struct_with_double u);
int test_one_int(union one_int u);
int test_one_int_nested(union one_int_nested u);
int test_char_int_mixed(union char_int_mixed u);
int test_has_union(struct has_union s);
int test_has_struct_with_ints(union has_struct_with_ints u);
int test_two_doubles(union two_doubles u);
int test_has_xmm_union(union has_xmm_union u);
int test_dbl_struct(struct dbl_struct s);
int test_has_dbl_struct(union has_dbl_struct u);
int test_char_arr(union char_arr u);
int test_two_arrs(union two_arrs u);
int test_two_eightbyte_has_struct(union two_eightbyte_has_struct u);
int test_two_structs(union two_structs u);
int test_has_nine_byte_struct(union has_nine_byte_struct u);
int test_has_uneven_union(struct has_uneven_union s);
int test_has_other_unions(union has_other_unions u);
int test_union_array(union union_array u);
int test_uneven_union_array(union uneven_union_array u);
int test_has_small_struct_array(union has_small_struct_array u);
int test_gp_and_xmm(union gp_and_xmm u);
int test_scalar_and_struct(union scalar_and_struct u);
int test_has_two_unions(struct has_two_unions s);
int test_small_struct_arr_and_dbl(union small_struct_arr_and_dbl u);
int test_xmm_and_gp(union xmm_and_gp u);
int test_xmm_and_gp_nested(union xmm_and_gp_nested u);
int test_lotsa_doubles(union lotsa_doubles u);
int test_lotsa_chars(union lotsa_chars u);
int test_contains_large_struct(union contains_large_struct u);
int test_contains_union_array(union contains_union_array u);

// validate multiple params (for param_passing test cases)
int pass_unions_and_structs(int i1, int i2, struct has_union one_gp_struct,
    double d1, union two_doubles two_xmm, union one_int one_gp, int i3, int i4,
    int i5);
int pass_gp_union_in_memory(union two_doubles two_xmm,
    struct has_union one_gp_struct, int i1, int i2, int i3,
    int i4, int i5, int i6, union one_int one_gp);
int pass_xmm_union_in_memory(double d1, double d2, union two_doubles two_xmm,
    union two_doubles two_xmm_copy, double d3, double d4,
    union two_doubles two_xmm_2);
int pass_borderline_union(int i1, int i2, int i3, int i4, int i5,
    union char_arr two_gp);
int pass_borderline_xmm_union(union two_doubles two_xmm, double d1, double d2,
    double d3, double d4, double d5, union two_doubles two_xmm_2);
int pass_mixed_reg_in_memory(double d1, double d2, double d3, double d4,
    int i1, int i2, int i3, int i4, int i5, int i6,
    union gp_and_xmm mixed_regs);
int pass_uneven_union_in_memory(int i1, int i2, int i3, int i4, int i5,
    union gp_and_xmm mixed_regs, union one_int one_gp, union uneven uneven);
int pass_in_mem_first(union lotsa_doubles mem, union gp_and_xmm mixed_regs,
    union char_arr two_gp, struct has_union one_gp_struct);

// validate return values (for union_retvals test case)
union one_double return_one_double(void);
union one_int_nested return_one_int_nested(void);
union has_dbl_struct return_has_dbl_struct(void);
union two_arrs return_two_arrs(void);
union scalar_and_struct return_scalar_and_struct(void);
union xmm_and_gp return_xmm_and_gp(void);
union contains_union_array return_contains_union_array(void);
union lotsa_chars pass_params_and_return_in_mem(int i1,
    union scalar_and_struct int_and_dbl, union two_arrs two_arrs, int i2,
    union contains_union_array big_union, union one_int_nested oin);
struct has_uneven_union return_struct_with_union(void);
/* Test passing union types along w/ other arguments according to ABI */


int main(void) {
    // mix of unions, structs, and other args; we can pass the unions in registers
    union two_doubles two_xmm = { {-10.0, -11.0} }; // in two XMM regs
    union one_int one_gp = { 13.0 }; // in one general-purpose reg
    struct has_union one_gp_struct = { -24, {0} };
    one_gp_struct.u.i = 123456789;

    if (!pass_unions_and_structs(1, 2, one_gp_struct, 4.0, two_xmm, one_gp, 100, 120, 130)) {
        return 1;
    }

    // out of general-purpose regs, pass the union in memory
    if (!pass_gp_union_in_memory(two_xmm, one_gp_struct, -1, -2, -3, -4, -5, -6, one_gp)) {
        return 2;
    }

    // out of XMM regs, pass the union in memory
    union two_doubles two_xmm_2 = { {33e4, 55e6 } };

    if (!pass_xmm_union_in_memory(1.0, 2.0, two_xmm, two_xmm, 3.0, 4.0, two_xmm_2)) {
        return 3;
    }

    // we have one register available for union but two are needed so we pass
    // the whole thing on the stack
    union char_arr two_gp = { "+_)(*&^%$#" };
    if (!pass_borderline_union(1, 2, 3, 4, 5, two_gp)) {
        return 4;
    }

    // same idea but w/ union passed in XMM registers

    // update values (reduce risk that test passes accidentally b/c correct
    // values are hanging around in regs/memory from earlier calls)
    two_xmm_2.arr[0] = two_xmm_2.arr[0] * 2;
    two_xmm_2.arr[1] = two_xmm_2.arr[1] * 2;
    if (!pass_borderline_xmm_union(two_xmm, 9.0, 8.0, 7.0, 6.0, 5.0, two_xmm_2)) {
        return 5;
    }

    // same idea but w/ union passed in a mix of registers - we have a free XMM reg
    // but not a free general-purpose reg
    union gp_and_xmm mixed_regs = { {0, 150.5} };
    if (!pass_mixed_reg_in_memory(101.2, 102.3, 103.4, 104.5, 75, 76, 77, 78, 79, 80, mixed_regs)) {
        return 6;
    }

    // pass a union in memory that isn't neatly divisible by eight
    union uneven uneven = { "boop" };
    if (!pass_uneven_union_in_memory(1100, 2200, 3300, 4400, 5500, mixed_regs, one_gp, uneven)) {
        return 7;
    }

    // first union in large and must be passed in memory;
    // later unions/structs can go in regs
    union lotsa_doubles mem = { {66., 77., 88.} };
    if (!pass_in_mem_first(mem, mixed_regs, two_gp, one_gp_struct)) {
        return 8;
    }

    return 0;
}
/* Test passing union types along w/ other arguments according to ABI;
 * these functions just validate params passed by client
 */

int pass_unions_and_structs(int i1, int i2, struct has_union one_gp_struct,
    double d1, union two_doubles two_xmm, union one_int one_gp, int i3, int i4,
    int i5) {
    // start w/ scalars
    if (!(i1 == 1 && i2 == 2 && d1 == 4.0 && i3 == 100 && i4 == 120 && i5 == 130)) {
        return 0; // fail
    }

    // then validate structs/unions
    if (!(one_gp_struct.i == (unsigned int)-24 && one_gp_struct.u.i == 123456789)) {
        return 0; // fail
    }

    if (!(two_xmm.arr[0] == -10. && two_xmm.arr[1] == -11.)) {
        return 0; // fail
    }

    if (!(one_gp.d == 13.)) {
        return 0; // fail
    }

    return 1; // success
}

int pass_gp_union_in_memory(union two_doubles two_xmm,
    struct has_union one_gp_struct, int i1, int i2, int i3,
    int i4, int i5, int i6, union one_int one_gp) {

    // first validate scalars
    if (!(i1 == -1 && i2 == -2 && i3 == -3 && i4 == -4 && i5 == -5 && i6 == -6)) {
        return 0; // fail
    }

    // now validate structs/unions
    if (!(two_xmm.arr[0] == -10. && two_xmm.arr[1] == -11.)) {
        return 0; // fail
    }

    if (!(one_gp_struct.i == (unsigned int)-24 && one_gp_struct.u.i == 123456789)) {
        return 0; // fail
    }

    if (!(one_gp.d == 13.)) {
        return 0; // fail
    }

    return 1; // success

}

int pass_xmm_union_in_memory(double d1, double d2, union two_doubles two_xmm,
    union two_doubles two_xmm_copy, double d3, double d4,
    union two_doubles two_xmm_2) {

    // start w/ scalars
    if (!(d1 == 1.0 && d2 == 2.0 && d3 == 3.0 && d4 == 4.0)) {
        return 0;
    }

    // next validate unions
    if (!(two_xmm.arr[0] == -10. && two_xmm.arr[1] == -11.)) {
        return 0; // fail
    }

    if (!(two_xmm_copy.arr[0] == -10. && two_xmm_copy.arr[1] == -11.)) {
        return 0; // fail
    }
    if (!(two_xmm_2.arr[0] == 33e4 && two_xmm_2.arr[1] == 55e6)) {
        return 0; // fail
    }

    return 1; // success
}

int pass_borderline_union(int i1, int i2, int i3, int i4, int i5,
    union char_arr two_gp) {

    if (!(i1 == 1 && i2 == 2 && i3 == 3 && i4 == 4 && i5 == 5)) {
        return 0; // fail
    }

    if (strcmp(two_gp.arr, "+_)(*&^%$#") != 0) {
        return 0; // fail
    }

    return 1; // success
}

int pass_borderline_xmm_union(union two_doubles two_xmm, double d1, double d2,
    double d3, double d4, double d5, union two_doubles two_xmm_2) {

    // scalars first
    if (!(d1 == 9.0 && d2 == 8.0 && d3 == 7.0 && d4 == 6.0 && d5 == 5.0)) {
        return 0; // fail
    }

    // then unions
    if (!(two_xmm.arr[0] == -10. && two_xmm.arr[1] == -11.)) {
        return 0; // fail
    }

    if (!(two_xmm_2.arr[0] == 66e4 && two_xmm_2.arr[1] == 110e6)) {
        return 0;
    }
    return 1; // success
}

int pass_mixed_reg_in_memory(double d1, double d2, double d3, double d4,
    int i1, int i2, int i3, int i4, int i5, int i6,
    union gp_and_xmm mixed_regs) {

    // start w/ scalars
    if (!(d1 == 101.2 && d2 == 102.3 && d3 == 103.4 && d4 == 104.5 && i1 == 75 && i2 == 76 && i3 == 77 && i4 == 78 && i5 == 79 && i6 == 80)) {
        return 0; // fail
    }

    // then union
    if (!(mixed_regs.d_arr[0] == 0 && mixed_regs.d_arr[1] == 150.5)) {
        return 0; // fail
    }

    return 1; // success
}
int pass_uneven_union_in_memory(int i1, int i2, int i3, int i4, int i5,
    union gp_and_xmm mixed_regs, union one_int one_gp, union uneven uneven) {

    // scalars first
    if (!(i1 == 1100 && i2 == 2200 && i3 == 3300 && i4 == 4400 && i5 == 5500)) {
        return 0; // fail
    }

    // then unions
    if (!(mixed_regs.d_arr[0] == 0 && mixed_regs.d_arr[1] == 150.5)) {
        return 0; // fail
    }

    if (!(one_gp.d == 13.)) {
        return 0; // fail
    }

    if (strcmp(uneven.arr, "boop") != 0) {
        return 0; // fail
    }

    return 1; // success

}
int pass_in_mem_first(union lotsa_doubles mem, union gp_and_xmm mixed_regs,
    union char_arr two_gp, struct has_union one_gp_struct) {

    if (!(mem.arr[0] == 66. && mem.arr[1] == 77. && mem.arr[2] == 88.)) {
        return 0; // fail
    }

    if (!(mixed_regs.d_arr[0] == 0 && mixed_regs.d_arr[1] == 150.5)) {
        return 0; // fail
    }

    if (strcmp(two_gp.arr, "+_)(*&^%$#") != 0) {
        return 0; // fail
    }

    if (!(one_gp_struct.i == (unsigned int)-24 && one_gp_struct.u.i == 123456789)) {
        return 0; // fail
    }

    return 1; // success
}
)PROG")));
}

// strcmp + 64-bit + static union punning.
TEST_F(CodegenTest, DISABLED_Chapter18_StaticUnionInits)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"PROG(
int strcmp(char* s1, char* s2);

// Test case 1 - simple union w/ scalar elements (and padding)
union simple {
    int i;
    char c;
    double d;
};

extern union simple s;
int validate_simple(void);

// Test case 2 - union w/ another union as first element
union has_union {
    union simple u;
    char c;
};

extern union has_union h;
int validate_has_union(void);

// Test case 3 - struct containing partially initialized array of unions
// (make sure we initialize padding to 0 for each of them)
struct has_union_array {
    union has_union union_array[4];
    char c;
    union simple s;
};


extern struct has_union_array my_struct;
int validate_has_union_array(void);


// Test case 4 - an uninitialized static union (make sure we initialize the
// whole thing, including padding, to zeroes)

extern union has_union all_zeros;
int validate_uninitialized(void);

// Test case 5 - an array of unions with trailing padding. Make sure padding
// is included
union with_padding {
    char arr[13];
    long l;
}; // extra 3 bytes of padding to make it 8-byte aligned

extern union with_padding padded_union_array[3];
int validate_padded_union_array(void);
// Test initialization of static unions; make sure uninitialized
// unions/sub-objects are initialized to zero

// Test case 1 - simple union w/ scalar elements

union simple s = {217};

// Test case 2 - union w/ another union as first element

union has_union h = {{77}};

// Test case 3 - struct containing partially initialized array of unions
// (make sure we initialize uninitialized values to zero)

struct has_union_array my_struct = {
    {{{'a'}}, {{'b'}}, {{'c'}}}, '#', {'!'}
};

// Test case 4 - uninitialized union (make sure whole thing is initialized to
// 0, not just first element)

union has_union all_zeros;

// Test case 5 - an array of unions with trailing padding. Make sure padding
// is included
union with_padding padded_union_array[3] = {
    {"first string"}, {"string #2"}, {
        "string #3"
    }
};

int main(void) {
    if (!validate_simple()) {
        return 1;
    }

    if (!validate_has_union()){
        return 2;
    }

    if (!validate_has_union_array()) {
        return 3;
    }

    if (!validate_uninitialized()) {
        return 4;
    }

    if (!validate_padded_union_array()) {
        return 5;
    }

    return 0;
}
// Test initialization of static unions; make sure uninitialized unions are initialized to zero


int validate_simple(void) {
    return (s.c == -39 && s.i == 217);
}

int validate_has_union(void) {
    return (h.u.c == 77 && h.c == 77 && h.u.i == 77);
}

int validate_has_union_array(void) {

    // validate array of unions
    // first validate elements 0-2
    for (int i = 0; i < 3; i = i + 1) {
        int expected = 'a' + i;
        if (my_struct.union_array[i].u.c != expected
            || my_struct.union_array[i].c != expected
            || my_struct.union_array[i].u.i != expected) {
            return 0;
        }
    }

    // last array element should be all 0s (including bytes that
    // aren't part of first member) b/c it's uninitialized
    if (my_struct.union_array[3].u.d != 0.0) {
        return 0;
    }

    // validate other elements of struct
    if (my_struct.c != '#') {
        return 0; // fail
    }

    if (my_struct.s.c != '!' || my_struct.s.i != '!') {
        return 0; // fail
    }

    return 1;
}

int validate_uninitialized(void) {
    if (all_zeros.u.d != 0.0) {
        return 0; // fail
    }
    return 1;
}

int validate_padded_union_array(void) {
    if (strcmp(padded_union_array[0].arr, "first string") != 0) {
        return 0; // fail
    }

    if (strcmp(padded_union_array[1].arr, "string #2") != 0) {
        return 0; // fail
    }

    if (strcmp(padded_union_array[2].arr, "string #3") != 0) {
        return 0; // fail
    }

    return 1;
}
)PROG")));
}

// strcmp + 64-bit + union punning.
TEST_F(CodegenTest, DISABLED_Chapter18_UnionInits)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"PROG(
// library functions
int strcmp(char *s1, char *s2);

union simple {
    double d;
    char c;
    int *ptr;
};

union inner {
    char arr[9];
};

struct my_struct {
    long l;
    union inner u;
    int i;
};

union nested {
    struct my_struct str;
    union simple s;
    long l;
};

int validate_simple(union simple *ptr);
int validate_simple_converted(union simple *ptr);
int validate_nested(union nested *ptr);
int validate_nested_partial(union nested *ptr);
// Test initialization of unions with automatic storage duration



int test_simple(void) {
    // initialize simple union w/ only scalar members
    union simple x = { 123.45 };
    return validate_simple(&x);
}

int test_simple_converted(void) {
    // initialize simple union where value of element is implicitly converted
    // to target type (in this case the nearest representatble double,
    // 18446744073709549568.0)
    union simple x = { 18446744073709550315UL };
    return validate_simple_converted(&x);
}


int test_nested(void) {
    // initalize nested union where first member is a structure
    union nested x = { {4294967395l, {{-1, -2, -3, -4, -5, -6, -7, -8, -9}}} };
    return validate_nested(&x);
}

int test_nested_partial_init(void) {
    // initialize union where inner subobject is a partly initialized struct
    union nested x = { {9000372036854775800l, {"string"}} };
    return validate_nested_partial(&x);
}

int main(void) {
    if (!test_simple()) {
        return 1;
    }

    if (!test_simple_converted()) {
        return 2;
    }

    if (!test_nested()) {
        return 3;
    }

    if (!test_nested_partial_init()) {
        return 4;
    }

    return 0;
}
// Test initialization of unions with both automatic and static storage duration


int validate_simple(union simple* ptr) {
    return (ptr->d == 123.45);
}

int validate_simple_converted(union simple* ptr) {
    return (ptr->d == 18446744073709549568.);
}

int validate_nested(union nested* ptr) {
    if (ptr->str.l != 4294967395l) {
        return 0; // fail
    }

    for (int i = 0; i < 9; i = i + 1) {
        if (ptr->str.u.arr[i] != -1 - i) {
            return 0;  // fail
        }
    }

    return 1; // success
}
int validate_nested_partial(union nested* ptr) {
    if (ptr->str.l != 9000372036854775800l) {
        return 0; // fail
    }

    if (strcmp(ptr->str.u.arr, "string")) {
        return 0; // fail
    }

    return 1; // success
}
)PROG")));
}

// strcmp + union punning + 64-bit.
TEST_F(CodegenTest, DISABLED_Chapter18_UnionRetvals)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"PROG(
// library functions
int strcmp(char* s1, char* s2);
#include <stdlib.h>
void *malloc(unsigned long size);

// I. unions passed in one register

// Ia. passed in one XMM reg

// union w/ only double-type members
union one_double {
    double d1;
    double d2;
};

// struct containing union
struct has_union_with_double {
    union one_double member;
};

// union containing struct and array
union has_struct_with_double {
    struct has_union_with_double s;
    double arr[1];
};

// Ib. passed in one general-purpose register

// passed in one general-purpose reg b/c it can hold
// either double or char
union one_int {
    double d;
    char c;
};

// may contain double (oi.d, od.d1 or od.d2) or one char
union one_int_nested {
    union one_int oi;
    union one_double od;
};

// could contain one of several types but they're all integer types
union char_int_mixed {
    char arr[7];
    union char_int_mixed* union_ptr;
    unsigned int ui;
};

// struct containing union
union char_int_short {
    char c;
    int i;
};

struct has_union {
    unsigned int i;
    union char_int_short u;
};

// union containing struct
union has_struct_with_ints {
    double d;
    struct has_union s;
    unsigned long ul;
};

// II. Unions passed in two registers

// IIa. two XMM regs

// only double-type members
union two_doubles {
    double arr[2];
    double single;
};

// union contains unions
union has_xmm_union {
    union one_double u;
    union two_doubles u2;
};

// struct contains union
struct dbl_struct {
    union one_double member1; // first eightbyte
    double member2; // second eightbyte
};

// union contains struct
union has_dbl_struct {
    struct dbl_struct member1;
};


// IIb. two general-purpose regs

// first eightbyte could hold chars or int, so it's in INTEGER class
// second must hold chars (and padding) so also in INTEGER class
union char_arr {
    char arr[11];
    int i;
};

// each eightbyte could hold either integers or double and therefore is in
// INTEGER class
union two_arrs {
    double dbl_arr[2];
    long long_arr[2];
};

// union contains struct
union two_eightbyte_has_struct {
    int arr[3]; // includes integers in both eightbytes
    struct dbl_struct member1; // all in the SSE class
};

// union contains structs w/ integer type
struct char_first_eightbyte {
    char c;
    double d;
};

struct int_second_eightbyte {
    double d;
    int i;
};

union two_structs {
    // this puts first eightbyte in INTEGER class
    struct char_first_eightbyte member1;
    // this puts second eightbyte in INTEGER class
    struct int_second_eightbyte member2;
};

// another union-with-struct example - one member is struct that just extends
// into second eightbyte
struct nine_bytes {
    int i;
    char arr[5];
};

union has_nine_byte_struct {
    char c;
    long l;
    struct nine_bytes s;
};

// struct contains union
union uneven {
    char arr[5];
    unsigned char uc;
};

struct has_uneven_union {
    int i;
    union uneven u;
};

// union contains unions
union has_other_unions {
    union uneven u;
    union two_doubles d;
    union has_nine_byte_struct n;
};

// union contains array of unions
union union_array {
    union one_int u_arr[2];
};

union uneven_union_array {
    union uneven u_arr[2];
};


// union contains array of structs
struct small {
    char arr[3];
    signed char sc;
};

union has_small_struct_array {
    struct small arr[3];
};

// IIc. general-purpose & XMM

// scalars and arrays
union gp_and_xmm {
    double d_arr[2]; // doubles in both eightbytes
    char c; // int in first eightbyte
};

// union contains struct

union scalar_and_struct {
    long* ptr; // only takes up first eightbyte
    struct char_first_eightbyte cfe; // second eightbyte is in SSE class
};

// struct contains unions
struct has_two_unions {
    union char_int_mixed member1;
    union one_double member2;
};

// union contains unions

union small_struct_arr_and_dbl {
    struct small arr[2];
    union two_doubles d;
};

// IId. XMM & general-purpose

union xmm_and_gp {
    double d;
    struct int_second_eightbyte ise;
};

// contains union
union xmm_and_gp_nested {
    union xmm_and_gp member1;
    double arr[2];
    union two_doubles d;
};

// III. passed in memory

// contains array of scalars
union lotsa_doubles {
    double arr[3];
    int i;
};

union lotsa_chars {
    char more_chars[18];
    char fewer_chars[5];
};

// contains a struct

// From uncaptioned listing in "Classifying Eightbytes" section
struct large {
    int i;
    double d;
    char arr[10];
};

union contains_large_struct {
    int i;
    unsigned long ul;
    struct large l;
};

// contains array of unions
union contains_union_array {
    union gp_and_xmm arr[2];
};

// validation functions defined in library

// validate one param (for classify_unions test cases)
int test_one_double(union one_double u);
int test_has_union_with_double(struct has_union_with_double s);
int test_has_struct_with_double(union has_struct_with_double u);
int test_one_int(union one_int u);
int test_one_int_nested(union one_int_nested u);
int test_char_int_mixed(union char_int_mixed u);
int test_has_union(struct has_union s);
int test_has_struct_with_ints(union has_struct_with_ints u);
int test_two_doubles(union two_doubles u);
int test_has_xmm_union(union has_xmm_union u);
int test_dbl_struct(struct dbl_struct s);
int test_has_dbl_struct(union has_dbl_struct u);
int test_char_arr(union char_arr u);
int test_two_arrs(union two_arrs u);
int test_two_eightbyte_has_struct(union two_eightbyte_has_struct u);
int test_two_structs(union two_structs u);
int test_has_nine_byte_struct(union has_nine_byte_struct u);
int test_has_uneven_union(struct has_uneven_union s);
int test_has_other_unions(union has_other_unions u);
int test_union_array(union union_array u);
int test_uneven_union_array(union uneven_union_array u);
int test_has_small_struct_array(union has_small_struct_array u);
int test_gp_and_xmm(union gp_and_xmm u);
int test_scalar_and_struct(union scalar_and_struct u);
int test_has_two_unions(struct has_two_unions s);
int test_small_struct_arr_and_dbl(union small_struct_arr_and_dbl u);
int test_xmm_and_gp(union xmm_and_gp u);
int test_xmm_and_gp_nested(union xmm_and_gp_nested u);
int test_lotsa_doubles(union lotsa_doubles u);
int test_lotsa_chars(union lotsa_chars u);
int test_contains_large_struct(union contains_large_struct u);
int test_contains_union_array(union contains_union_array u);

// validate multiple params (for param_passing test cases)
int pass_unions_and_structs(int i1, int i2, struct has_union one_gp_struct,
    double d1, union two_doubles two_xmm, union one_int one_gp, int i3, int i4,
    int i5);
int pass_gp_union_in_memory(union two_doubles two_xmm,
    struct has_union one_gp_struct, int i1, int i2, int i3,
    int i4, int i5, int i6, union one_int one_gp);
int pass_xmm_union_in_memory(double d1, double d2, union two_doubles two_xmm,
    union two_doubles two_xmm_copy, double d3, double d4,
    union two_doubles two_xmm_2);
int pass_borderline_union(int i1, int i2, int i3, int i4, int i5,
    union char_arr two_gp);
int pass_borderline_xmm_union(union two_doubles two_xmm, double d1, double d2,
    double d3, double d4, double d5, union two_doubles two_xmm_2);
int pass_mixed_reg_in_memory(double d1, double d2, double d3, double d4,
    int i1, int i2, int i3, int i4, int i5, int i6,
    union gp_and_xmm mixed_regs);
int pass_uneven_union_in_memory(int i1, int i2, int i3, int i4, int i5,
    union gp_and_xmm mixed_regs, union one_int one_gp, union uneven uneven);
int pass_in_mem_first(union lotsa_doubles mem, union gp_and_xmm mixed_regs,
    union char_arr two_gp, struct has_union one_gp_struct);

// validate return values (for union_retvals test case)
union one_double return_one_double(void);
union one_int_nested return_one_int_nested(void);
union has_dbl_struct return_has_dbl_struct(void);
union two_arrs return_two_arrs(void);
union scalar_and_struct return_scalar_and_struct(void);
union xmm_and_gp return_xmm_and_gp(void);
union contains_union_array return_contains_union_array(void);
union lotsa_chars pass_params_and_return_in_mem(int i1,
    union scalar_and_struct int_and_dbl, union two_arrs two_arrs, int i2,
    union contains_union_array big_union, union one_int_nested oin);
struct has_uneven_union return_struct_with_union(void);
/* Test returning unions (and structs containing unions) according to the ABI */


int main(void) {

    // return a value in one XMM register
    union one_double od = return_one_double();
    if (!(od.d1 == 245.5 && od.d2 == 245.5)) {
        return 1; // fail
    }

    // return a value in one general-purpose register
    union one_int_nested oin = return_one_int_nested();
    if (oin.oi.d != -9876.5) {
        return 2; // fail
    }

    // return a value in two XMM registers
    union has_dbl_struct two_xmm = return_has_dbl_struct();
    if (!(two_xmm.member1.member1.d1 == 1234.5 && two_xmm.member1.member2 == 6789.)) {
        return 3; // fail
    }

    // return a value in two general-purpose registers
    union two_arrs two_arrs = return_two_arrs();
    if (two_arrs.dbl_arr[0] != 66.75 || two_arrs.long_arr[1] != -4294967300l) {
        return 4;
    }

    // return a value in one general-purpose and one XMM register
    union scalar_and_struct int_and_dbl = return_scalar_and_struct();
    if (int_and_dbl.cfe.c != -115 || int_and_dbl.cfe.d != 222222.25) {
        return 5;
    }

    // return a value in one XMM and one general-purpose register
    union xmm_and_gp dbl_and_int = return_xmm_and_gp();
    if (dbl_and_int.d != -50000.125 || dbl_and_int.ise.d != -50000.125
        || dbl_and_int.ise.i != -3000) {
        return 6;
    }

    // return a value in memory
    union contains_union_array big_union = return_contains_union_array();
    if (!(big_union.arr[0].d_arr[0] == -2000e-4 && big_union.arr[0].d_arr[1] == -3000e-4
        && big_union.arr[1].d_arr[0] == 20000e10 && big_union.arr[1].d_arr[1] == 5000e11)) {
        return 7;
    }

    // pass some unions and return a value in memory;
    // make sure returning in memory doesn't screw up param passing
    union lotsa_chars chars_union = pass_params_and_return_in_mem(1,
        int_and_dbl, two_arrs, 25, big_union, oin);

    if (strcmp(chars_union.more_chars, "ABCDEFGHIJKLMNOPQ") != 0) {
        return 8;
    }

    // return a struct that contains a union (in two registers)
    struct has_uneven_union s = return_struct_with_union();
    if (s.i != -8765 || strcmp(s.u.arr, "done") != 0) {
        return 9;
    }

    return 0; // success!
}
/* Test returning unions (and structs containing unions) according to the ABI */


union one_double return_one_double(void) {
    union one_double result = { 245.5 };
    return result;
}

union one_int_nested return_one_int_nested(void) {
    union one_int_nested result = { {-9876.5} };
    return result;
}

union has_dbl_struct return_has_dbl_struct(void) {
    union has_dbl_struct result = {
        {
            {1234.5}, 6789.
        }
    };
    return result;
}

union two_arrs return_two_arrs(void) {
    union two_arrs result;
    result.dbl_arr[0] = 66.75;
    result.long_arr[1] = -4294967300l;
    return result;
}

union scalar_and_struct return_scalar_and_struct(void) {
    union scalar_and_struct result;
    result.cfe.c = -115;
    result.cfe.d =  222222.25;
    return result;
}

union xmm_and_gp return_xmm_and_gp(void) {
    union xmm_and_gp result;
    result.ise.d = -50000.125;
    result.ise.i = -3000;
    return result;
}

union contains_union_array return_contains_union_array(void) {
    union contains_union_array result = {
        {
            {{-2000e-4, -3000e-4}}, {{20000e10, 5000e11}}
        }
    };
    return result;
}

union lotsa_chars pass_params_and_return_in_mem(int i1,
    union scalar_and_struct int_and_dbl, union two_arrs two_arrs, int i2,
    union contains_union_array big_union, union one_int_nested oin) {

    // first, validate params, starting w/ scalars
    if (i1 != 1 || i2 != 25) {
        exit(-1);
    }

    // now validate non-scalar params
    if (int_and_dbl.cfe.c != -115 || int_and_dbl.cfe.d != 222222.25) {
        exit(-2);
    }

    if (two_arrs.dbl_arr[0] != 66.75 || two_arrs.long_arr[1] != -4294967300l) {
        exit(-3);
    }

    if (!(big_union.arr[0].d_arr[0] == -2000e-4 && big_union.arr[0].d_arr[1] == -3000e-4
        && big_union.arr[1].d_arr[0] == 20000e10 && big_union.arr[1].d_arr[1] == 5000e11)) {
        exit(-4);
    }

    if (oin.oi.d != -9876.5) {
        exit(-5);
    }

    // now construct result
    union lotsa_chars result = { "ABCDEFGHIJKLMNOPQ" };
    return result;
}

struct has_uneven_union return_struct_with_union(void) {
    struct has_uneven_union result = {
        -8765, {"done"}
    };
    return result;
}
)PROG")));
}
// no identifier shadowing (a parameter named l shadows the file-scope static l)
// plus block-scope static storage.
TEST_F(CodegenTest, DISABLED_Chapter18_ScalarMemberAccessDot)
{
    EXPECT_EQ("0\n", CompileAndRun(WrapMain(R"PROG(
/* Test the . operator.
 * Relatively simple tests without nested accesses or members of aggregate
 * types.
 */

struct four_members {
    double d;
    char c;
    long l;
    char *ptr;
};

// helper functions/variables

// get_double and l are used to initialize members
double get_double(void) {
    return 2e12;
}

static long l = 34359738378l;

// validate members (and values derived from members) that are passed as
// parameters
int accept_params(int d_divided, int c_doubled, double l_cast,
                  int dereferenced_ptr, double d, int c, long l, char *ptr) {
    if (d != 4e12 || c != 127 || l != 8589934594l || *ptr != 100 ||
        d_divided != 100.0 || c_doubled != 254 || l_cast != 8589934594.0 ||
        dereferenced_ptr != 100) {
        return 0;
    }

    return 1;  // success
}

int test_auto(void) {
    // test reading, writing, and getting address of members
    // in struct with automatic storage duration
    struct four_members autom;

    // write to all members - assign results of complex expression to members
    autom.d = get_double() * 2.0;  // 4e12
    autom.c = 127;
    autom.l = l / 4;  // 8589934594l

    char chr = 100;
    autom.ptr = &chr;

    // read all members
    if (autom.d != 4e12 || autom.c != 127 || autom.l != 8589934594l ||
        autom.ptr != &chr) {
        return 0;
    }

    // take address of members
    double *d_ptr = &autom.d;
    char *c_ptr = &autom.c;
    if (*d_ptr != 4e12 || *c_ptr != 127) {
        return 0;
    }

    // dereference member
    if (*autom.ptr != 100) {
        return 0;
    }

    // read members and use them in complex expressions (e.g. function calls)
    if (!accept_params(autom.d / 4e10, autom.c * 2, (double)autom.l, *autom.ptr,
                       autom.d, autom.c, autom.l, autom.ptr)) {
        return 0;
    }

    return 1;
}

int test_static(void) {
    // test reading, writing, and getting address of members
    // in struct with static storage duration
    static struct four_members stat;
    static char chr = 100;

    // same test as test_auto above

    // write to all members - assign results of complex expression to members
    stat.d = get_double() * 2.0;  // 4e12
    stat.c = 127;
    stat.l = l / 4;  // 8589934594l

    stat.ptr = &chr;

    // read all members
    if (stat.d != 4e12 || stat.c != 127 || stat.l != 8589934594l ||
        stat.ptr != &chr) {
        return 0;
    }

    // take address of members
    double *d_ptr = &stat.d;
    char *c_ptr = &stat.c;
    if (*d_ptr != 4e12 || *c_ptr != 127) {
        return 0;
    }

    // dereference member
    if (*stat.ptr != 100) {
        return 0;
    }

    // read members and use them in complex expressions (e.g. function calls)
    if (!accept_params(stat.d / 4e10, stat.c * 2, (double)stat.l, *stat.ptr,
                       stat.d, stat.c, stat.l, stat.ptr)) {
        return 0;
    }

    return 1;  // success
}

int main(void) {
    // accessing struct w/ automatic storage duration
    if (!test_auto()) {
        return 1;
    }

    // accessing struct w/ static storage duration
    if (!test_static()) {
        return 2;
    }

    return 0;
}
)PROG")));
}
