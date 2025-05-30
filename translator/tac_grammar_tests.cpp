#include <gtest/gtest.h>

#include <cctype>
#include <cstring>
#include <string>

#include "tac.h"

// External parser functions (from tac_parser.y)
extern "C" int yyparse(void);
extern "C" void yyerror(const char *s);
extern "C" Tac_Program *parsed_program;

// External TAC functions
extern "C" Tac_Program *new_tac_program(void);
extern "C" Tac_Type *new_tac_type(Tac_TypeKind kind);
extern "C" Tac_Instruction *new_tac_instruction(Tac_InstructionKind kind);
extern "C" Tac_Val *new_tac_val(Tac_ValKind kind);
extern "C" Tac_Const *new_tac_const(Tac_ConstKind kind);
extern "C" Tac_Param *new_tac_param(void);
extern "C" Tac_TopLevel *new_tac_toplevel(Tac_TopLevelKind kind);
extern "C" Tac_StaticInit *new_tac_static_init(Tac_StaticInitKind kind);
extern "C" bool compare_tac_program(const Tac_Program *a, const Tac_Program *b);
extern "C" bool compare_tac_toplevel(const Tac_TopLevel *a, const Tac_TopLevel *b);
extern "C" bool compare_tac_param(const Tac_Param *a, const Tac_Param *b);
extern "C" bool compare_tac_static_init(const Tac_StaticInit *a, const Tac_StaticInit *b);
extern "C" bool compare_tac_type(const Tac_Type *a, const Tac_Type *b);
extern "C" bool compare_tac_instruction(const Tac_Instruction *a, const Tac_Instruction *b);
extern "C" bool compare_tac_val(const Tac_Val *a, const Tac_Val *b);
extern "C" bool compare_tac_const(const Tac_Const *a, const Tac_Const *b);
extern "C" void free_tac_type(Tac_Type *type);
extern "C" void free_tac_instruction(Tac_Instruction *instr);
extern "C" void free_tac_val(Tac_Val *val);
extern "C" void free_tac_const(Tac_Const *constant);
extern "C" void free_tac_param(Tac_Param *param);
extern "C" void free_tac_static_init(Tac_StaticInit *init);
extern "C" void free_tac_toplevel(Tac_TopLevel *toplevel);

// Define free_tac_program if not provided
extern "C" void free_tac_program(Tac_Program *prog)
{
    if (!prog)
        return;
    free_tac_toplevel(prog->decls);
    free(prog);
}

// Scanner state
static std::string input_buffer;
static size_t input_pos = 0;

// yylval union (must match parser)
union yylval_type {
    Tac_Program *program;
    Tac_TopLevel *toplevel;
    Tac_Param *param;
    Tac_StaticInit *init;
    Tac_Type *type;
    Tac_Instruction *instr;
    Tac_Val *val;
    char *str;
    int num;
    double fval;
    Tac_UnaryOperator unary_op;
    Tac_BinaryOperator binary_op;
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
#define MATCH_KEYWORD(kw, tok, len)                                          \
    if (strncmp(&input[input_pos], kw, len) == 0 &&                          \
        (input_pos + len >= len || !std::isalnum(input[input_pos + len]))) { \
        input_pos += len;                                                    \
        return tok;                                                          \
    }
    MATCH_KEYWORD("char", 258, 4)
    MATCH_KEYWORD("signed", 259, 6)
    MATCH_KEYWORD("unsigned", 260, 8)
    MATCH_KEYWORD("short", 261, 5)
    MATCH_KEYWORD("int", 262, 3)
    MATCH_KEYWORD("long", 263, 4)
    MATCH_KEYWORD("float", 264, 5)
    MATCH_KEYWORD("double", 265, 6)
    MATCH_KEYWORD("void", 266, 4)
    MATCH_KEYWORD("fun", 267, 3)
    MATCH_KEYWORD("struct", 268, 6)
    MATCH_KEYWORD("function", 269, 8)
    MATCH_KEYWORD("global", 270, 6)
    MATCH_KEYWORD("static", 271, 6)
    MATCH_KEYWORD("const", 272, 5)
    MATCH_KEYWORD("zero", 273, 4)
    MATCH_KEYWORD("return", 274, 6)
    MATCH_KEYWORD("sign_extend", 275, 11)
    MATCH_KEYWORD("truncate", 276, 8)
    MATCH_KEYWORD("zero_extend", 277, 11)
    MATCH_KEYWORD("double_to_int", 278, 12)
    MATCH_KEYWORD("double_to_uint", 279, 13)
    MATCH_KEYWORD("int_to_double", 280, 12)
    MATCH_KEYWORD("uint_to_double", 281, 13)
    MATCH_KEYWORD("unary", 282, 5)
    MATCH_KEYWORD("binary", 283, 6)
    MATCH_KEYWORD("copy", 284, 4)
    MATCH_KEYWORD("get_address", 285, 11)
    MATCH_KEYWORD("load", 286, 4)
    MATCH_KEYWORD("store", 287, 5)
    MATCH_KEYWORD("add_ptr", 288, 7)
    MATCH_KEYWORD("copy_to_offset", 289, 14)
    MATCH_KEYWORD("copy_from_offset", 290, 16)
    MATCH_KEYWORD("jump", 291, 4)
    MATCH_KEYWORD("jump_if_zero", 292, 12)
    MATCH_KEYWORD("jump_if_not_zero", 293, 16)
    MATCH_KEYWORD("label", 294, 5)
    MATCH_KEYWORD("fun_call", 295, 8)
    MATCH_KEYWORD("complement", 296, 10)
    MATCH_KEYWORD("negate", 297, 6)
    MATCH_KEYWORD("not", 298, 3)
    MATCH_KEYWORD("add", 299, 3)
    MATCH_KEYWORD("subtract", 300, 8)
    MATCH_KEYWORD("multiply", 301, 8)
    MATCH_KEYWORD("divide", 302, 6)
    MATCH_KEYWORD("remainder", 303, 9)
    MATCH_KEYWORD("equal", 304, 5)
    MATCH_KEYWORD("not_equal", 305, 9)
    MATCH_KEYWORD("less_than", 306, 9)
    MATCH_KEYWORD("less_or_equal", 307, 13)
    MATCH_KEYWORD("greater_than", 308, 12)
    MATCH_KEYWORD("greater_or_equal", 309, 16)
    MATCH_KEYWORD("bitwise_and", 310, 11)
    MATCH_KEYWORD("bitwise_or", 311, 10)
    MATCH_KEYWORD("bitwise_xor", 312, 11)
    MATCH_KEYWORD("left_shift", 313, 10)
    MATCH_KEYWORD("right_shift", 314, 11)

    // Symbols
    if (input[input_pos] == '*') {
        input_pos++;
        return 315;
    } // STAR
    if (strncmp(&input[input_pos], "->", 2) == 0) {
        input_pos += 2;
        return 316;
    } // ARROW
    if (input[input_pos] == '(') {
        input_pos++;
        return 317;
    } // LPAREN
    if (input[input_pos] == ')') {
        input_pos++;
        return 318;
    } // RPAREN
    if (input[input_pos] == '[') {
        input_pos++;
        return 319;
    } // LBRACKET
    if (input[input_pos] == ']') {
        input_pos++;
        return 320;
    } // RBRACKET
    if (input[input_pos] == '{') {
        input_pos++;
        return 321;
    } // LBRACE
    if (input[input_pos] == '}') {
        input_pos++;
        return 322;
    } // RBRACE
    if (input[input_pos] == ',') {
        input_pos++;
        return 323;
    } // COMMA
    if (input[input_pos] == ';') {
        input_pos++;
        return 324;
    } // SEMICOLON
    if (input[input_pos] == '=') {
        input_pos++;
        return 325;
    } // EQUAL
    if (input[input_pos] == '&') {
        input_pos++;
        return 326;
    } // AMPERSAND

    // String literal
    if (input[input_pos] == '"') {
        input_pos++;
        size_t start = input_pos;
        while (input_pos < len && input[input_pos] != '"') {
            input_pos++;
        }
        if (input_pos < len) {
            yylval.str = strndup(&input[start], input_pos - start);
            input_pos++;
            return 327; // STRING
        }
        return -1;
    }

    // Character literal
    if (input[input_pos] == '\'') {
        input_pos++;
        if (input_pos < len) {
            yylval.num = input[input_pos];
            input_pos++;
            if (input_pos < len && input[input_pos] == '\'') {
                input_pos++;
                return 328; // CHAR_LITERAL
            }
        }
        return -1;
    }

    // Identifier
    if (std::isalpha(input[input_pos]) || input[input_pos] == '_') {
        size_t start = input_pos;
        while (input_pos < len && (std::isalnum(input[input_pos]) || input[input_pos] == '_')) {
            input_pos++;
        }
        yylval.str = strndup(&input[start], input_pos - start);
        return 329; // IDENT
    }

    // Number or Float
    if (std::isdigit(input[input_pos]) || input[input_pos] == '.') {
        size_t start  = input_pos;
        bool is_float = false;
        while (input_pos < len && (std::isdigit(input[input_pos]) || input[input_pos] == '.')) {
            if (input[input_pos] == '.')
                is_float = true;
            input_pos++;
        }
        char *num_str = strndup(&input[start], input_pos - start);
        if (is_float) {
            yylval.fval = atof(num_str);
            free(num_str);
            return 330; // FLOAT
        } else {
            yylval.num = atoi(num_str);
            free(num_str);
            return 331; // NUMBER
        }
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
class TacParserTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        parsed_program = nullptr;
        input_buffer.clear();
        input_pos = 0;
        error_message.clear();
    }

    void TearDown() override
    {
        if (parsed_program) {
            free_tac_program(parsed_program);
            parsed_program = nullptr;
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

    // Helper to create a function type
    Tac_Type *CreateFunType(Tac_Type *params, Tac_Type *ret)
    {
        Tac_Type *type          = new_tac_type(TAC_TYPE_FUN_TYPE);
        type->u.fun_type.params = params;
        type->u.fun_type.ret    = ret;
        return type;
    }

    // Helper to create a structure type
    Tac_Type *CreateStructType(const char *tag)
    {
        Tac_Type *type        = new_tac_type(TAC_TYPE_STRUCTURE);
        type->u.structure.tag = strdup(tag);
        return type;
    }

    // Helper to create a variable value
    Tac_Val *CreateVar(const char *name)
    {
        Tac_Val *val    = new_tac_val(TAC_VAL_VAR);
        val->u.var_name = strdup(name);
        return val;
    }

    // Helper to create an integer constant value
    Tac_Val *CreateIntConst(int val)
    {
        Tac_Const *c  = new_tac_const(TAC_CONST_INT);
        c->u.int_val  = val;
        Tac_Val *v    = new_tac_val(TAC_VAL_CONSTANT);
        v->u.constant = c;
        return v;
    }

    // Helper to create a double constant value
    Tac_Val *CreateDoubleConst(double val)
    {
        Tac_Const *c    = new_tac_const(TAC_CONST_DOUBLE);
        c->u.double_val = val;
        Tac_Val *v      = new_tac_val(TAC_VAL_CONSTANT);
        v->u.constant   = c;
        return v;
    }

    // Helper to create an instruction
    Tac_Instruction *CreateInstruction(Tac_InstructionKind kind)
    {
        return new_tac_instruction(kind);
    }

    // Helper to create a parameter
    Tac_Param *CreateParam(const char *name)
    {
        Tac_Param *param = new_tac_param();
        param->name      = strdup(name);
        return param;
    }

    // Helper to create a static initializer
    Tac_StaticInit *CreateStaticInitInt(int val)
    {
        Tac_StaticInit *init = new_tac_static_init(TAC_STATIC_INIT_INT);
        init->u.int_val      = val;
        return init;
    }

    Tac_StaticInit *CreateStaticInitDouble(double val)
    {
        Tac_StaticInit *init = new_tac_static_init(TAC_STATIC_INIT_DOUBLE);
        init->u.double_val   = val;
        return init;
    }

    Tac_StaticInit *CreateStaticInitChar(int val)
    {
        Tac_StaticInit *init = new_tac_static_init(TAC_STATIC_INIT_CHAR);
        init->u.char_val     = val;
        return init;
    }

    Tac_StaticInit *CreateStaticInitZero(int bytes)
    {
        Tac_StaticInit *init = new_tac_static_init(TAC_STATIC_INIT_ZERO);
        init->u.zero_bytes   = bytes;
        return init;
    }

    Tac_StaticInit *CreateStaticInitString(const char *val, bool null_term)
    {
        Tac_StaticInit *init           = new_tac_static_init(TAC_STATIC_INIT_STRING);
        init->u.string.val             = strdup(val);
        init->u.string.null_terminated = null_term;
        return init;
    }

    Tac_StaticInit *CreateStaticInitPointer(const char *name)
    {
        Tac_StaticInit *init = new_tac_static_init(TAC_STATIC_INIT_POINTER);
        init->u.pointer_name = strdup(name);
        return init;
    }

    // Helper to create a top-level declaration
    Tac_TopLevel *CreateFunction(const char *name, bool global, Tac_Param *params,
                                 Tac_Instruction *body)
    {
        Tac_TopLevel *tl      = new_tac_toplevel(TAC_TOPLEVEL_FUNCTION);
        tl->u.function.name   = strdup(name);
        tl->u.function.global = global;
        tl->u.function.params = params;
        tl->u.function.body   = body;
        return tl;
    }

    Tac_TopLevel *CreateStaticVar(const char *name, bool global, Tac_Type *type,
                                  Tac_StaticInit *init_list)
    {
        Tac_TopLevel *tl                = new_tac_toplevel(TAC_TOPLEVEL_STATIC_VARIABLE);
        tl->u.static_variable.name      = strdup(name);
        tl->u.static_variable.global    = global;
        tl->u.static_variable.type      = type;
        tl->u.static_variable.init_list = init_list;
        return tl;
    }

    Tac_TopLevel *CreateStaticConst(const char *name, Tac_Type *type, Tac_StaticInit *init)
    {
        Tac_TopLevel *tl           = new_tac_toplevel(TAC_TOPLEVEL_STATIC_CONSTANT);
        tl->u.static_constant.name = strdup(name);
        tl->u.static_constant.type = type;
        tl->u.static_constant.init = init;
        return tl;
    }
};

// Test empty program
TEST_F(TacParserTest, EmptyProgram)
{
    EXPECT_TRUE(Parse(""));
    Tac_Program *expected = new_tac_program();
    EXPECT_TRUE(compare_tac_program(parsed_program, expected));
    free_tac_program(expected);
}

// Test function with no parameters
TEST_F(TacParserTest, FunctionNoParams)
{
    EXPECT_TRUE(Parse("function main() { return 0; }"));
    Tac_Program *expected  = new_tac_program();
    Tac_Instruction *instr = CreateInstruction(TAC_INSTRUCTION_RETURN);
    instr->u.return_.src   = CreateIntConst(0);
    expected->decls        = CreateFunction("main", false, nullptr, instr);
    EXPECT_TRUE(compare_tac_program(parsed_program, expected));
    free_tac_program(expected);
}

// Test global function with parameters
TEST_F(TacParserTest, GlobalFunctionWithParams)
{
    EXPECT_TRUE(Parse("function global add(x, y) { binary add t1, x, y; return t1; }"));
    Tac_Program *expected   = new_tac_program();
    Tac_Param *param1       = CreateParam("x");
    Tac_Param *param2       = CreateParam("y");
    param1->next            = param2;
    Tac_Instruction *instr1 = CreateInstruction(TAC_INSTRUCTION_BINARY);
    instr1->u.binary.op     = TAC_BINARY_ADD;
    instr1->u.binary.dst    = CreateVar("t1");
    instr1->u.binary.src1   = CreateVar("x");
    instr1->u.binary.src2   = CreateVar("y");
    Tac_Instruction *instr2 = CreateInstruction(TAC_INSTRUCTION_RETURN);
    instr2->u.return_.src   = CreateVar("t1");
    instr1->next            = instr2;
    expected->decls         = CreateFunction("add", true, param1, instr1);
    EXPECT_TRUE(compare_tac_program(parsed_program, expected));
    free_tac_program(expected);
}

// Test static variable
TEST_F(TacParserTest, StaticVariable)
{
    EXPECT_TRUE(Parse("static int x = 42;"));
    Tac_Program *expected = new_tac_program();
    Tac_StaticInit *init  = CreateStaticInitInt(42);
    expected->decls       = CreateStaticVar("x", false, CreateBasicType(TAC_TYPE_INT), init);
    EXPECT_TRUE(compare_tac_program(parsed_program, expected));
    free_tac_program(expected);
}

// Test global static variable with init list
TEST_F(TacParserTest, GlobalStaticVariableWithInitList)
{
    EXPECT_TRUE(Parse("static global *int arr = 1, zero(4), \"text\";"));
    Tac_Program *expected = new_tac_program();
    Tac_StaticInit *init1 = CreateStaticInitInt(1);
    Tac_StaticInit *init2 = CreateStaticInitZero(4);
    Tac_StaticInit *init3 = CreateStaticInitString("text", true);
    init1->next           = init2;
    init2->next           = init3;
    expected->decls =
        CreateStaticVar("arr", true, CreatePointerType(CreateBasicType(TAC_TYPE_INT)), init1);
    EXPECT_TRUE(compare_tac_program(parsed_program, expected));
    free_tac_program(expected);
}

// Test static constant
TEST_F(TacParserTest, StaticConstant)
{
    EXPECT_TRUE(Parse("const double pi = 3.14;"));
    Tac_Program *expected = new_tac_program();
    Tac_StaticInit *init  = CreateStaticInitDouble(3.14);
    expected->decls       = CreateStaticConst("pi", CreateBasicType(TAC_TYPE_DOUBLE), init);
    EXPECT_TRUE(compare_tac_program(parsed_program, expected));
    free_tac_program(expected);
}

// Test multiple top-level declarations
TEST_F(TacParserTest, MultipleTopLevel)
{
    EXPECT_TRUE(Parse("function main() { return 0; } static int x = 42; const char c = 'a';"));
    Tac_Program *expected  = new_tac_program();
    Tac_Instruction *instr = CreateInstruction(TAC_INSTRUCTION_RETURN);
    instr->u.return_.src   = CreateIntConst(0);
    Tac_TopLevel *tl1      = CreateFunction("main", false, nullptr, instr);
    Tac_TopLevel *tl2 =
        CreateStaticVar("x", false, CreateBasicType(TAC_TYPE_INT), CreateStaticInitInt(42));
    Tac_TopLevel *tl3 =
        CreateStaticConst("c", CreateBasicType(TAC_TYPE_CHAR), CreateStaticInitChar('a'));
    tl1->next       = tl2;
    tl2->next       = tl3;
    expected->decls = tl1;
    EXPECT_TRUE(compare_tac_program(parsed_program, expected));
    free_tac_program(expected);
}

// Test function with complex instructions
TEST_F(TacParserTest, ComplexFunction)
{
    EXPECT_TRUE(
        Parse("function foo(a) { copy t1, a; fun_call bar, (t1, 3.14), t2; jump_if_zero t2, L1; "
              "label L1; }"));
    Tac_Program *expected            = new_tac_program();
    Tac_Param *param                 = CreateParam("a");
    Tac_Instruction *instr1          = CreateInstruction(TAC_INSTRUCTION_COPY);
    instr1->u.copy.dst               = CreateVar("t1");
    instr1->u.copy.src               = CreateVar("a");
    Tac_Instruction *instr2          = CreateInstruction(TAC_INSTRUCTION_FUN_CALL);
    instr2->u.fun_call.fun_name      = strdup("bar");
    instr2->u.fun_call.args          = CreateVar("t1");
    instr2->u.fun_call.args->next    = CreateDoubleConst(3.14);
    instr2->u.fun_call.dst           = CreateVar("t2");
    Tac_Instruction *instr3          = CreateInstruction(TAC_INSTRUCTION_JUMP_IF_ZERO);
    instr3->u.jump_if_zero.condition = CreateVar("t2");
    instr3->u.jump_if_zero.target    = strdup("L1");
    Tac_Instruction *instr4          = CreateInstruction(TAC_INSTRUCTION_LABEL);
    instr4->u.label.name             = strdup("L1");
    instr1->next                     = instr2;
    instr2->next                     = instr3;
    instr3->next                     = instr4;
    expected->decls                  = CreateFunction("foo", false, param, instr1);
    EXPECT_TRUE(compare_tac_program(parsed_program, expected));
    free_tac_program(expected);
}

// Test static init with pointer
TEST_F(TacParserTest, StaticInitPointer)
{
    EXPECT_TRUE(Parse("static *int p = &func;"));
    Tac_Program *expected = new_tac_program();
    Tac_StaticInit *init  = CreateStaticInitPointer("func");
    expected->decls =
        CreateStaticVar("p", false, CreatePointerType(CreateBasicType(TAC_TYPE_INT)), init);
    EXPECT_TRUE(compare_tac_program(parsed_program, expected));
    free_tac_program(expected);
}

// Test complex type
TEST_F(TacParserTest, ComplexType)
{
    EXPECT_TRUE(Parse("static fun(int, *double) -> char f = &g;"));
    Tac_Program *expected = new_tac_program();
    Tac_Type *param1      = CreateBasicType(TAC_TYPE_INT);
    Tac_Type *param2      = CreatePointerType(CreateBasicType(TAC_TYPE_DOUBLE));
    param1->next          = param2;
    Tac_Type *type        = CreateFunType(param1, CreateBasicType(TAC_TYPE_CHAR));
    Tac_StaticInit *init  = CreateStaticInitPointer("g");
    expected->decls       = CreateStaticVar("f", false, type, init);
    EXPECT_TRUE(compare_tac_program(parsed_program, expected));
    free_tac_program(expected);
}

// Test empty instruction list
TEST_F(TacParserTest, EmptyInstructionList)
{
    EXPECT_TRUE(Parse("function empty() { }"));
    Tac_Program *expected = new_tac_program();
    expected->decls       = CreateFunction("empty", false, nullptr, nullptr);
    EXPECT_TRUE(compare_tac_program(parsed_program, expected));
    free_tac_program(expected);
}

// Test invalid inputs
TEST_F(TacParserTest, InvalidFunctionSyntax)
{
    EXPECT_FALSE(Parse("function main { return 0; }"));
    EXPECT_TRUE(error_message.find("Error") != std::string::npos);
}

TEST_F(TacParserTest, InvalidStaticSyntax)
{
    EXPECT_FALSE(Parse("static int x;"));
    EXPECT_TRUE(error_message.find("Error") != std::string::npos);
}

TEST_F(TacParserTest, InvalidInstruction)
{
    EXPECT_FALSE(Parse("function main() { invalid t1, t2; }"));
    EXPECT_TRUE(error_message.find("Error") != std::string::npos);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
