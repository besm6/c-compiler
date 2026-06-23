//
// Chapter 15 — Arrays / pointer arithmetic: invalid parser input.  Imported from
// "Writing a C Compiler" (tests/chapter_15/invalid_parse).  Each program contains a
// malformed (abstract) array/declarator that the parser rejects.  Tests assert on a
// substring of the diagnostic.
//
// Reclassified vs. the book:
//   * Many invalid_parse programs parse cleanly for us and are caught by the type
//     checker instead (array-of-functions, return_array, the abstract cast-to-array
//     declarators); they live in semantic/chapter15_tests.cpp.
//   * cast_to_array_type_3 lives in the book's invalid_types, but a nested array
//     abstract declarator like `long(([2])[3])` is a parse error for us ("Empty type
//     specifier list"), so it is here.
//
// Three of the book's invalid_parse programs are accepted by our front end today and
// carry DISABLED_ markers with the gap noted: our array declarator parser does not
// reject a non-integer size (int x[2.0]), a negative size (int arr[-3]), or an empty
// brace initializer (int arr[1] = {}; valid as of C23 anyway).
//
#include "fixture.h"

// --- invalid_parse ----------------------------------------------------------

// int foo[[10]]; — a second '[' cannot open an array size expression.
TEST_F(ParserTest, Chapter15_MalformedArrayDeclarator_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile(R"(int main(void) {
    int foo[[10]];
    return 0;
}
)")),
                 "Expected primary expression");
}

// int (*)(ptr_to_array[3]) = 0; — a parenthesized (*) is not a valid named declarator.
TEST_F(ParserTest, Chapter15_MalformedArrayDeclarator2_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile(R"(int main(void) {
    int (*)(ptr_to_array[3]) = 0;
    return 0;
}
)")),
                 "Expected identifier or");
}

// int [3] arr = {1, 2, 3}; — an array size must follow the identifier, not precede it.
TEST_F(ParserTest, Chapter15_MalformedArrayDeclarator3_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile(R"(int main(void) {
    int [3] arr  = {1, 2, 3};
    return 0;
}
)")),
                 "Expected identifier or");
}

// (int[3] *)0 — a pointer declarator cannot follow an array size in an abstract declarator.
TEST_F(ParserTest, Chapter15_MalformedAbstractArrayDeclarator_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile(R"(int main(void) {
    return (int[3] *)0;
}
)")),
                 "expected ')'");
}

// (*[3]) foo — invalid cast syntax, missing type specifier.
TEST_F(ParserTest, Chapter15_MalformedTypeName_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile(R"(int main(void) {
    int a = 4;
    int *foo = &a;
    int *bar[3] = (*[3]) foo;
    return 0;
}
)")),
                 "Expected primary expression");
}

// ([3](*)) ptr — invalid declarator and missing type specifier.
TEST_F(ParserTest, Chapter15_MalformedTypeName2_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile(R"(int main(void) {
    int *ptr;
    int *array_pointer[3] = ([3](*)) ptr;
    return 0;
}
)")),
                 "Expected primary expression");
}

// vals[indices[1]; — the outer subscript is never closed.
TEST_F(ParserTest, Chapter15_MismatchedSubscript_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile(R"(int main(void) {
    int indices[3] = {1, 2, 3};
    int vals[3] = {4, 5, 6};
    return vals[indices[1];
}
)")),
                 "expected ']'");
}

// arr[1; — an unclosed subscript.
TEST_F(ParserTest, Chapter15_UnclosedSubscript_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile(R"(int main(void) {
    int arr[] = {1, 2, 3};
    return arr[1;
}
)")),
                 "expected ']'");
}

// {1, 2; — an unclosed initializer list.
TEST_F(ParserTest, Chapter15_UnclosedInitializer_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile(R"(int main(void) {
    int arr = {1, 2;
    return arr[0];
}
)")),
                 "expected '}'");
}

// {{1, 2}, {3, 4}; — an unclosed nested initializer list.
TEST_F(ParserTest, Chapter15_UnclosedNestedInitializer_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile(R"(int main(void) {
    int arr[2][2] = {{ 1, 2}, {3, 4};
    return arr[0][0];
}
)")),
                 "expected '}'");
}

// --- accepted today (front-end gaps) ----------------------------------------

// int x[2.0]; — a non-integer array size is accepted today (no diagnostic for a
// floating array dimension).
TEST_F(ParserTest, DISABLED_Chapter15_DoubleDeclarator_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile(R"(int main(void) {
    int x[2.0];
}
)")),
                 "array size");
}

// int arr[-3]; — a negative array dimension is accepted today (no range check on the
// array size literal).
TEST_F(ParserTest, DISABLED_Chapter15_NegativeArrayDimension_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile(R"(int main(void)
{
    int arr[-3];
    return 0;
}
)")),
                 "array size");
}

// int arr[1] = {}; — an empty initializer list is accepted today (and is valid as of
// C23, so this is arguably no longer a negative case).
TEST_F(ParserTest, DISABLED_Chapter15_EmptyInitializerList_Neg)
{
    EXPECT_DEATH(parse(CreateTempFile(R"(int main(void) {
    int arr[1] = {};
    return 0;
}
)")),
                 "initializer");
}
