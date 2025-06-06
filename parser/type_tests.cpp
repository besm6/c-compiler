//
// Tests for types are split into ten groups:
// 1. Simple Types ("specifier_qualifier_list" only)
// 2. Modified Basic Types
// 3. Qualified Types
// 4. Struct, Union, Enum, and Typedef Types
// 5. Atomic Type Specifier
// 6. Pointer Types
// 7. Array Types
// 8. Function Types
// 9. Nested Combinations
// 10. Parenthesized and Complex Declarators
//
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
    EXPECT_EQ(type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeInt)
{
    Type *type = TestType("int");

    EXPECT_EQ(type->kind, TYPE_INT);
    EXPECT_EQ(type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeShort)
{
    Type *type = TestType("short");

    EXPECT_EQ(type->kind, TYPE_SHORT);
    EXPECT_EQ(type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeLong)
{
    Type *type = TestType("long");

    EXPECT_EQ(type->kind, TYPE_LONG);
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
    EXPECT_EQ(type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeUnsigned)
{
    Type *type = TestType("unsigned");

    EXPECT_EQ(type->kind, TYPE_UINT);
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

    EXPECT_EQ(type->kind, TYPE_UINT);
    EXPECT_EQ(type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeSignedChar)
{
    Type *type = TestType("signed char");

    EXPECT_EQ(type->kind, TYPE_SCHAR);
    EXPECT_EQ(type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeLongInt)
{
    Type *type = TestType("long int");

    EXPECT_EQ(type->kind, TYPE_LONG);
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
    ASSERT_NE(type->qualifiers, nullptr);
    EXPECT_EQ(type->qualifiers->kind, TYPE_QUALIFIER_CONST);
    EXPECT_EQ(type->qualifiers->next, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeVolatileChar)
{
    Type *type = TestType("volatile char");

    EXPECT_EQ(type->kind, TYPE_CHAR);
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

    EXPECT_EQ(type->kind, TYPE_UINT);
    ASSERT_NE(type->qualifiers, nullptr);
    EXPECT_EQ(type->qualifiers->kind, TYPE_QUALIFIER_CONST);
    EXPECT_EQ(type->qualifiers->next, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeVolatileLongLong)
{
    Type *type = TestType("volatile long long");

    EXPECT_EQ(type->kind, TYPE_LONG_LONG);
    ASSERT_NE(type->qualifiers, nullptr);
    EXPECT_EQ(type->qualifiers->kind, TYPE_QUALIFIER_VOLATILE);
    EXPECT_EQ(type->qualifiers->next, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeConstRestrictInt)
{
    Type *type = TestType("const restrict int");

    EXPECT_EQ(type->kind, TYPE_INT);
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

    EXPECT_EQ(type->kind, TYPE_UCHAR);
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

TEST_F(ParserTest, TypeTypedef)
{
    // Add typedef MyType to the symbol table.
    nametab_define("MyType", TOKEN_TYPEDEF_NAME, 0);

    Type *type = TestType("MyType");

    EXPECT_EQ(type->kind, TYPE_TYPEDEF_NAME);
    EXPECT_STREQ(type->u.enum_t.name, "MyType");
    EXPECT_EQ(type->qualifiers, nullptr);
    free_type(type);
    nametab_remove("MyType");
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
TEST_F(ParserTest, TypeAtomicPtr)
{
    Type *type = TestType("_Atomic(int)");

    EXPECT_EQ(type->kind, TYPE_ATOMIC);
    ASSERT_NE(type->u.atomic.base, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.atomic.base->kind, TYPE_INT);
    EXPECT_EQ(type->u.atomic.base->qualifiers, nullptr);
    free_type(type);
}

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
    EXPECT_EQ(type->u.pointer.target->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeCharPtr)
{
    Type *type = TestType("char *");

    EXPECT_EQ(type->kind, TYPE_POINTER);
    ASSERT_NE(type->u.pointer.target, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.target->kind, TYPE_CHAR);
    EXPECT_EQ(type->u.pointer.target->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeVoidPtr)
{
    Type *type = TestType("void *");

    EXPECT_EQ(type->kind, TYPE_POINTER);
    ASSERT_NE(type->u.pointer.target, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.target->kind, TYPE_VOID);
    EXPECT_EQ(type->u.pointer.target->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeConstIntPtr)
{
    Type *type = TestType("const int *");

    EXPECT_EQ(type->kind, TYPE_POINTER);
    ASSERT_NE(type->u.pointer.target, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.target->kind, TYPE_INT);
    ASSERT_NE(type->u.pointer.target->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.target->qualifiers->kind, TYPE_QUALIFIER_CONST);
    EXPECT_EQ(type->u.pointer.target->qualifiers->next, nullptr);
    EXPECT_EQ(type->u.pointer.qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeIntPtrConst)
{
    Type *type = TestType("int * const");

    EXPECT_EQ(type->kind, TYPE_POINTER);
    ASSERT_NE(type->u.pointer.target, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.target->kind, TYPE_INT);
    EXPECT_EQ(type->u.pointer.target->qualifiers, nullptr);
    ASSERT_NE(type->u.pointer.qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.qualifiers->kind, TYPE_QUALIFIER_CONST);
    EXPECT_EQ(type->u.pointer.qualifiers->next, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeVolatileCharPtr)
{
    Type *type = TestType("volatile char *");

    EXPECT_EQ(type->kind, TYPE_POINTER);
    ASSERT_NE(type->u.pointer.target, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.target->kind, TYPE_CHAR);
    ASSERT_NE(type->u.pointer.target->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.target->qualifiers->kind, TYPE_QUALIFIER_VOLATILE);
    EXPECT_EQ(type->u.pointer.target->qualifiers->next, nullptr);
    EXPECT_EQ(type->u.pointer.qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeIntPtrVolatile)
{
    Type *type = TestType("int * volatile");

    EXPECT_EQ(type->kind, TYPE_POINTER);
    ASSERT_NE(type->u.pointer.target, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.target->kind, TYPE_INT);
    EXPECT_EQ(type->u.pointer.target->qualifiers, nullptr);
    ASSERT_NE(type->u.pointer.qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.qualifiers->kind, TYPE_QUALIFIER_VOLATILE);
    EXPECT_EQ(type->u.pointer.qualifiers->next, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeConstRestrictIntPtr)
{
    Type *type = TestType("const restrict int *");

    EXPECT_EQ(type->kind, TYPE_POINTER);
    ASSERT_NE(type->u.pointer.target, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.target->kind, TYPE_INT);
    ASSERT_NE(type->u.pointer.target->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.target->qualifiers->kind, TYPE_QUALIFIER_CONST);
    ASSERT_NE(type->u.pointer.target->qualifiers->next, nullptr);
    EXPECT_EQ(type->u.pointer.target->qualifiers->next->kind, TYPE_QUALIFIER_RESTRICT);
    EXPECT_EQ(type->u.pointer.target->qualifiers->next->next, nullptr);
    EXPECT_EQ(type->u.pointer.qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeIntPtrPtr)
{
    Type *type = TestType("int **");

    EXPECT_EQ(type->kind, TYPE_POINTER);
    ASSERT_NE(type->u.pointer.target, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.target->kind, TYPE_POINTER);
    EXPECT_EQ(type->u.pointer.target->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.target->u.pointer.target->kind, TYPE_INT);
    EXPECT_EQ(type->u.pointer.target->u.pointer.target->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.target->u.pointer.qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeStructPtr)
{
    Type *type = TestType("struct S *");

    EXPECT_EQ(type->kind, TYPE_POINTER);
    ASSERT_NE(type->u.pointer.target, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.target->kind, TYPE_STRUCT);
    EXPECT_STREQ(type->u.pointer.target->u.struct_t.name, "S");
    EXPECT_EQ(type->u.pointer.target->u.struct_t.fields, nullptr);
    EXPECT_EQ(type->u.pointer.target->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeUnsignedLongPtr)
{
    Type *type = TestType("unsigned long *");

    EXPECT_EQ(type->kind, TYPE_POINTER);
    ASSERT_NE(type->u.pointer.target, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.target->kind, TYPE_ULONG);
    EXPECT_EQ(type->u.pointer.target->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeIntPtrConstVolatile)
{
    Type *type = TestType("int * const volatile");

    EXPECT_EQ(type->kind, TYPE_POINTER);
    ASSERT_NE(type->u.pointer.target, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.target->kind, TYPE_INT);
    EXPECT_EQ(type->u.pointer.target->qualifiers, nullptr);
    ASSERT_NE(type->u.pointer.qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.qualifiers->kind, TYPE_QUALIFIER_CONST);
    ASSERT_NE(type->u.pointer.qualifiers->next, nullptr);
    EXPECT_EQ(type->u.pointer.qualifiers->next->kind, TYPE_QUALIFIER_VOLATILE);
    EXPECT_EQ(type->u.pointer.qualifiers->next->next, nullptr);
    free_type(type);
}

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
TEST_F(ParserTest, TypeIntArray)
{
    Type *type = TestType("int []");

    EXPECT_EQ(type->kind, TYPE_ARRAY);
    ASSERT_NE(type->u.array.element, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.array.element->kind, TYPE_INT);
    EXPECT_EQ(type->u.array.element->qualifiers, nullptr);
    EXPECT_EQ(type->u.array.size, nullptr);
    EXPECT_EQ(type->u.array.qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeCharArray5)
{
    Type *type = TestType("char [5]");

    EXPECT_EQ(type->kind, TYPE_ARRAY);
    ASSERT_NE(type->u.array.element, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.array.element->kind, TYPE_CHAR);
    EXPECT_EQ(type->u.array.element->qualifiers, nullptr);
    ASSERT_NE(type->u.array.size, nullptr);
    EXPECT_EQ(type->u.array.size->kind, EXPR_LITERAL);
    EXPECT_EQ(type->u.array.size->u.literal->kind, LITERAL_INT);
    EXPECT_EQ(type->u.array.size->u.literal->u.int_val, 5);
    EXPECT_EQ(type->u.array.qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeDoubleArray10)
{
    Type *type = TestType("double [10]");

    EXPECT_EQ(type->kind, TYPE_ARRAY);
    ASSERT_NE(type->u.array.element, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.array.element->kind, TYPE_DOUBLE);
    EXPECT_EQ(type->u.array.element->qualifiers, nullptr);
    ASSERT_NE(type->u.array.size, nullptr);
    EXPECT_EQ(type->u.array.size->kind, EXPR_LITERAL);
    EXPECT_EQ(type->u.array.size->u.literal->kind, LITERAL_INT);
    EXPECT_EQ(type->u.array.size->u.literal->u.int_val, 10);
    EXPECT_EQ(type->u.array.qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeConstIntArray)
{
    Type *type = TestType("const int []");

    EXPECT_EQ(type->kind, TYPE_ARRAY);
    ASSERT_NE(type->u.array.element, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.array.element->kind, TYPE_INT);
    ASSERT_NE(type->u.array.element->qualifiers, nullptr);
    EXPECT_EQ(type->u.array.element->qualifiers->kind, TYPE_QUALIFIER_CONST);
    EXPECT_EQ(type->u.array.element->qualifiers->next, nullptr);
    EXPECT_EQ(type->u.array.size, nullptr);
    EXPECT_EQ(type->u.array.qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeIntArrayConst5)
{
    Type *type = TestType("int [const 5]");

    EXPECT_EQ(type->kind, TYPE_ARRAY);
    ASSERT_NE(type->u.array.element, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.array.element->kind, TYPE_INT);
    EXPECT_EQ(type->u.array.element->qualifiers, nullptr);
    ASSERT_NE(type->u.array.size, nullptr);
    EXPECT_EQ(type->u.array.size->kind, EXPR_LITERAL);
    EXPECT_EQ(type->u.array.size->u.literal->kind, LITERAL_INT);
    EXPECT_EQ(type->u.array.size->u.literal->u.int_val, 5);
    ASSERT_NE(type->u.array.qualifiers, nullptr);
    EXPECT_EQ(type->u.array.qualifiers->kind, TYPE_QUALIFIER_CONST);
    EXPECT_EQ(type->u.array.qualifiers->next, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeIntArrayStatic10)
{
    Type *type = TestType("int [static 10]");

    EXPECT_EQ(type->kind, TYPE_ARRAY);
    ASSERT_NE(type->u.array.element, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.array.element->kind, TYPE_INT);
    EXPECT_EQ(type->u.array.element->qualifiers, nullptr);
    ASSERT_NE(type->u.array.size, nullptr);
    EXPECT_EQ(type->u.array.size->kind, EXPR_LITERAL);
    EXPECT_EQ(type->u.array.size->u.literal->kind, LITERAL_INT);
    EXPECT_EQ(type->u.array.size->u.literal->u.int_val, 10);
    EXPECT_EQ(type->u.array.qualifiers, nullptr);
    EXPECT_TRUE(type->u.array.is_static);
    free_type(type);
}

TEST_F(ParserTest, TypeIntArrayConstStatic5)
{
    Type *type = TestType("int [const static 5]");

    EXPECT_EQ(type->kind, TYPE_ARRAY);
    ASSERT_NE(type->u.array.element, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.array.element->kind, TYPE_INT);
    EXPECT_EQ(type->u.array.element->qualifiers, nullptr);
    ASSERT_NE(type->u.array.size, nullptr);
    EXPECT_EQ(type->u.array.size->kind, EXPR_LITERAL);
    EXPECT_EQ(type->u.array.size->u.literal->kind, LITERAL_INT);
    EXPECT_EQ(type->u.array.size->u.literal->u.int_val, 5);
    ASSERT_NE(type->u.array.qualifiers, nullptr);
    EXPECT_EQ(type->u.array.qualifiers->kind, TYPE_QUALIFIER_CONST);
    EXPECT_EQ(type->u.array.qualifiers->next, nullptr);
    EXPECT_TRUE(type->u.array.is_static);
    free_type(type);
}

TEST_F(ParserTest, TypeStructArray3)
{
    Type *type = TestType("struct S [3]");

    EXPECT_EQ(type->kind, TYPE_ARRAY);
    ASSERT_NE(type->u.array.element, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.array.element->kind, TYPE_STRUCT);
    EXPECT_STREQ(type->u.array.element->u.struct_t.name, "S");
    EXPECT_EQ(type->u.array.element->u.struct_t.fields, nullptr);
    EXPECT_EQ(type->u.array.element->qualifiers, nullptr);
    ASSERT_NE(type->u.array.size, nullptr);
    EXPECT_EQ(type->u.array.size->kind, EXPR_LITERAL);
    EXPECT_EQ(type->u.array.size->u.literal->kind, LITERAL_INT);
    EXPECT_EQ(type->u.array.size->u.literal->u.int_val, 3);
    EXPECT_EQ(type->u.array.qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeConstIntArrayStar)
{
    Type *type = TestType("const int [*]");

    EXPECT_EQ(type->kind, TYPE_ARRAY);
    ASSERT_NE(type->u.array.element, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.array.element->kind, TYPE_INT);
    ASSERT_NE(type->u.array.element->qualifiers, nullptr);
    EXPECT_EQ(type->u.array.element->qualifiers->kind, TYPE_QUALIFIER_CONST);
    EXPECT_EQ(type->u.array.element->qualifiers->next, nullptr);
    EXPECT_EQ(type->u.array.size, nullptr);
    EXPECT_EQ(type->u.array.qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeConstIntArrayN)
{
    Type *type = TestType("const int [N]");

    EXPECT_EQ(type->kind, TYPE_ARRAY);
    ASSERT_NE(type->u.array.element, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.array.element->kind, TYPE_INT);
    ASSERT_NE(type->u.array.element->qualifiers, nullptr);
    EXPECT_EQ(type->u.array.element->qualifiers->kind, TYPE_QUALIFIER_CONST);
    EXPECT_EQ(type->u.array.element->qualifiers->next, nullptr);
    ASSERT_NE(type->u.array.size, nullptr);
    EXPECT_EQ(type->u.array.size->kind, EXPR_VAR);
    EXPECT_STREQ(type->u.array.size->u.var, "N");
    EXPECT_EQ(type->u.array.qualifiers, nullptr);
    free_type(type);
}

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
TEST_F(ParserTest, TypeIntFunc)
{
    Type *type = TestType("int ()");

    EXPECT_EQ(type->kind, TYPE_FUNCTION);
    ASSERT_NE(type->u.function.return_type, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.function.return_type->kind, TYPE_INT);
    EXPECT_EQ(type->u.function.return_type->qualifiers, nullptr);
    EXPECT_EQ(type->u.function.params, nullptr);
    EXPECT_FALSE(type->u.function.variadic);
    free_type(type);
}

TEST_F(ParserTest, TypeVoidFuncInt)
{
    Type *type = TestType("void (int)");

    EXPECT_EQ(type->kind, TYPE_FUNCTION);
    ASSERT_NE(type->u.function.return_type, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.function.return_type->kind, TYPE_VOID);
    EXPECT_EQ(type->u.function.return_type->qualifiers, nullptr);
    ASSERT_NE(type->u.function.params, nullptr);
    EXPECT_FALSE(type->u.function.variadic);
    ASSERT_NE(type->u.function.params, nullptr);
    EXPECT_EQ(type->u.function.params->next, nullptr);
    EXPECT_EQ(type->u.function.params->name, nullptr);
    ASSERT_NE(type->u.function.params->type, nullptr);
    EXPECT_EQ(type->u.function.params->type->kind, TYPE_INT);
    EXPECT_EQ(type->u.function.params->type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeVoidFuncFloatVariadic)
{
    Type *type = TestType("void (double, ...)");

    EXPECT_EQ(type->kind, TYPE_FUNCTION);
    ASSERT_NE(type->u.function.return_type, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.function.return_type->kind, TYPE_VOID);
    EXPECT_EQ(type->u.function.return_type->qualifiers, nullptr);
    ASSERT_NE(type->u.function.params, nullptr);
    EXPECT_TRUE(type->u.function.variadic);
    ASSERT_NE(type->u.function.params, nullptr);
    EXPECT_EQ(type->u.function.params->next, nullptr);
    EXPECT_EQ(type->u.function.params->name, nullptr);
    ASSERT_NE(type->u.function.params->type, nullptr);
    EXPECT_EQ(type->u.function.params->type->kind, TYPE_DOUBLE);
    EXPECT_EQ(type->u.function.params->type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeCharFuncCharPtrInt)
{
    Type *type = TestType("char (char *, int)");

    EXPECT_EQ(type->kind, TYPE_FUNCTION);
    EXPECT_EQ(type->qualifiers, nullptr);

    ASSERT_NE(type->u.function.return_type, nullptr);
    EXPECT_EQ(type->u.function.return_type->kind, TYPE_CHAR);
    EXPECT_EQ(type->u.function.return_type->qualifiers, nullptr);

    EXPECT_FALSE(type->u.function.variadic);

    ASSERT_NE(type->u.function.params, nullptr);
    const Param *p1 = type->u.function.params;
    ASSERT_NE(p1, nullptr);
    const Param *p2 = p1->next;
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p2->next, nullptr);

    EXPECT_EQ(p1->name, nullptr);
    ASSERT_NE(p1->type, nullptr);
    EXPECT_EQ(p1->type->kind, TYPE_POINTER);
    ASSERT_NE(p1->type->u.pointer.target, nullptr);
    EXPECT_EQ(p1->type->qualifiers, nullptr);
    EXPECT_EQ(p1->type->u.pointer.target->kind, TYPE_CHAR);
    EXPECT_EQ(p1->type->u.pointer.target->qualifiers, nullptr);
    EXPECT_EQ(p1->type->u.pointer.qualifiers, nullptr);

    EXPECT_EQ(p2->name, nullptr);
    ASSERT_NE(p2->type, nullptr);
    EXPECT_EQ(p2->type->kind, TYPE_INT);
    EXPECT_EQ(p2->type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeStructFuncVoid)
{
    Type *type = TestType("struct S (void)");

    EXPECT_EQ(type->kind, TYPE_FUNCTION);
    EXPECT_EQ(type->qualifiers, nullptr);

    ASSERT_NE(type->u.function.return_type, nullptr);
    EXPECT_EQ(type->u.function.return_type->kind, TYPE_STRUCT);
    EXPECT_STREQ(type->u.function.return_type->u.struct_t.name, "S");
    EXPECT_EQ(type->u.function.return_type->u.struct_t.fields, nullptr);
    EXPECT_EQ(type->u.function.return_type->qualifiers, nullptr);

    EXPECT_FALSE(type->u.function.variadic);

    ASSERT_NE(type->u.function.params, nullptr);
    const Param *p1 = type->u.function.params;
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->next, nullptr);

    EXPECT_EQ(p1->name, nullptr);
    ASSERT_NE(p1->type, nullptr);
    EXPECT_EQ(p1->type->kind, TYPE_VOID);
    EXPECT_EQ(p1->type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeConstIntFuncDoubleChar)
{
    Type *type = TestType("const int (double, char)");

    EXPECT_EQ(type->kind, TYPE_FUNCTION);
    EXPECT_EQ(type->qualifiers, nullptr);

    ASSERT_NE(type->u.function.return_type, nullptr);
    EXPECT_EQ(type->u.function.return_type->kind, TYPE_INT);
    ASSERT_NE(type->u.function.return_type->qualifiers, nullptr);
    EXPECT_EQ(type->u.function.return_type->qualifiers->kind, TYPE_QUALIFIER_CONST);
    EXPECT_EQ(type->u.function.return_type->qualifiers->next, nullptr);

    EXPECT_FALSE(type->u.function.variadic);

    ASSERT_NE(type->u.function.params, nullptr);
    const Param *p1 = type->u.function.params;
    ASSERT_NE(p1, nullptr);
    const Param *p2 = p1->next;
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p2->next, nullptr);

    EXPECT_EQ(p1->name, nullptr);
    ASSERT_NE(p1->type, nullptr);
    EXPECT_EQ(p1->type->kind, TYPE_DOUBLE);
    EXPECT_EQ(p1->type->qualifiers, nullptr);

    EXPECT_EQ(p2->name, nullptr);
    ASSERT_NE(p2->type, nullptr);
    EXPECT_EQ(p2->type->kind, TYPE_CHAR);
    EXPECT_EQ(p2->type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeIntParensPtrFuncInt)
{
    Type *type = TestType("int (*)(int)");

    EXPECT_EQ(type->kind, TYPE_POINTER);
    EXPECT_EQ(type->qualifiers, nullptr);

    ASSERT_NE(type->u.pointer.target, nullptr);
    EXPECT_EQ(type->u.pointer.qualifiers, nullptr);

    EXPECT_EQ(type->u.pointer.target->kind, TYPE_FUNCTION);
    EXPECT_EQ(type->u.pointer.target->qualifiers, nullptr);

    ASSERT_NE(type->u.pointer.target->u.function.return_type, nullptr);
    EXPECT_EQ(type->u.pointer.target->u.function.return_type->kind, TYPE_INT);
    EXPECT_EQ(type->u.pointer.target->u.function.return_type->qualifiers, nullptr);

    EXPECT_FALSE(type->u.pointer.target->u.function.variadic);

    ASSERT_NE(type->u.pointer.target->u.function.params, nullptr);
    const Param *p1 = type->u.pointer.target->u.function.params;
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->next, nullptr);

    EXPECT_EQ(p1->name, nullptr);
    ASSERT_NE(p1->type, nullptr);
    EXPECT_EQ(p1->type->kind, TYPE_INT);
    EXPECT_EQ(p1->type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeVoidParensPtrFuncCharPtr)
{
    Type *type = TestType("void (*)(char *)");

    EXPECT_EQ(type->kind, TYPE_POINTER);
    EXPECT_EQ(type->qualifiers, nullptr);

    ASSERT_NE(type->u.pointer.target, nullptr);
    EXPECT_EQ(type->u.pointer.qualifiers, nullptr);

    EXPECT_EQ(type->u.pointer.target->kind, TYPE_FUNCTION);
    EXPECT_EQ(type->u.pointer.target->qualifiers, nullptr);

    ASSERT_NE(type->u.pointer.target->u.function.return_type, nullptr);
    EXPECT_EQ(type->u.pointer.target->u.function.return_type->kind, TYPE_VOID);
    EXPECT_EQ(type->u.pointer.target->u.function.return_type->qualifiers, nullptr);

    EXPECT_FALSE(type->u.pointer.target->u.function.variadic);

    ASSERT_NE(type->u.pointer.target->u.function.params, nullptr);
    const Param *p1 = type->u.pointer.target->u.function.params;
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->next, nullptr);

    EXPECT_EQ(p1->name, nullptr);
    ASSERT_NE(p1->type, nullptr);
    EXPECT_EQ(p1->type->kind, TYPE_POINTER);
    ASSERT_NE(p1->type->u.pointer.target, nullptr);
    EXPECT_EQ(p1->type->qualifiers, nullptr);
    EXPECT_EQ(p1->type->u.pointer.target->kind, TYPE_CHAR);
    EXPECT_EQ(p1->type->u.pointer.target->qualifiers, nullptr);
    EXPECT_EQ(p1->type->u.pointer.qualifiers, nullptr);
    free_type(type);
}

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
TEST_F(ParserTest, TypeIntPtrArray)
{
    Type *type = TestType("int *[]");

    EXPECT_EQ(type->kind, TYPE_ARRAY);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.array.size, nullptr);
    EXPECT_EQ(type->u.array.qualifiers, nullptr);
    EXPECT_FALSE(type->u.array.is_static);

    Type *element = type->u.array.element;
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(element->kind, TYPE_POINTER);
    EXPECT_EQ(element->qualifiers, nullptr);
    ASSERT_NE(element->u.pointer.target, nullptr);
    EXPECT_EQ(element->u.pointer.target->kind, TYPE_INT);
    EXPECT_EQ(element->u.pointer.target->qualifiers, nullptr);
    EXPECT_EQ(element->u.pointer.qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeIntArrayArray5)
{
    Type *type = TestType("int [][5]");

    EXPECT_EQ(type->kind, TYPE_ARRAY);
    EXPECT_EQ(type->qualifiers, nullptr);

    ASSERT_NE(type->u.array.size, nullptr);
    EXPECT_EQ(type->u.array.size->kind, EXPR_LITERAL);
    EXPECT_EQ(type->u.array.size->u.literal->kind, LITERAL_INT);
    EXPECT_EQ(type->u.array.size->u.literal->u.int_val, 5);

    EXPECT_EQ(type->u.array.qualifiers, nullptr);
    EXPECT_FALSE(type->u.array.is_static);

    Type *element = type->u.array.element;
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(element->kind, TYPE_ARRAY);
    EXPECT_EQ(element->qualifiers, nullptr);
    EXPECT_EQ(element->u.array.size, nullptr);
    EXPECT_EQ(element->u.array.qualifiers, nullptr);
    EXPECT_FALSE(element->u.array.is_static);

    Type *item = element->u.array.element;
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->kind, TYPE_INT);
    EXPECT_EQ(item->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeCharPtrArray3)
{
    Type *type = TestType("char * [3]");

    EXPECT_EQ(type->kind, TYPE_ARRAY);
    EXPECT_EQ(type->qualifiers, nullptr);

    ASSERT_NE(type->u.array.size, nullptr);
    EXPECT_EQ(type->u.array.size->kind, EXPR_LITERAL);
    EXPECT_EQ(type->u.array.size->u.literal->kind, LITERAL_INT);
    EXPECT_EQ(type->u.array.size->u.literal->u.int_val, 3);

    EXPECT_EQ(type->u.array.qualifiers, nullptr);
    EXPECT_FALSE(type->u.array.is_static);

    Type *element = type->u.array.element;
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(element->kind, TYPE_POINTER);
    EXPECT_EQ(element->qualifiers, nullptr);
    ASSERT_NE(element->u.pointer.target, nullptr);
    EXPECT_EQ(element->u.pointer.target->kind, TYPE_CHAR);
    EXPECT_EQ(element->u.pointer.target->qualifiers, nullptr);
    EXPECT_EQ(element->u.pointer.qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeIntParensPtrArray5)
{
    Type *type = TestType("int (*)[5]");

    EXPECT_EQ(type->kind, TYPE_POINTER);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.qualifiers, nullptr);

    Type *target = type->u.pointer.target;
    ASSERT_NE(target, nullptr);

    EXPECT_EQ(target->kind, TYPE_ARRAY);
    EXPECT_EQ(target->qualifiers, nullptr);

    ASSERT_NE(target->u.array.size, nullptr);
    EXPECT_EQ(target->u.array.size->kind, EXPR_LITERAL);
    EXPECT_EQ(target->u.array.size->u.literal->kind, LITERAL_INT);
    EXPECT_EQ(target->u.array.size->u.literal->u.int_val, 5);

    EXPECT_EQ(target->u.array.qualifiers, nullptr);
    EXPECT_FALSE(target->u.array.is_static);

    Type *element = target->u.array.element;
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(element->kind, TYPE_INT);
    EXPECT_EQ(element->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeVoidParensPtrArrayFuncInt)
{
    Type *type = TestType("void (*[]) (int)");

    EXPECT_EQ(type->kind, TYPE_ARRAY);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.array.size, nullptr);
    EXPECT_EQ(type->u.array.qualifiers, nullptr);
    EXPECT_FALSE(type->u.array.is_static);

    Type *element = type->u.array.element;
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(element->kind, TYPE_POINTER);
    EXPECT_EQ(element->qualifiers, nullptr);

    ASSERT_NE(element->u.pointer.target, nullptr);
    EXPECT_EQ(element->u.pointer.qualifiers, nullptr);

    EXPECT_EQ(element->u.pointer.target->kind, TYPE_FUNCTION);
    EXPECT_EQ(element->u.pointer.target->qualifiers, nullptr);

    ASSERT_NE(element->u.pointer.target->u.function.return_type, nullptr);
    EXPECT_EQ(element->u.pointer.target->u.function.return_type->kind, TYPE_VOID);
    EXPECT_EQ(element->u.pointer.target->u.function.return_type->qualifiers, nullptr);

    EXPECT_FALSE(element->u.pointer.target->u.function.variadic);

    ASSERT_NE(element->u.pointer.target->u.function.params, nullptr);

    const Param *p1 = element->u.pointer.target->u.function.params;
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->next, nullptr);

    EXPECT_EQ(p1->name, nullptr);
    ASSERT_NE(p1->type, nullptr);
    EXPECT_EQ(p1->type->kind, TYPE_INT);
    EXPECT_EQ(p1->type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeIntParensPtrArray3FuncChar)
{
    Type *type = TestType("int (*[3])(char)");

    EXPECT_EQ(type->kind, TYPE_ARRAY);
    EXPECT_EQ(type->qualifiers, nullptr);

    ASSERT_NE(type->u.array.size, nullptr);
    EXPECT_EQ(type->u.array.size->kind, EXPR_LITERAL);
    EXPECT_EQ(type->u.array.size->u.literal->kind, LITERAL_INT);
    EXPECT_EQ(type->u.array.size->u.literal->u.int_val, 3);

    EXPECT_EQ(type->u.array.qualifiers, nullptr);
    EXPECT_FALSE(type->u.array.is_static);

    Type *element = type->u.array.element;
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(element->kind, TYPE_POINTER);
    EXPECT_EQ(element->qualifiers, nullptr);

    ASSERT_NE(element->u.pointer.target, nullptr);
    EXPECT_EQ(element->u.pointer.qualifiers, nullptr);

    EXPECT_EQ(element->u.pointer.target->kind, TYPE_FUNCTION);
    EXPECT_EQ(element->u.pointer.target->qualifiers, nullptr);

    ASSERT_NE(element->u.pointer.target->u.function.return_type, nullptr);
    EXPECT_EQ(element->u.pointer.target->u.function.return_type->kind, TYPE_INT);
    EXPECT_EQ(element->u.pointer.target->u.function.return_type->qualifiers, nullptr);

    EXPECT_FALSE(element->u.pointer.target->u.function.variadic);

    ASSERT_NE(element->u.pointer.target->u.function.params, nullptr);

    const Param *p1 = element->u.pointer.target->u.function.params;
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->next, nullptr);

    EXPECT_EQ(p1->name, nullptr);
    ASSERT_NE(p1->type, nullptr);
    EXPECT_EQ(p1->type->kind, TYPE_CHAR);
    EXPECT_EQ(p1->type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeIntPtrPtrArray5)
{
    Type *type = TestType("int **[5]");

    EXPECT_EQ(type->kind, TYPE_ARRAY);
    EXPECT_EQ(type->qualifiers, nullptr);

    ASSERT_NE(type->u.array.size, nullptr);
    EXPECT_EQ(type->u.array.size->kind, EXPR_LITERAL);
    EXPECT_EQ(type->u.array.size->u.literal->kind, LITERAL_INT);
    EXPECT_EQ(type->u.array.size->u.literal->u.int_val, 5);

    EXPECT_EQ(type->u.array.qualifiers, nullptr);
    EXPECT_FALSE(type->u.array.is_static);

    Type *element = type->u.array.element;
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(element->kind, TYPE_POINTER);
    EXPECT_EQ(element->qualifiers, nullptr);
    EXPECT_EQ(element->u.pointer.qualifiers, nullptr);

    Type *target = element->u.pointer.target;
    ASSERT_NE(target, nullptr);

    EXPECT_EQ(target->kind, TYPE_POINTER);
    EXPECT_EQ(target->qualifiers, nullptr);
    EXPECT_EQ(target->u.pointer.qualifiers, nullptr);

    EXPECT_EQ(target->u.pointer.target->kind, TYPE_INT);
    EXPECT_EQ(target->u.pointer.target->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeStructPtrArray10)
{
    Type *type = TestType("struct S *[10]");

    EXPECT_EQ(type->kind, TYPE_ARRAY);
    EXPECT_EQ(type->qualifiers, nullptr);

    ASSERT_NE(type->u.array.size, nullptr);
    EXPECT_EQ(type->u.array.size->kind, EXPR_LITERAL);
    EXPECT_EQ(type->u.array.size->u.literal->kind, LITERAL_INT);
    EXPECT_EQ(type->u.array.size->u.literal->u.int_val, 10);

    EXPECT_EQ(type->u.array.qualifiers, nullptr);
    EXPECT_FALSE(type->u.array.is_static);

    Type *element = type->u.array.element;
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(element->kind, TYPE_POINTER);
    EXPECT_EQ(element->qualifiers, nullptr);
    EXPECT_EQ(element->u.pointer.qualifiers, nullptr);

    Type *target = element->u.pointer.target;
    ASSERT_NE(target, nullptr);
    EXPECT_EQ(target->kind, TYPE_STRUCT);
    EXPECT_STREQ(target->u.struct_t.name, "S");
    EXPECT_EQ(target->u.struct_t.fields, nullptr);
    EXPECT_EQ(target->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeIntParensPtrParensPtrArray5FuncInt)
{
    Type *type = TestType("int (*(*)[5])(int)");

    EXPECT_EQ(type->kind, TYPE_POINTER);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.qualifiers, nullptr);

    Type *target = type->u.pointer.target;
    ASSERT_NE(target, nullptr);

    EXPECT_EQ(target->kind, TYPE_ARRAY);
    EXPECT_EQ(target->qualifiers, nullptr);

    ASSERT_NE(target->u.array.size, nullptr);
    EXPECT_EQ(target->u.array.size->kind, EXPR_LITERAL);
    EXPECT_EQ(target->u.array.size->u.literal->kind, LITERAL_INT);
    EXPECT_EQ(target->u.array.size->u.literal->u.int_val, 5);

    EXPECT_EQ(target->u.array.qualifiers, nullptr);
    EXPECT_FALSE(target->u.array.is_static);

    Type *element = target->u.array.element;
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(element->kind, TYPE_POINTER);
    EXPECT_EQ(element->qualifiers, nullptr);

    ASSERT_NE(element->u.pointer.target, nullptr);
    EXPECT_EQ(element->u.pointer.qualifiers, nullptr);

    EXPECT_EQ(element->u.pointer.target->kind, TYPE_FUNCTION);
    EXPECT_EQ(element->u.pointer.target->qualifiers, nullptr);

    ASSERT_NE(element->u.pointer.target->u.function.return_type, nullptr);
    EXPECT_EQ(element->u.pointer.target->u.function.return_type->kind, TYPE_INT);
    EXPECT_EQ(element->u.pointer.target->u.function.return_type->qualifiers, nullptr);

    EXPECT_FALSE(element->u.pointer.target->u.function.variadic);

    ASSERT_NE(element->u.pointer.target->u.function.params, nullptr);

    const Param *p1 = element->u.pointer.target->u.function.params;
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->next, nullptr);

    EXPECT_EQ(p1->name, nullptr);
    ASSERT_NE(p1->type, nullptr);
    EXPECT_EQ(p1->type->kind, TYPE_INT);
    EXPECT_EQ(p1->type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeConstIntPtrConstArray3)
{
    Type *type = TestType("const int * const [3]");

    EXPECT_EQ(type->kind, TYPE_ARRAY);
    EXPECT_EQ(type->qualifiers, nullptr);

    ASSERT_NE(type->u.array.size, nullptr);
    EXPECT_EQ(type->u.array.size->kind, EXPR_LITERAL);
    EXPECT_EQ(type->u.array.size->u.literal->kind, LITERAL_INT);
    EXPECT_EQ(type->u.array.size->u.literal->u.int_val, 3);

    EXPECT_EQ(type->u.array.qualifiers, nullptr);
    EXPECT_FALSE(type->u.array.is_static);

    Type *element = type->u.array.element;
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(element->kind, TYPE_POINTER);
    EXPECT_EQ(element->qualifiers, nullptr);

    ASSERT_NE(element->u.pointer.qualifiers, nullptr);
    EXPECT_EQ(element->u.pointer.qualifiers->kind, TYPE_QUALIFIER_CONST);
    EXPECT_EQ(element->u.pointer.qualifiers->next, nullptr);

    Type *target = element->u.pointer.target;
    ASSERT_NE(target, nullptr);
    EXPECT_EQ(target->kind, TYPE_INT);

    ASSERT_NE(target->qualifiers, nullptr);
    EXPECT_EQ(target->qualifiers->kind, TYPE_QUALIFIER_CONST);
    EXPECT_EQ(target->qualifiers->next, nullptr);
    free_type(type);
}

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
TEST_F(ParserTest, TypeIntParensPtr)
{
    Type *type = TestType("int (*)");

    EXPECT_EQ(type->kind, TYPE_POINTER);
    ASSERT_NE(type->u.pointer.target, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.target->kind, TYPE_INT);
    EXPECT_EQ(type->u.pointer.target->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeConstCharParensPtrConst)
{
    Type *type = TestType("const char (* const)");

    EXPECT_EQ(type->kind, TYPE_POINTER);
    ASSERT_NE(type->u.pointer.target, nullptr);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.target->kind, TYPE_CHAR);

    ASSERT_NE(type->u.pointer.target->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.target->qualifiers->kind, TYPE_QUALIFIER_CONST);
    EXPECT_EQ(type->u.pointer.target->qualifiers->next, nullptr);

    ASSERT_NE(type->u.pointer.qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.qualifiers->kind, TYPE_QUALIFIER_CONST);
    EXPECT_EQ(type->u.pointer.qualifiers->next, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeStructParensPtrArray3)
{
    Type *type = TestType("struct S (*[3])");

    EXPECT_EQ(type->kind, TYPE_ARRAY);
    EXPECT_EQ(type->qualifiers, nullptr);

    ASSERT_NE(type->u.array.size, nullptr);
    EXPECT_EQ(type->u.array.size->kind, EXPR_LITERAL);
    EXPECT_EQ(type->u.array.size->u.literal->kind, LITERAL_INT);
    EXPECT_EQ(type->u.array.size->u.literal->u.int_val, 3);

    EXPECT_EQ(type->u.array.qualifiers, nullptr);
    EXPECT_FALSE(type->u.array.is_static);

    Type *element = type->u.array.element;
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(element->kind, TYPE_POINTER);
    ASSERT_NE(element->u.pointer.target, nullptr);
    EXPECT_EQ(element->qualifiers, nullptr);
    EXPECT_EQ(element->u.pointer.target->kind, TYPE_STRUCT);
    EXPECT_STREQ(element->u.pointer.target->u.struct_t.name, "S");
    EXPECT_EQ(element->u.pointer.target->u.struct_t.fields, nullptr);
    EXPECT_EQ(element->u.pointer.target->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeVoidParensPtrParensPtrFuncInt)
{
    Type *type = TestType("void (*(*)(int))");

    EXPECT_EQ(type->kind, TYPE_POINTER);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.qualifiers, nullptr);

    Type *target = type->u.pointer.target;
    ASSERT_NE(target, nullptr);

    EXPECT_EQ(target->kind, TYPE_FUNCTION);
    EXPECT_EQ(target->qualifiers, nullptr);

    ASSERT_NE(target->u.function.return_type, nullptr);

    EXPECT_EQ(target->u.function.return_type->kind, TYPE_POINTER);
    EXPECT_EQ(target->u.function.return_type->qualifiers, nullptr);
    EXPECT_EQ(target->u.function.return_type->u.pointer.qualifiers, nullptr);

    ASSERT_NE(target->u.function.return_type->u.pointer.target, nullptr);
    EXPECT_EQ(target->u.function.return_type->u.pointer.target->kind, TYPE_VOID);
    EXPECT_EQ(target->u.function.return_type->u.pointer.target->qualifiers, nullptr);

    EXPECT_FALSE(target->u.function.variadic);

    ASSERT_NE(target->u.function.params, nullptr);

    const Param *p1 = target->u.function.params;
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->next, nullptr);

    EXPECT_EQ(p1->name, nullptr);
    ASSERT_NE(p1->type, nullptr);
    EXPECT_EQ(p1->type->kind, TYPE_INT);
    EXPECT_EQ(p1->type->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeUnsignedIntParensPtrParensPtrArray5)
{
    Type *type = TestType("unsigned int (*(*)[5])");

    EXPECT_EQ(type->kind, TYPE_POINTER);
    EXPECT_EQ(type->qualifiers, nullptr);
    EXPECT_EQ(type->u.pointer.qualifiers, nullptr);

    Type *target = type->u.pointer.target;
    ASSERT_NE(target, nullptr);

    EXPECT_EQ(target->kind, TYPE_ARRAY);
    EXPECT_EQ(target->qualifiers, nullptr);

    ASSERT_NE(target->u.array.size, nullptr);
    EXPECT_EQ(target->u.array.size->kind, EXPR_LITERAL);
    EXPECT_EQ(target->u.array.size->u.literal->kind, LITERAL_INT);
    EXPECT_EQ(target->u.array.size->u.literal->u.int_val, 5);

    EXPECT_EQ(target->u.array.qualifiers, nullptr);
    EXPECT_FALSE(target->u.array.is_static);

    Type *element = target->u.array.element;
    ASSERT_NE(element, nullptr);

    EXPECT_EQ(element->kind, TYPE_POINTER);
    EXPECT_EQ(element->qualifiers, nullptr);
    EXPECT_EQ(element->u.pointer.qualifiers, nullptr);

    ASSERT_NE(element->u.pointer.target, nullptr);
    EXPECT_EQ(element->u.pointer.target->kind, TYPE_UINT);
    EXPECT_EQ(element->u.pointer.target->qualifiers, nullptr);
    free_type(type);
}

TEST_F(ParserTest, TypeConstStructParensPtrFuncInt)
{
    Type *type = TestType("const struct S (*(int))");

    EXPECT_EQ(type->kind, TYPE_FUNCTION);
    EXPECT_EQ(type->qualifiers, nullptr);

    ASSERT_NE(type->u.function.return_type, nullptr);
    EXPECT_EQ(type->u.function.return_type->kind, TYPE_POINTER);
    EXPECT_EQ(type->u.function.return_type->qualifiers, nullptr);
    EXPECT_EQ(type->u.function.return_type->u.pointer.qualifiers, nullptr);

    ASSERT_NE(type->u.function.return_type->u.pointer.target, nullptr);
    EXPECT_EQ(type->u.function.return_type->u.pointer.target->kind, TYPE_STRUCT);
    EXPECT_STREQ(type->u.function.return_type->u.pointer.target->u.struct_t.name, "S");
    EXPECT_EQ(type->u.function.return_type->u.pointer.target->u.struct_t.fields, nullptr);

    ASSERT_NE(type->u.function.return_type->u.pointer.target->qualifiers, nullptr);
    EXPECT_EQ(type->u.function.return_type->u.pointer.target->qualifiers->kind, TYPE_QUALIFIER_CONST);
    EXPECT_EQ(type->u.function.return_type->u.pointer.target->qualifiers->next, nullptr);

    EXPECT_FALSE(type->u.function.variadic);

    ASSERT_NE(type->u.function.params, nullptr);
    const Param *p1 = type->u.function.params;
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->next, nullptr);

    EXPECT_EQ(p1->name, nullptr);
    ASSERT_NE(p1->type, nullptr);
    EXPECT_EQ(p1->type->kind, TYPE_INT);
    EXPECT_EQ(p1->type->qualifiers, nullptr);
    free_type(type);
}
