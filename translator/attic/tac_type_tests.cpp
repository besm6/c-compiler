#include <gtest/gtest.h>

#include <cctype>
#include <cstring>
#include <string>

#include "tac.h"

// External parser functions (from tac_type_parser.y)
extern "C" int yyparse(void);
extern "C" void yyerror(const char *s);

// External TAC functions
extern "C" Tac_Type *new_tac_type(Tac_TypeKind kind);
extern "C" bool compare_tac_type(const Tac_Type *a, const Tac_Type *b);
extern "C" void free_tac_type(Tac_Type *type);

// Global for parsed result
Tac_Type *parsed_type = nullptr;

// Scanner state
static std::string input_buffer;
static size_t input_pos = 0;

// yylval union (must match parser)
union yylval_type {
    Tac_Type *type;
    char *str;
    int num;
};
extern union yylval_type yylval;

// Custom yylex for testing
extern "C" int yylex(void)
{
    while (input_pos < input_buffer.size() && std::isspace(input_buffer[input_pos])) {
        input_pos++;
    }
    if (input_pos >= input_buffer.size())
        return 0;

    const char *input = input_buffer.c_str();
    size_t len        = input_buffer.size();

    // Keywords
    if (strncmp(&input[input_pos], "char", 4) == 0 &&
        (input_pos + 4 >= len || !std::isalnum(input[input_pos + 4]))) {
        input_pos += 4;
        return 258; // TOKEN_CHAR
    }
    if (strncmp(&input[input_pos], "signed", 6) == 0 &&
        (input_pos + 6 >= len || !std::isalnum(input[input_pos + 6]))) {
        input_pos += 6;
        return 259; // TOKEN_SIGNED
    }
    if (strncmp(&input[input_pos], "unsigned", 8) == 0 &&
        (input_pos + 8 >= len || !std::isalnum(input[input_pos + 8]))) {
        input_pos += 8;
        return 260; // TOKEN_UNSIGNED
    }
    if (strncmp(&input[input_pos], "short", 5) == 0 &&
        (input_pos + 5 >= len || !std::isalnum(input[input_pos + 5]))) {
        input_pos += 5;
        return 261; // TOKEN_SHORT
    }
    if (strncmp(&input[input_pos], "int", 3) == 0 &&
        (input_pos + 3 >= len || !std::isalnum(input[input_pos + 3]))) {
        input_pos += 3;
        return 262; // TOKEN_INT
    }
    if (strncmp(&input[input_pos], "long", 4) == 0 &&
        (input_pos + 4 >= len || !std::isalnum(input[input_pos + 4]))) {
        input_pos += 4;
        return 263; // TOKEN_LONG
    }
    if (strncmp(&input[input_pos], "float", 5) == 0 &&
        (input_pos + 5 >= len || !std::isalnum(input[input_pos + 5]))) {
        input_pos += 5;
        return 264; // TOKEN_FLOAT
    }
    if (strncmp(&input[input_pos], "double", 6) == 0 &&
        (input_pos + 6 >= len || !std::isalnum(input[input_pos + 6]))) {
        input_pos += 6;
        return 265; // TOKEN_DOUBLE
    }
    if (strncmp(&input[input_pos], "void", 4) == 0 &&
        (input_pos + 4 >= len || !std::isalnum(input[input_pos + 4]))) {
        input_pos += 4;
        return 266; // TOKEN_VOID
    }
    if (strncmp(&input[input_pos], "fun", 3) == 0 &&
        (input_pos + 3 >= len || !std::isalnum(input[input_pos + 3]))) {
        input_pos += 3;
        return 267; // TOKEN_FUN
    }
    if (strncmp(&input[input_pos], "struct", 6) == 0 &&
        (input_pos + 6 >= len || !std::isalnum(input[input_pos + 6]))) {
        input_pos += 6;
        return 268; // TOKEN_STRUCT
    }

    // Symbols
    if (input[input_pos] == '*') {
        input_pos++;
        return 271; // TOKEN_STAR
    }
    if (strncmp(&input[input_pos], "->", 2) == 0) {
        input_pos += 2;
        return 272; // TOKEN_ARROW
    }
    if (input[input_pos] == '(') {
        input_pos++;
        return 273; // TOKEN_LPAREN
    }
    if (input[input_pos] == ')') {
        input_pos++;
        return 274; // TOKEN_RPAREN
    }
    if (input[input_pos] == '[') {
        input_pos++;
        return 275; // TOKEN_LBRACKET
    }
    if (input[input_pos] == ']') {
        input_pos++;
        return 276; // TOKEN_RBRACKET
    }
    if (input[input_pos] == ',') {
        input_pos++;
        return 277; // TOKEN_COMMA
    }

    // Identifier
    if (std::isalpha(input[input_pos]) || input[input_pos] == '_') {
        size_t start = input_pos;
        while (input_pos < len && (std::isalnum(input[input_pos]) || input[input_pos] == '_')) {
            input_pos++;
        }
        yylval.str = strndup(&input[start], input_pos - start);
        return 269; // TOKEN_IDENT
    }

    // Number
    if (std::isdigit(input[input_pos])) {
        size_t start = input_pos;
        while (input_pos < len && std::isdigit(input[input_pos])) {
            input_pos++;
        }
        char *num_str = strndup(&input[start], input_pos - start);
        yylval.num    = atoi(num_str);
        free(num_str);
        return 270; // TOKEN_NUMBER
    }

    input_pos++;
    return -1; // Unknown token
}

// Capture yyerror messages
static std::string error_message;
extern "C" void yyerror(const char *s)
{
    error_message = s;
}

// Test fixture
class TacTypeParserTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        parsed_type = nullptr;
        input_buffer.clear();
        input_pos = 0;
        error_message.clear();
    }

    void TearDown() override
    {
        if (parsed_type) {
            free_tac_type(parsed_type);
            parsed_type = nullptr;
        }
        input_buffer.clear();
        input_pos = 0;
        error_message.clear();
    }

    // Helper to set input and parse
    bool Parse(const std::string &input)
    {
        input_buffer = input;
        input_pos    = 0;
        error_message.clear();
        return yyparse() == 0;
    }

    // Helper to create a basic type
    Tac_Type *CreateBasicType(Tac_TypeKind kind) { return new_tac_type(kind); }

    // Helper to create a pointer type
    Tac_Type *CreatePointerType(Tac_Type *referenced)
    {
        Tac_Type *type             = new_tac_type(TAC_TYPE_POINTER);
        type->u.pointer.referenced = referenced;
        return type;
    }

    // Helper to create an array type
    Tac_Type *CreateArrayType(Tac_Type *element, int size)
    {
        Tac_Type *type        = new_tac_type(TAC_TYPE_ARRAY);
        type->u.array.element = element;
        type->u.array.size    = size;
        return type;
    }

    // Helper to create a structure type
    Tac_Type *CreateStructType(const char *tag)
    {
        Tac_Type *type        = new_tac_type(TAC_TYPE_STRUCTURE);
        type->u.structure.tag = strdup(tag);
        return type;
    }

    // Helper to create a function type
    Tac_Type *CreateFunType(Tac_Type *params, Tac_Type *ret)
    {
        Tac_Type *type          = new_tac_type(TAC_TYPE_FUN_TYPE);
        type->u.fun_type.params = params;
        type->u.fun_type.ret    = ret;
        return type;
    }
};

// Test basic types
TEST_F(TacTypeParserTest, BasicTypeInt)
{
    EXPECT_TRUE(Parse("int"));
    Tac_Type *expected = CreateBasicType(TAC_TYPE_INT);
    EXPECT_TRUE(compare_tac_type(parsed_type, expected));
    free_tac_type(expected);
}

TEST_F(TacTypeParserTest, BasicTypeUnsignedChar)
{
    EXPECT_TRUE(Parse("unsigned char"));
    Tac_Type *expected = CreateBasicType(TAC_TYPE_UCHAR);
    EXPECT_TRUE(compare_tac_type(parsed_type, expected));
    free_tac_type(expected);
}

TEST_F(TacTypeParserTest, BasicTypeLongLong)
{
    EXPECT_TRUE(Parse("long long"));
    Tac_Type *expected = CreateBasicType(TAC_TYPE_LONG_LONG);
    EXPECT_TRUE(compare_tac_type(parsed_type, expected));
    free_tac_type(expected);
}

TEST_F(TacTypeParserTest, BasicTypeVoid)
{
    EXPECT_TRUE(Parse("void"));
    Tac_Type *expected = CreateBasicType(TAC_TYPE_VOID);
    EXPECT_TRUE(compare_tac_type(parsed_type, expected));
    free_tac_type(expected);
}

// Test pointer types
TEST_F(TacTypeParserTest, PointerToInt)
{
    EXPECT_TRUE(Parse("*int"));
    Tac_Type *expected = CreatePointerType(CreateBasicType(TAC_TYPE_INT));
    EXPECT_TRUE(compare_tac_type(parsed_type, expected));
    free_tac_type(expected);
}

TEST_F(TacTypeParserTest, DoublePointerToDouble)
{
    EXPECT_TRUE(Parse("**double"));
    Tac_Type *expected = CreatePointerType(CreatePointerType(CreateBasicType(TAC_TYPE_DOUBLE)));
    EXPECT_TRUE(compare_tac_type(parsed_type, expected));
    free_tac_type(expected);
}

// Test array types
TEST_F(TacTypeParserTest, ArrayOfInt)
{
    EXPECT_TRUE(Parse("int[5]"));
    Tac_Type *expected = CreateArrayType(CreateBasicType(TAC_TYPE_INT), 5);
    EXPECT_TRUE(compare_tac_type(parsed_type, expected));
    free_tac_type(expected);
}

TEST_F(TacTypeParserTest, ArrayOfPointerToChar)
{
    EXPECT_TRUE(Parse("*char[10]"));
    Tac_Type *expected = CreateArrayType(CreatePointerType(CreateBasicType(TAC_TYPE_CHAR)), 10);
    EXPECT_TRUE(compare_tac_type(parsed_type, expected));
    free_tac_type(expected);
}

TEST_F(TacTypeParserTest, ZeroSizedArray)
{
    EXPECT_TRUE(Parse("float[0]"));
    Tac_Type *expected = CreateArrayType(CreateBasicType(TAC_TYPE_FLOAT), 0);
    EXPECT_TRUE(compare_tac_type(parsed_type, expected));
    free_tac_type(expected);
}

// Test structure types
TEST_F(TacTypeParserTest, StructType)
{
    EXPECT_TRUE(Parse("struct mystruct"));
    Tac_Type *expected = CreateStructType("mystruct");
    EXPECT_TRUE(compare_tac_type(parsed_type, expected));
    free_tac_type(expected);
}

// Test function types
TEST_F(TacTypeParserTest, FunctionNoParams)
{
    EXPECT_TRUE(Parse("fun() -> void"));
    Tac_Type *expected = CreateFunType(nullptr, CreateBasicType(TAC_TYPE_VOID));
    EXPECT_TRUE(compare_tac_type(parsed_type, expected));
    free_tac_type(expected);
}

TEST_F(TacTypeParserTest, FunctionSingleParam)
{
    EXPECT_TRUE(Parse("fun(int) -> double"));
    Tac_Type *expected =
        CreateFunType(CreateBasicType(TAC_TYPE_INT), CreateBasicType(TAC_TYPE_DOUBLE));
    EXPECT_TRUE(compare_tac_type(parsed_type, expected));
    free_tac_type(expected);
}

TEST_F(TacTypeParserTest, FunctionMultipleParams)
{
    EXPECT_TRUE(Parse("fun(char, *int, double[3]) -> *void"));
    Tac_Type *param1   = CreateBasicType(TAC_TYPE_CHAR);
    Tac_Type *param2   = CreatePointerType(CreateBasicType(TAC_TYPE_INT));
    Tac_Type *param3   = CreateArrayType(CreateBasicType(TAC_TYPE_DOUBLE), 3);
    param1->next       = param2;
    param2->next       = param3;
    Tac_Type *expected = CreateFunType(param1, CreatePointerType(CreateBasicType(TAC_TYPE_VOID)));
    EXPECT_TRUE(Parse("fun(char, *int, double[3]) -> *void"));
    EXPECT_TRUE(compare_tac_type(parsed_type, expected));
    free_tac_type(expected);
}

// Test invalid inputs
TEST_F(TacTypeParserTest, InvalidType)
{
    EXPECT_FALSE(Parse("unknown"));
    EXPECT_TRUE(error_message.find("Error") != std::string::npos);
}

TEST_F(TacTypeParserTest, IncompleteFunction)
{
    EXPECT_FALSE(Parse("fun(int"));
    EXPECT_TRUE(error_message.find("Error") != std::string::npos);
}

TEST_F(TacTypeParserTest, MissingArraySize)
{
    EXPECT_FALSE(Parse("int[]"));
    EXPECT_TRUE(error_message.find("Error") != std::string::npos);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
