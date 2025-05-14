#include "fixture.h"

//
// 1. Simple Types ("specifier_qualifier_list" only)
// These test the "type_name : specifier_qualifier_list" branch with basic type specifiers.
//
// 1. void
// 2. char
// 3. int
// 4. short
// 5. long
// 6. float
// 7. double
// 8. signed
// 9. unsigned
// 10. bool
// 11. _Complex
// 12. _Imaginary
//
TEST_F(ParserTest, TypeVoid)
{
    Type *type = TestType("void");

    EXPECT_EQ(type->kind, TYPE_VOID);
    EXPECT_EQ(type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeChar)
{
    Type *type = TestType("char");

    EXPECT_EQ(type->kind, TYPE_CHAR);
    EXPECT_EQ(type->u.integer.signedness, SIGNED_SIGNED);
    EXPECT_EQ(type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeInt)
{
    Type *type = TestType("int");

    EXPECT_EQ(type->kind, TYPE_INT);
    EXPECT_EQ(type->u.integer.signedness, SIGNED_SIGNED);
    EXPECT_EQ(type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeShort)
{
    Type *type = TestType("short");

    EXPECT_EQ(type->kind, TYPE_SHORT);
    EXPECT_EQ(type->u.integer.signedness, SIGNED_SIGNED);
    EXPECT_EQ(type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeLong)
{
    Type *type = TestType("long");

    EXPECT_EQ(type->kind, TYPE_LONG);
    EXPECT_EQ(type->u.integer.signedness, SIGNED_SIGNED);
    EXPECT_EQ(type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeFloat)
{
    Type *type = TestType("float");

    EXPECT_EQ(type->kind, TYPE_FLOAT);
    EXPECT_EQ(type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeDouble)
{
    Type *type = TestType("double");

    EXPECT_EQ(type->kind, TYPE_DOUBLE);
    EXPECT_EQ(type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeSigned)
{
    Type *type = TestType("signed");

    EXPECT_EQ(type->kind, TYPE_INT);
    EXPECT_EQ(type->u.integer.signedness, SIGNED_SIGNED);
    EXPECT_EQ(type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeUnsigned)
{
    Type *type = TestType("unsigned");

    EXPECT_EQ(type->kind, TYPE_INT);
    EXPECT_EQ(type->u.integer.signedness, SIGNED_UNSIGNED);
    EXPECT_EQ(type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeBool)
{
    Type *type = TestType("_Bool");

    EXPECT_EQ(type->kind, TYPE_BOOL);
    EXPECT_EQ(type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeComplex)
{
    Type *type = TestType("_Complex");

    EXPECT_EQ(type->kind, TYPE_COMPLEX);
    ASSERT_NE(type->u.complex.base, nullptr);
    EXPECT_EQ(type->u.complex.base->kind, TYPE_DOUBLE);
    EXPECT_EQ(type->u.complex.base->qualifiers, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeImaginary)
{
    Type *type = TestType("_Imaginary");

    EXPECT_EQ(type->kind, TYPE_IMAGINARY);
    ASSERT_NE(type->u.complex.base, nullptr);
    EXPECT_EQ(type->u.complex.base->kind, TYPE_DOUBLE);
    EXPECT_EQ(type->u.complex.base->qualifiers, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    free_type(type);
}

//
// 2. Modified Basic Types
// Combinations of type specifiers for integer and floating-point types.
//
// 13. unsigned int
// 14. signed char
// 15. long int
// 16. long long
// 17. unsigned short
// 18. signed long
// 19. long double
// 20. unsigned long long
//

TEST_F(ParserTest, TypeUnsignedInt)
{
    Type *type = TestType("unsigned int");

    EXPECT_EQ(type->kind, TYPE_INT);
    EXPECT_EQ(type->u.integer.signedness, SIGNED_UNSIGNED);
    EXPECT_EQ(type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeSignedChar)
{
    Type *type = TestType("signed char");

    EXPECT_EQ(type->kind, TYPE_CHAR);
    EXPECT_EQ(type->u.integer.signedness, SIGNED_SIGNED);
    EXPECT_EQ(type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeLongInt)
{
    Type *type = TestType("long int");

    EXPECT_EQ(type->kind, TYPE_LONG);
    EXPECT_EQ(type->u.integer.signedness, SIGNED_SIGNED);
    EXPECT_EQ(type->qualifiers, nullptr);
    free_type(type);
}

//
// 3. Qualified Types
// Adding type qualifiers to basic and modified types.
//
// 21. const int
// 22. volatile char
// 23. restrict double
// 24. _Atomic float
// 25. const unsigned int
// 26. volatile long long
// 27. const restrict int
// 28. const volatile unsigned char
//

TEST_F(ParserTest, TypeConstInt)
{
    Type *type = TestType("const int");

    EXPECT_EQ(type->kind, TYPE_INT);
    EXPECT_EQ(type->u.integer.signedness, SIGNED_SIGNED);
    ASSERT_NE(type->qualifiers, nullptr);
    EXPECT_EQ(type->qualifiers->kind, TYPE_QUALIFIER_CONST);
    EXPECT_EQ(type->qualifiers->next, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeVolatileChar)
{
    Type *type = TestType("volatile char");

    EXPECT_EQ(type->kind, TYPE_CHAR);
    EXPECT_EQ(type->u.integer.signedness, SIGNED_SIGNED);
    ASSERT_NE(type->qualifiers, nullptr);
    EXPECT_EQ(type->qualifiers->kind, TYPE_QUALIFIER_VOLATILE);
    EXPECT_EQ(type->qualifiers->next, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeRestrictDouble)
{
    Type *type = TestType("restrict double");

    EXPECT_EQ(type->kind, TYPE_DOUBLE);
    ASSERT_NE(type->qualifiers, nullptr);
    EXPECT_EQ(type->qualifiers->kind, TYPE_QUALIFIER_RESTRICT);
    EXPECT_EQ(type->qualifiers->next, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeAtomicFloat)
{
    Type *type = TestType("_Atomic float");

    EXPECT_EQ(type->kind, TYPE_FLOAT);
    ASSERT_NE(type->qualifiers, nullptr);
    EXPECT_EQ(type->qualifiers->kind, TYPE_QUALIFIER_ATOMIC);
    EXPECT_EQ(type->qualifiers->next, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeConstUnsignedInt)
{
    Type *type = TestType("const unsigned int");

    EXPECT_EQ(type->kind, TYPE_INT);
    EXPECT_EQ(type->u.integer.signedness, SIGNED_UNSIGNED);
    ASSERT_NE(type->qualifiers, nullptr);
    EXPECT_EQ(type->qualifiers->kind, TYPE_QUALIFIER_CONST);
    EXPECT_EQ(type->qualifiers->next, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeVolatileLongLong)
{
    Type *type = TestType("volatile long long");

    EXPECT_EQ(type->kind, TYPE_LONG_LONG);
    EXPECT_EQ(type->u.integer.signedness, SIGNED_SIGNED);
    ASSERT_NE(type->qualifiers, nullptr);
    EXPECT_EQ(type->qualifiers->kind, TYPE_QUALIFIER_VOLATILE);
    EXPECT_EQ(type->qualifiers->next, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeConstRestrictInt)
{
    Type *type = TestType("const restrict int");

    EXPECT_EQ(type->kind, TYPE_INT);
    EXPECT_EQ(type->u.integer.signedness, SIGNED_SIGNED);
    ASSERT_NE(type->qualifiers, nullptr);
    EXPECT_EQ(type->qualifiers->kind, TYPE_QUALIFIER_CONST);
    ASSERT_NE(type->qualifiers->next, nullptr);
    EXPECT_EQ(type->qualifiers->next->kind, TYPE_QUALIFIER_RESTRICT);
    EXPECT_EQ(type->qualifiers->next->next, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeConstVolatileUnsignedChar)
{
    Type *type = TestType("const volatile unsigned char");

    EXPECT_EQ(type->kind, TYPE_CHAR);
    EXPECT_EQ(type->u.integer.signedness, SIGNED_UNSIGNED);
    ASSERT_NE(type->qualifiers, nullptr);
    EXPECT_EQ(type->qualifiers->kind, TYPE_QUALIFIER_CONST);
    ASSERT_NE(type->qualifiers->next, nullptr);
    EXPECT_EQ(type->qualifiers->next->kind, TYPE_QUALIFIER_VOLATILE);
    EXPECT_EQ(type->qualifiers->next->next, nullptr);
    free_type(type);
}

//
// 4. Struct, Union, Enum, and Typedef Types
// Testing complex type specifiers.
//
// 29. struct S
// 30. union U
// 31. enum E
// 32. MyType
// 33. const struct S
// 34. volatile union U
// 35. _Atomic enum E
//

TEST_F(ParserTest, TypeStruct)
{
    Type *type = TestType("struct S");

    EXPECT_EQ(type->kind, TYPE_STRUCT);
    EXPECT_STREQ(type->u.struct_t.name, "S");
    EXPECT_EQ(type->u.struct_t.fields, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeUnion)
{
    Type *type = TestType("union U");

    EXPECT_EQ(type->kind, TYPE_UNION);
    EXPECT_STREQ(type->u.struct_t.name, "U");
    EXPECT_EQ(type->u.struct_t.fields, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeEnum)
{
    Type *type = TestType("enum E");

    EXPECT_EQ(type->kind, TYPE_ENUM);
    EXPECT_STREQ(type->u.enum_t.name, "E");
    EXPECT_EQ(type->u.enum_t.enumerators, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, DISABLED_TypeTypedef)
{
    //TODO: Add typedef MyType to the symbol table.
    Type *type = TestType("MyType");

    EXPECT_EQ(type->kind, TYPE_TYPEDEF_NAME);
    EXPECT_STREQ(type->u.enum_t.name, "MyType");
    EXPECT_EQ(type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeConstStruct)
{
    Type *type = TestType("const struct S");

    EXPECT_EQ(type->kind, TYPE_STRUCT);
    EXPECT_STREQ(type->u.struct_t.name, "S");
    EXPECT_EQ(type->u.struct_t.fields, nullptr);
    ASSERT_NE(type->qualifiers, nullptr);
    EXPECT_EQ(type->qualifiers->kind, TYPE_QUALIFIER_CONST);
    EXPECT_EQ(type->qualifiers->next, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeVolatileUnion)
{
    Type *type = TestType("volatile union U");

    EXPECT_EQ(type->kind, TYPE_UNION);
    EXPECT_STREQ(type->u.struct_t.name, "U");
    EXPECT_EQ(type->u.struct_t.fields, nullptr);
    ASSERT_NE(type->qualifiers, nullptr);
    EXPECT_EQ(type->qualifiers->kind, TYPE_QUALIFIER_VOLATILE);
    EXPECT_EQ(type->qualifiers->next, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeAtomicEnum)
{
    Type *type = TestType("_Atomic enum E");

    EXPECT_EQ(type->kind, TYPE_ENUM);
    EXPECT_STREQ(type->u.enum_t.name, "E");
    EXPECT_EQ(type->u.enum_t.enumerators, nullptr);
    ASSERT_NE(type->qualifiers, nullptr);
    EXPECT_EQ(type->qualifiers->kind, TYPE_QUALIFIER_ATOMIC);
    EXPECT_EQ(type->qualifiers->next, nullptr);
    free_type(type);
}

//
// 5. Atomic Type Specifier
// Testing "_Atomic" with a type name.
//
// 36. _Atomic(int)
// 37. _Atomic(struct S)
// 38. _Atomic(const char)
//
// _Atomic() is not supported in this parser.
//

//
// 6. Pointer Types
// Testing "type_name : specifier_qualifier_list abstract_declarator"
// with "abstract_declarator : pointer".
//
// 39. int *
// 40. char *
// 41. void *
// 42. const int *
// 43. int * const
// 44. volatile char *
// 45. int * volatile
// 46. const restrict int *
// 47. int **
// 48. struct S *
// 49. unsigned long *
// 50. int * const volatile
//

TEST_F(ParserTest, TypeIntPtr)
{
    Type *type = TestType("int *");

    EXPECT_EQ(type->kind, TYPE_POINTER);
    ASSERT_NE(type->u.pointer.target, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.target->kind, TYPE_INT);
    EXPECT_EQ(type->u.pointer.target->u.integer.signedness, SIGNED_SIGNED);
    EXPECT_EQ(type->u.pointer.target->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.qualifiers, nullptr);
    free_type(type);
}

//TODO:
//
// 7. Array Types
// Testing "abstract_declarator : direct_abstract_declarator" with array declarators.
//
// 51. int []
// 52. char [5]
// 53. double [10]
// 54. const int []
// 55. int [const 5]
// 56. int [static 10]
// 57. int [const static 5]
// 58. struct S [3]
// 59. int [*]
// 60. const int [N]
//

//
// 8. Function Types
// Testing "direct_abstract_declarator : '(' parameter_type_list ')'" or "()".
//
// 61. int ()
// 62. void (int)
// 63. char (char *, int)
// 64. struct S (void)
// 65. const int (double, char)
// 66. int (*)(int)
// 67. void (*)(char *)
//

//
// 9. Nested Combinations
// Testing complex combinations of pointers, arrays, and functions.
//
// 68. int *[]
// 69. int [][5]
// 70. char * [3]
// 71. int (*)[5]
// 72. void (*[]) (int)
// 73. int (*[3])(char)
// 74. int **[5]
// 75. struct S *[10]
// 76. int (*(*)[5])(int)
// 77. const int * const [3]
//

//
// 10. Parenthesized and Complex Declarators
// Testing "direct_abstract_declarator : '(' abstract_declarator ')'" and nested constructs.
//
// 78. int (*)
// 79. const char (* const)
// 80. struct S (*[3])
// 81. void (*(*)(int))
// 82. unsigned int (*(*)[5])
// 83. const struct S (*(int))
//

//
// Nested types
//
// Variant 1: Nested Struct with Simple Field
// struct Outer { int x; struct Inner { int y; } inner; }
//
// Variant 2: Struct with Pointer to Itself
// struct Node { int data; struct Node *next; }
//
// Variant 3: Function Pointer with Struct Parameter
// void (*)(struct Pair { int x; int y; })
//
// Variant 4: Nested Struct with Array Field
// struct Container { struct Item { int value; } items[10]; }
//
// Variant 5: Union with Nested Struct and Anonymous Struct
// union Variant { struct { int a; int b; }; struct Named { float x; } named; }
//
