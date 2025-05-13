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
    free_type(type);
}

TEST_F(ParserTest, TypeChar)
{
    Type *type = TestType("char");

    EXPECT_EQ(type->kind, TYPE_CHAR);
    free_type(type);
}

TEST_F(ParserTest, TypeInt)
{
    Type *type = TestType("int");

    EXPECT_EQ(type->kind, TYPE_INT);
    free_type(type);
}

//TODO:
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

//
// 5. Atomic Type Specifier
// Testing "_Atomic" with a type name.
//
// 36. _Atomic(int)
// 37. _Atomic(struct S)
// 38. _Atomic(const char)
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
