//
// Golden-file tests for the Bemsh (Cyrillic autocode) emitter (emit_bemsh.c).
//
// These assert the exact `.bemsh` text produced for a representative set — the B1
// acceptance criteria.  Bemsh is a Dubna-monitor dialect like Madlen, so the output uses
// the `старт`/`финиш` module framing, Cyrillic mnemonics, and `=в'…'`/`=е'…'` literal
// commands, with the index register parenthesized after the address (`сч (6)`, `уиа g(14)`).
//
// Layout: label field is 8 columns (column 1 = leftmost), then the mnemonic, then the
// operand — all space-separated.  Names are provisionally sanitized to ≤6 chars (the real
// mangling and the Bemsh-libc helper-symbol map are task B2), so e.g. `program`→`progra`,
// `counter`→`counte`, `b$ret`→`b_ret`.
//
#include "codegen_test.h"

// A scalar-returning function: `старт` framing, prologue (счим/пв b_save), param load
// `сч (6)`, `=в'…'` octal literal, epilogue `пб b_ret`, `финиш`.
TEST_F(CodegenTest, BemshScalarReturn)
{
    std::string out = CompileToBemsh("int f(int x) { return x + 1; }");
    EXPECT_EQ(R"(*
f        старт
b_ret    внешн .b_ret
         счим 13
         пв b_save
         сч (6)
         сл =в'1'
         пб b_ret
         финиш
)",
              out);
}

// A direct call: `уиа -1(14)` sets the negated arg count in r14, then `пв g`.
TEST_F(CodegenTest, BemshCall)
{
    std::string out = CompileToBemsh("int g(int); int f(int x) { return g(x) + 1; }");
    EXPECT_EQ(R"(*
f        старт
b_ret    внешн .b_ret
         счим 13
         пв b_save
         сч (6)
         уиа -1(14)
         пв g
         сл =в'1'
         пб b_ret
         финиш
)",
              out);
}

// `main` gets the Dubna-monitor `program` entry via `входн` (provisional 6-char `progra`).
TEST_F(CodegenTest, BemshMainEntry)
{
    std::string out = CompileToBemsh("int main() { return 0; }");
    EXPECT_EQ(R"(*
main     старт
b_ret    внешн .b_ret
         входн progra
         счим 13
         пв b_save
         сч
         пб b_ret
         финиш
)",
              out);
}

// A module-level global: reserved with `пам 1`; the accessor reaches it via `мода counte`
// (UTC) + a bare `сч` load.
TEST_F(CodegenTest, BemshGlobalAccess)
{
    std::string out = CompileToBemsh("int counter; int f(void) { return counter; }");
    EXPECT_EQ(R"(*
counte   старт
         пам 1
         финиш
*
f        старт
b_ret    внешн .b_ret
counte   внешн .counte
         счим 13
         пв b_save
         мода counte
         сч
         пб b_ret
         финиш
)",
              out);
}

// A floating-point constant becomes a `=е'…'` literal command (type Е, mandatory point).
TEST_F(CodegenTest, BemshFloatConst)
{
    std::string out = CompileToBemsh("double f(void) { return 1.5; }");
    EXPECT_EQ(R"(*
f        старт
b_ret    внешн .b_ret
         счим 13
         пв b_save
         сч =е'1.5'
         пб b_ret
         финиш
)",
              out);
}

// A string global: the character data is KOI-7-packed (via utf8_to_koi7 in static.c) into
// a `конд в'…'` octal word — confirming Bemsh, like Madlen, encodes program strings to KOI-7.
TEST_F(CodegenTest, BemshStringGlobal)
{
    std::string out = CompileToBemsh("char *s = \"HELLO\";");
    EXPECT_EQ(R"(*
s        старт
         конд а()
         конд а(_str0)
_str0    конд в'2204251423047400'
         финиш
)",
              out);
}

// An initialized int array: each word is a `конд в'…'` octal constant (10→'12', 20→'24',
// 30→'36').
TEST_F(CodegenTest, BemshIntArray)
{
    std::string out = CompileToBemsh("int a[3] = {10, 20, 30};");
    EXPECT_EQ(R"(*
a        старт
         конд в'12'
         конд в'24'
         конд в'36'
         финиш
)",
              out);
}

// Address-of a global: `уиа g(14)` loads its address into r14, then `счи 14` moves it to A.
TEST_F(CodegenTest, BemshAddrOfGlobal)
{
    std::string out = CompileToBemsh("int g; int *f(void) { return &g; }");
    EXPECT_EQ(R"(*
g        старт
         пам 1
         финиш
*
f        старт
b_ret    внешн .b_ret
g        внешн .g
         счим 13
         пв b_save
         уиа g(14)
         счи 14
         пб b_ret
         финиш
)",
              out);
}
