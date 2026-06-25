//
// Chapter 17 — Supporting types: void, sizeof, and dynamic allocation: semantic
// errors.  Imported from "Writing a C Compiler" (tests/chapter_17/invalid_types
// and invalid_types/extra_credit).  Each program parses but violates a void /
// incomplete-type / pointer-conversion rule.  Tests assert on a substring of the
// fatal-error text.
//
// Every program yields a clean, specific diagnostic — no DISABLED_ needed.  Ten
// programs that the compiler previously accepted are now rejected by five
// frontend fixes (semantic/{statements,expressions,typecheck}.c): a value-less
// `return` in a non-void function, ++/-- on a void* (incomplete pointee),
// relational/equality comparison involving a void or void* operand, comparing a
// pointer to a non-null integer, and a named `void` parameter.
//
#include "typecheck_fixture.h"

// --- invalid_types/void -----------------------------------------------------

// *x = foo(); (x is void *) — can't dereference a pointer to void.
TEST_F(PipelineTest, Chapter17_AssignToVoidLvalue_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(extern void *x;

void foo(void) { return; }

int main(void) {
  *x = foo();
  return 0;
}
)"),
                 "Can't dereference pointer to void");
}

// extern void v1; v1 = (void)0; — can't declare a void variable.
TEST_F(PipelineTest, Chapter17_AssignToVoidVar_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(extern void v1;

int main(void) {
  v1 = (void)0;
  return 0;
}
)"),
                 "Void variables not allowed");
}

// a = (void)20; — can't convert void to another type by assignment.
TEST_F(PipelineTest, Chapter17_AssignVoidRval_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
  int a = 10;
  a = (void)20;
  return 0;
}
)"),
                 "Cannot convert type for assignment");
}

// void x; — can't define (allocate) a void object.
TEST_F(PipelineTest, Chapter17_DefineVoid_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
    void x;
    return 0;
}
)"),
                 "No void declarations");
}

// extern void v = 0; — can't initialize a void object.
TEST_F(PipelineTest, Chapter17_InitializedVoid_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(extern void v = 0;

int main(void) { return 0; }
)"),
                 "Void variables not allowed");
}

// flag ? foo() : (a = 3); — a ternary can't have only one void branch.
TEST_F(PipelineTest, Chapter17_MismatchedConditional_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(void foo(void) {
    return;
}

int main(void) {
    int a = 3;
    int flag = 4;
    flag ? foo() : (a = 3);
    return 0;
}
)"),
                 "Invalid operands for conditional");
}

// -(void)10; — can't negate a void expression.
TEST_F(PipelineTest, Chapter17_NegateVoid_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
  -(void)10;
  return 0;
}
)"),
                 "Can only apply unary");
}

// int foo(void) { return; } — a non-void function must return a value.
TEST_F(PipelineTest, Chapter17_NoReturnValue_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int foo(void) {
  return;
}

int main(void) {
  foo();
  return 0;
}
)"),
                 "Non-void function must return a value");
}

// void x(void) { return 1; } — a void function can't return a value.
TEST_F(PipelineTest, Chapter17_NonVoidReturn_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(void x(void) {
  return 1;
}

int main(void) {
  x();
  return 0;
}
)"),
                 "Void function cannot return a value");
}

// void *x(void) { return (void)0; } — can't convert void to a pointer.
TEST_F(PipelineTest, Chapter17_ReturnVoidAsPointer_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(void *x(void) {
  return (void)0;
}
)"),
                 "Cannot convert type for assignment");
}

// arr[(void)1] — a subscript index must be an integer, not void.
TEST_F(PipelineTest, Chapter17_SubscriptVoid_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
  char arr[3];
  return arr[(void)1];
}
)"),
                 "Invalid types for subscript operation");
}

// (void)1 < (void)2 — can't compare void expressions.
TEST_F(PipelineTest, Chapter17_VoidCompare_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
  if ((void)1 < (void)2)
    return 1;
  return 0;
}
)"),
                 "Invalid types for comparison");
}

// x() == (void)10 — can't compare void expressions for equality.
TEST_F(PipelineTest, Chapter17_VoidEquality_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(void x(void);
int main(void) {
    return x() == (void)10;
}
)"),
                 "Invalid operands for comparison");
}

// void foo(void x); — a named void parameter is not allowed.
TEST_F(PipelineTest, Chapter17_VoidFunParams_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(void foo(void x);

int main(void) {
    return 0;
}
)"),
                 "Void parameter not allowed");
}

// --- invalid_types/scalar_expressions ---------------------------------------

// (void)1 && 2 — a void operand isn't scalar.
TEST_F(PipelineTest, Chapter17_AndVoid_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
    return (void)1 && 2;
}
)"),
                 "A scalar operand is required");
}

// (int)(void)3 — can only cast scalar values.
TEST_F(PipelineTest, Chapter17_CastVoid_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
    int y = (int) (void) 3;
    return y;
}
)"),
                 "Can only cast scalar types");
}

// !(1 ? f() : g()) — a void operand isn't scalar.
TEST_F(PipelineTest, Chapter17_NotVoid_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(void f(void);
void g(void);
int main(void) { return !(1 ? f() : g()); }
)"),
                 "A scalar operand is required");
}

// 1 || (void)2 — a void operand isn't scalar.
TEST_F(PipelineTest, Chapter17_OrVoid_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) { return 1 || (void)2; }
)"),
                 "A scalar operand is required");
}

// do { ... } while (f()); (f returns void) — controlling expr must be scalar.
TEST_F(PipelineTest, Chapter17_VoidConditionDoLoop_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(void f(void) { return; }
int main(void) {
  int i = 0;
  do {
    i = i + 1;
  } while (f());
  return 0;
}
)"),
                 "A scalar operand is required");
}

// for (; foo(); ) (foo returns void) — controlling expr must be scalar.
TEST_F(PipelineTest, Chapter17_VoidConditionForLoop_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(void foo(void) {
    return;
}

int main(void) {
    for (int i = 0; foo(); )
        ;
    return 0;
}
)"),
                 "A scalar operand is required");
}

// while ((void)10) — controlling expr must be scalar.
TEST_F(PipelineTest, Chapter17_VoidConditionWhileLoop_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(void f(void) { return; }
int main(void) {
  int i = 0;
  while ((void)10) {
    i = i + 1;
  }
  return 0;
}
)"),
                 "A scalar operand is required");
}

// if ((void)x) — controlling expr must be scalar.
TEST_F(PipelineTest, Chapter17_VoidIfCondition_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
  int x = 10;
  if ((void)x)
    return 0;
  return 1;
}
)"),
                 "A scalar operand is required");
}

// f() ? 1 : 2 (f returns void) — ternary condition must be scalar.
TEST_F(PipelineTest, Chapter17_VoidTernaryCondition_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(void f(void);

int main(void) {
    return f() ? 1 : 2;
}
)"),
                 "A scalar operand is required");
}

// --- invalid_types/pointer_conversions --------------------------------------

// (void *)0 == 20ul — can't compare a pointer to a non-null integer.
TEST_F(PipelineTest, Chapter17_CompareVoidPtrToInt_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
    return (void *)0 == 20ul;
}
)"),
                 "Incompatible pointer types");
}

// ptr < arr + 1 (void * vs int *) — can't relationally compare to void *.
TEST_F(PipelineTest, Chapter17_CompareVoidToOtherPointer_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
  int arr[3] = {1, 2, 3};
  void *ptr = (void *)arr;
  return ptr < arr + 1;
}
)"),
                 "Invalid types for comparison");
}

// void *v = x; (x is unsigned long) — can't convert a non-pointer to a pointer.
TEST_F(PipelineTest, Chapter17_ConvertUlongToVoidPtr_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
  unsigned long x = 0;
  void *v = x;
}
)"),
                 "Cannot convert type for assignment");
}

// return x; (x is void *) — can't convert void * to int by assignment.
TEST_F(PipelineTest, Chapter17_ConvertVoidPtrToInt_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
  void *x = 0;
  return x;
}
)"),
                 "Cannot convert type for assignment");
}

// 10 * (void *)0 — usual arithmetic conversions can't apply to void *.
TEST_F(PipelineTest, Chapter17_UsualArithmeticConversionsPtr_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
  int i = 10 * (void *)0;
}
)"),
                 "Can only multiply arithmetic types");
}

// --- invalid_types/incomplete_types -----------------------------------------

// x = x + 1; (x is void *) — no pointer arithmetic on incomplete types.
TEST_F(PipelineTest, Chapter17_AddVoidPointer_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(void *malloc(unsigned long size);

int main(void) {
  void *x = malloc(100);
  x = x + 1;
  return 0;
}
)"),
                 "Invalid operands for addition");
}

// sizeof x (x is a function) — can't apply sizeof to a function.
TEST_F(PipelineTest, Chapter17_SizeofFunction_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int x(void) { return 0; }

int main(void) { return sizeof x; }
)"),
                 "Can't apply sizeof to a function type");
}

// sizeof(void[3]) — an array element type must be complete.
TEST_F(PipelineTest, Chapter17_SizeofVoidArray_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
    return sizeof(void[3]);
}
)"),
                 "Array of incomplete type");
}

// sizeof((void)x) — can't apply sizeof to an incomplete-type expression.
TEST_F(PipelineTest, Chapter17_SizeofVoidExpression_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
  int x;
  return sizeof((void)x);
}
)"),
                 "Can't apply sizeof to incomplete type");
}

// sizeof (void) — can only apply sizeof to complete types.
TEST_F(PipelineTest, Chapter17_SizeofVoid_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
    return sizeof (void);
}
)"),
                 "Can't apply sizeof to incomplete type");
}

// x - null; (both void *) — no pointer arithmetic on incomplete types.
TEST_F(PipelineTest, Chapter17_SubVoidPointer_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
  int y;
  void *x = &y;
  void *null = 0;
  return x - null;
}
)"),
                 "Invalid operands for subtraction");
}

// (1 ? int_ptr : void_ptr)[1] — can't subscript a pointer to incomplete type.
TEST_F(PipelineTest, Chapter17_SubscriptVoidPointerConditional_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
  int arr[3] = {1, 2, 3};
  void *void_ptr = arr;
  int *int_ptr = arr + 1;
  return (1 ? int_ptr : void_ptr)[1];
}
)"),
                 "Invalid types for subscript operation");
}

// v[0] (v is void *) — can't subscript a pointer to incomplete type.
TEST_F(PipelineTest, Chapter17_IncompleteSubscriptVoid_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
  int x = 10;
  void *v = &x;
  v[0];
  return 0;
}
)"),
                 "Invalid types for subscript operation");
}

// (void(*)[3]) 4 — an array element type must be complete.
TEST_F(PipelineTest, Chapter17_VoidArrayInCast_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
    (void(*)[3]) 4;
    return 0;
}
)"),
                 "Array of incomplete type");
}

// int arr(void foo[3]) — an array element type must be complete.
TEST_F(PipelineTest, Chapter17_VoidArrayInParamType_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int arr(void foo[3]) { return 3; }

int main(void) { return 0; }
)"),
                 "Array of incomplete type");
}

// extern void (*ptr)[3][4]; — nested incomplete array element type.
TEST_F(PipelineTest, Chapter17_VoidArrayNestedInDeclaration_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(extern void (*ptr)[3][4];

void *foo(void) {
    return ptr;
}
)"),
                 "Array of incomplete type");
}

// void (*ptr)[3] = malloc(3); — incomplete array element type.
TEST_F(PipelineTest, Chapter17_VoidArrayPointerInDeclaration_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(void *malloc(unsigned long size);

int main(void) {
    void (*ptr)[3] = malloc(3);
    return ptr == 0;
}
)"),
                 "Array of incomplete type");
}

// int foo(void (*bad_array)[3]) — incomplete array element type.
TEST_F(PipelineTest, Chapter17_VoidArrayPointerInParamType_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int foo(void (*bad_array)[3]) {
    return bad_array == 0;
}

int main(void) {
    return 0;
}
)"),
                 "Array of incomplete type");
}

// void arr[3]; — an array element type must be complete.
TEST_F(PipelineTest, Chapter17_VoidArray_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
    void arr[3];
    return 0;
}
)"),
                 "Array of incomplete type");
}

// --- invalid_types/extra_credit ---------------------------------------------

// x << f(); (f returns void) — bitshift operands must be integers.
TEST_F(PipelineTest, Chapter17_BitshiftVoid_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(void f(void){
    return;
}

int main(void) {
    int x = 10;
    x << f();
    return 0;
}
)"),
                 "Shift operators require integer operands");
}

// x & (void)y; — bitwise operands must be integers.
TEST_F(PipelineTest, Chapter17_BitwiseVoid_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(int main(void) {
    int x = 10;
    int y = 11;
    x & (void) y;
    return 0;
}
)"),
                 "Bitwise operators require integer operands");
}

// buff += 3; (buff is void *) — no compound arithmetic on void *.
TEST_F(PipelineTest, Chapter17_CompoundAddVoidPointer_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(void *malloc(unsigned long size);

int main(void) {
    void *buff = malloc(100);
    buff += 3;
    return 0;
}
)"),
                 "Invalid operands for compound assignment");
}

// buff -= 0; (buff is void *) — no compound arithmetic on void *.
TEST_F(PipelineTest, Chapter17_CompoundSubVoidPointer_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(void *malloc(unsigned long size);

int main(void) {
    void *buff = malloc(100);
    buff -= 0;
    return 0;
}
)"),
                 "Invalid operands for compound assignment");
}

// x += f(); (f returns void) — compound rval can't be void.
TEST_F(PipelineTest, Chapter17_CompoundVoidRvalAdd_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(void f(void) {
    return;
}

int main(void) {
    int *x = 0;
    x += f();
    return 0;
}
)"),
                 "Pointer arithmetic requires integer operand");
}

// x >>= f(); (f returns void) — compound rval can't be void.
TEST_F(PipelineTest, Chapter17_CompoundVoidRvalBitshift_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(void f(void) {
    return;
}

int main(void) {
    int x = 10;
    x >>= f();
    return 0;
}
)"),
                 "Invalid operands for compound assignment");
}

// x *= f(); (f returns void) — compound rval can't be void.
TEST_F(PipelineTest, Chapter17_CompoundVoidRval_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(void f(void) {
    return;
}

int main(void) {
    int x = 10;
    x *= f();
    return 0;
}
)"),
                 "Invalid operands for compound assignment");
}

// buff--; (buff is void *) — can't decrement a pointer to void.
TEST_F(PipelineTest, Chapter17_PostfixDecrVoidPointer_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(void *malloc(unsigned long size);

int main(void) {
    void *buff = malloc(100);
    buff--;
    return 0;
}
)"),
                 "Cannot increment/decrement pointer to incomplete type");
}

// (*x)-- (x is void *) — can't dereference a pointer to void.
TEST_F(PipelineTest, Chapter17_PostfixDecrVoid_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(extern void *x;

int main(void) {
    ++(*x)--;
    return 0;
}
)"),
                 "Can't dereference pointer to void");
}

// buff++; (buff is void *) — can't increment a pointer to void.
TEST_F(PipelineTest, Chapter17_PostfixIncrVoidPointer_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(void *malloc(unsigned long size);

int main(void) {
    void *buff = malloc(100);
    buff++;
    return 0;
}
)"),
                 "Cannot increment/decrement pointer to incomplete type");
}

// --buff; (buff is void *) — can't decrement a pointer to void.
TEST_F(PipelineTest, Chapter17_PrefixDecrVoidPointer_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(void *malloc(unsigned long size);

int main(void) {
    void *buff = malloc(100);
    --buff;
    return 0;
}
)"),
                 "Cannot increment/decrement pointer to incomplete type");
}

// ++buff; (buff is void *) — can't increment a pointer to void.
TEST_F(PipelineTest, Chapter17_PrefixIncrVoidPointer_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(void *malloc(unsigned long size);

int main(void) {
    void *buff = malloc(100);
    ++buff;
    return 0;
}
)"),
                 "Cannot increment/decrement pointer to incomplete type");
}

// ++(*x); (x is void *) — can't dereference a pointer to void.
TEST_F(PipelineTest, Chapter17_PrefixIncrVoid_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(extern void *x;

int main(void) {
    ++(*x);
    return 0;
}
)"),
                 "Can't dereference pointer to void");
}

// switch(f()) (f returns void) — switch controlling expr must be an integer.
TEST_F(PipelineTest, Chapter17_SwitchVoid_Neg)
{
    EXPECT_DEATH(RunPipeline(R"(void f(void) {
    return;
}

int main(void) {
    switch(f()) {
        default: return 0;
    }
}
)"),
                 "Switch controlling expression must be of integer type");
}
