//
// Golden-file tests for the Bemsh (Cyrillic autocode) emitter (emit_bemsh.c).
//
// These assert the exact `.bemsh` text produced for a representative set.  Bemsh is a
// Dubna-monitor dialect like Madlen: each module uses the `старт`/`финиш` framing, Cyrillic
// mnemonics, and `=в'…'`/`=е'…'` literal commands, with the index register parenthesized after
// the address (`сч (6)`, `уиа g(14)`).  Every `старт…финиш` module is further wrapped in its own
// Macro-Bemsh `ввд$$$` … `квч$$$/трн$$$/0-0/блмак/бтмалф/кнц$$$` deck (task B3) — the БЕМШ
// translator processes one module per deck, and besmc/the Dubna job add no such markers.
//
// Layout: label field is 6 columns (column 1 = leftmost), then the mnemonic, then the
// operand — all space-separated.  Names are mangled by bemsh_mangle (task B2) to ≤6-char,
// letter-first Bemsh labels, with runtime helpers mapped to the `libbem.bin` exports:
// `program`→`progra`, `counter`→`counte`, `b$ret`→`_ret`, `b$save`→`_save`.
//
#include <map>
#include <set>
#include <utility>

#include "codegen_test.h"

// The mangler under test.  emit_bemsh.c is compiled as C, so declare it with C linkage
// (internal.h has no C++ guard, so we do not include it from this C++ test).
extern "C" void bemsh_mangle(char *dst, size_t n, const char *src);

namespace {

// One call to the mangler with a comfortable buffer, returned as a std::string.
std::string mangle(const char *src)
{
    char buf[16];
    bemsh_mangle(buf, sizeof(buf), src);
    return std::string(buf);
}

// A leading letter, or a leading '_' (Dubna renders it as the Cyrillic letter Ю, which
// satisfies Bemsh's "labels must begin with a letter" rule — see docs/Bemsh.md §4).
bool letter_first(const std::string &s)
{
    if (s.empty())
        return false;
    char c = s[0];
    return c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

// Only letters, digits, and '_' are permitted in a mangled Bemsh label.
bool valid_chars(const std::string &s)
{
    return std::all_of(s.begin(), s.end(), [](char c) {
        return c == '_' || (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
               (c >= 'a' && c <= 'z');
    });
}

} // namespace

// A scalar-returning function: `старт` framing, prologue (счим/пв _save), param load
// `сч (6)`, `=в'…'` octal literal, epilogue `пб _ret`, `финиш`.
TEST_F(CodegenTest, BemshScalarReturn)
{
    std::string out = CompileToBemsh("int f(int x) { return x + 1; }");
    EXPECT_EQ(R"(ввд$$$
*
f      старт 1
_save  внешн ._save
_ret   внешн ._ret
       счим 13
       пв _save(13)
       сч (6)
       сл =в'1'
       пб _ret
       финиш
квч$$$
трн$$$
0-0
блмак
бтмалф
кнц$$$
)",
              out);
}

// A direct call: `уиа -1(14)` sets the negated arg count in r14, then `пв g`.
TEST_F(CodegenTest, BemshCall)
{
    std::string out = CompileToBemsh("int g(int); int f(int x) { return g(x) + 1; }");
    EXPECT_EQ(R"(ввд$$$
*
f      старт 1
_save  внешн ._save
g      внешн .g
_ret   внешн ._ret
       счим 13
       пв _save(13)
       сч (6)
       уиа -1(14)
       пв g(13)
       сл =в'1'
       пб _ret
       финиш
квч$$$
трн$$$
0-0
блмак
бтмалф
кнц$$$
)",
              out);
}

// `main` gets the Dubna-monitor `program` entry via `входн`, truncated to the 6-char
// Bemsh label `progra` (matching B3's `*main progra` control card).
TEST_F(CodegenTest, BemshMainEntry)
{
    std::string out = CompileToBemsh("int main() { return 0; }");
    EXPECT_EQ(R"(ввд$$$
*
main   старт 1
_save0 внешн ._save0
_ret   внешн ._ret
       входн progra
       счим 13
       пв _save0(13)
       сч
       пб _ret
       финиш
квч$$$
трн$$$
0-0
блмак
бтмалф
кнц$$$
)",
              out);
}

// A module-level global: reserved with `пам 1`; the accessor reaches it via `мода counte`
// (UTC) + a bare `сч` load.
TEST_F(CodegenTest, BemshGlobalAccess)
{
    std::string out = CompileToBemsh("int counter; int f(void) { return counter; }");
    EXPECT_EQ(R"(ввд$$$
*
counte старт 1
       пам 1
       финиш
квч$$$
трн$$$
0-0
блмак
бтмалф
кнц$$$
ввд$$$
*
f      старт 1
_save0 внешн ._save0
_ret   внешн ._ret
counte внешн .counte
       счим 13
       пв _save0(13)
       мода counte
       сч
       пб _ret
       финиш
квч$$$
трн$$$
0-0
блмак
бтмалф
кнц$$$
)",
              out);
}

// A floating-point constant becomes a `=е'…'` literal command (type Е, mandatory point).
TEST_F(CodegenTest, BemshFloatConst)
{
    std::string out = CompileToBemsh("double f(void) { return 1.5; }");
    EXPECT_EQ(R"(ввд$$$
*
f      старт 1
_save0 внешн ._save0
_ret   внешн ._ret
       счим 13
       пв _save0(13)
       сч =е'1.5'
       пб _ret
       финиш
квч$$$
трн$$$
0-0
блмак
бтмалф
кнц$$$
)",
              out);
}

// A string global: the character data is KOI-7-packed (via utf8_to_koi7 in static.c) into
// a `конд в'…'` octal word — confirming Bemsh, like Madlen, encodes program strings to KOI-7.
TEST_F(CodegenTest, BemshStringGlobal)
{
    std::string out = CompileToBemsh("char *s = \"HELLO\";");
    EXPECT_EQ(R"(ввд$$$
*
s      старт 1
       конд а()
       конд а(_str0)
_str0  конд в'2204251423047400'
       финиш
квч$$$
трн$$$
0-0
блмак
бтмалф
кнц$$$
)",
              out);
}

// An initialized int array: each word is a `конд в'…'` octal constant (10→'12', 20→'24',
// 30→'36').
TEST_F(CodegenTest, BemshIntArray)
{
    std::string out = CompileToBemsh("int a[3] = {10, 20, 30};");
    EXPECT_EQ(R"(ввд$$$
*
a      старт 1
       конд в'12'
       конд в'24'
       конд в'36'
       финиш
квч$$$
трн$$$
0-0
блмак
бтмалф
кнц$$$
)",
              out);
}

// Address-of a global: `уиа g(14)` loads its address into r14, then `счи 14` moves it to A.
TEST_F(CodegenTest, BemshAddrOfGlobal)
{
    std::string out = CompileToBemsh("int g; int *f(void) { return &g; }");
    EXPECT_EQ(R"(ввд$$$
*
g      старт 1
       пам 1
       финиш
квч$$$
трн$$$
0-0
блмак
бтмалф
кнц$$$
ввд$$$
*
f      старт 1
_save0 внешн ._save0
_ret   внешн ._ret
g      внешн .g
       счим 13
       пв _save0(13)
       уиа g(14)
       счи 14
       пб _ret
       финиш
квч$$$
трн$$$
0-0
блмак
бтмалф
кнц$$$
)",
              out);
}

//
// bemsh_mangle unit tests (task B2 acceptance): deterministic ≤6-char letter-first labels,
// runtime helpers mapped to the libbem.bin exports, no collisions over a corpus.
//

// Every runtime helper the compiler emits maps to its exact libbem.bin export symbol.
// This list must stay in sync with bemsh_helper_map[] in emit_bemsh.c and the export map
// in libc/besm6/bemsh/README.md.
TEST(BemshMangle, HelperNamesMatchLibbem)
{
    const std::pair<const char *, const char *> helpers[] = {
        { "b$save", "_save" },   { "b$save0", "_save0" }, { "b$ret", "_ret" },
        { "b$mul", "_mul" },     { "b$div", "_div" },     { "b$mod", "_mod" },
        { "b$uadd", "_uadd" },   { "b$usub", "_usub" },   { "b$umul", "_umul" },
        { "b$udiv", "_udiv" },   { "b$umod", "_umod" },   { "b$uneg", "_uneg" },
        { "b$lsh", "_lsh" },     { "b$rsh", "_rsh" },     { "b$eq", "_eq" },
        { "b$ne", "_ne" },       { "b$lt", "_lt" },       { "b$le", "_le" },
        { "b$gt", "_gt" },       { "b$ge", "_ge" },       { "b$not", "_not" },
        { "b$ult", "_ult" },     { "b$ule", "_ule" },     { "b$ugt", "_ugt" },
        { "b$uge", "_uge" },     { "b$flt", "_flt" },     { "b$fle", "_fle" },
        { "b$fgt", "_fgt" },     { "b$fge", "_fge" },     { "b$dtoi", "_dtoi" },
        { "b$dtou", "_dtou" },   { "b$utod", "_utod" },   { "b$padd", "_padd" },
        { "b$pinc", "_pinc" },   { "b$pdec", "_pdec" },   { "b$pdiff", "_pdiff" },
        { "b$stb", "_stb" },     { "b$tout", "_tout" },
    };
    std::set<std::string> outputs;
    for (auto &h : helpers) {
        std::string got = mangle(h.first);
        EXPECT_EQ(got, h.second) << "helper " << h.first;
        EXPECT_LE(got.size(), 6u) << h.first;
        EXPECT_TRUE(letter_first(got)) << got;
        EXPECT_TRUE(outputs.insert(got).second) << "duplicate helper symbol " << got;
    }
    EXPECT_EQ(outputs.size(), 38u); // all helper exports distinct
}

// The non-b$ libc leaves carry no '$' and are already ≤6 chars, so they pass through
// unchanged (they are NOT in the helper table).
TEST(BemshMangle, LibcLeavesUnchanged)
{
    EXPECT_EQ(mangle("exit"), "exit");
    EXPECT_EQ(mangle("frexp"), "frexp");
    EXPECT_EQ(mangle("ldexp"), "ldexp");
}

// Reserved / representative names: truncation preserves the shapes the goldens and the
// Dubna control card depend on.
TEST(BemshMangle, GeneralRuleShapes)
{
    EXPECT_EQ(mangle("program"), "progra"); // must match B3's `*main progra` card
    EXPECT_EQ(mangle("counter"), "counte"); // truncate to 6
    EXPECT_EQ(mangle("main"), "main");
    EXPECT_EQ(mangle("_str0"), "_str0"); // string-constant name, leading '_' kept
    EXPECT_EQ(mangle("g"), "g");
    EXPECT_EQ(mangle("%L2"), "L2");   // branch label: '%' dropped
    EXPECT_EQ(mangle("%3"), "T3");    // temp label: '%' dropped, digit-first → 'T' prefix
    EXPECT_EQ(mangle("foo$1"), "foo1"); // static-local suffix: '$' dropped (NOT a helper)
    EXPECT_EQ(mangle("b$0"), "b0");     // a static named `b` — general path, not a helper
    EXPECT_EQ(mangle("=в'1'"), "=в'1'"); // literal-command operand passes through verbatim
}

// Invariants over a broad corpus: every output is ≤6 chars, letter-first, letters/digits/_
// only, and the mangler is deterministic (idempotent per input).
TEST(BemshMangle, InvariantsAndDeterminism)
{
    const char *corpus[] = {
        "main", "counter", "program", "helper", "compute", "process", "verylongname",
        "_str0", "_str1", "value", "index", "buffer", "result", "total",
        "%L1", "%L2", "%L10", "%1", "%3", "%99",
        "flag$0", "flag$1", "b$ret", "b$save", "b$pdiff", "exit", "frexp",
        "123abc", "_only", "$$$", "%", "a_b_c_d", "X",
    };
    for (const char *name : corpus) {
        std::string a = mangle(name);
        std::string b = mangle(name);
        EXPECT_EQ(a, b) << "non-deterministic for " << name;
        EXPECT_FALSE(a.empty()) << name;
        EXPECT_LE(a.size(), 6u) << name << " → " << a;
        EXPECT_TRUE(letter_first(a)) << name << " → " << a;
        EXPECT_TRUE(valid_chars(a)) << name << " → " << a;
    }
}

// No collisions over a representative corpus of distinct globals/functions/strings/statics/
// labels — the guard for the accepted provisional truncation-collision risk.
TEST(BemshMangle, NoCollisionsOverCorpus)
{
    const char *corpus[] = {
        "counter", "total", "result", "buffer", "index", "value",  // globals
        "main", "helper", "compute", "process",                    // functions
        "_str0", "_str1", "_str2",                                 // string constants
        "flag$0", "flag$1",                                        // static locals
        "%L1", "%L2", "%L10", "%3", "%7",                          // labels / temps
        "b$ret", "b$save", "b$mul",                                // helpers
    };
    std::map<std::string, std::string> seen; // output → first input that produced it
    for (const char *name : corpus) {
        std::string out = mangle(name);
        auto it = seen.find(out);
        EXPECT_TRUE(it == seen.end())
            << "collision: '" << name << "' and '" << (it == seen.end() ? "" : it->second)
            << "' both mangle to '" << out << "'";
        seen[out] = name;
    }
}

// Cross-module consistency: a linkage name (here the 7-char function `counter`, truncated to
// `counte`) must mangle identically at its definition module and at its `пв` call site in
// another module — otherwise separately-emitted modules would fail to link.  (The `внешн`
// declaration B3 now emits for a `пв` call target is exercised by the golden tests above.)
TEST_F(CodegenTest, BemshCrossModuleNameConsistency)
{
    std::string out =
        CompileToBemsh("int counter(void) { return 1; } int f(void) { return counter(); }");
    EXPECT_NE(out.find("counte старт"), std::string::npos); // definition label
    EXPECT_NE(out.find("пв counte"), std::string::npos);    // call site in f
    EXPECT_EQ(out.find("counter"), std::string::npos);      // never the un-truncated name
}

// LOG literal (task B5): the shared instruction selector carries octal masks/immediates in
// Madlen form (`=377`).  Bemsh must render the type-В (octal) literal `=в'377'` — Macro-Bemsh
// rejects a bare `=377` as a mistyped constant.  A char-byte load masks with `и =в'377'`.
TEST_F(CodegenTest, BemshByteMaskLiteral)
{
    std::string out = CompileToBemsh("int f(char *p) { return *p; }");
    EXPECT_NE(out.find("и =в'377'"), std::string::npos); // type-В octal, not bare =377
    EXPECT_EQ(out.find("=377"), std::string::npos);      // never the Madlen LOG form
}

// OCT literal (task B5): the Madlen `=:64` (OCT) literal is *left*-justified in the word, so
// Bemsh must render `=в'6400000000000000'` (value at the left, zero-padded right), not the
// right-justified `=в'0000000000000064'`.  A char*/array decay ORs in the fat-pointer marker.
TEST_F(CodegenTest, BemshFatMarkerLiteral)
{
    std::string out = CompileToBemsh("char a[4]; int f(void) { return a[0]; }");
    EXPECT_NE(out.find("=в'6400000000000000'"), std::string::npos); // left-justified OCT
    EXPECT_EQ(out.find("=в'0000000000000064'"), std::string::npos); // not right-justified
}

// Large real (task B5): 2^40 = 1.099511627776e12 has a 13-digit decimal mantissa one over the
// Bemsh type-Е field limit (2^40-1) and no exact shorter form, so the emitter falls back to the
// exact octal bit pattern `=в'…'` rather than an overflowing `=е'…'`.  Small reals stay decimal.
TEST_F(CodegenTest, BemshLargeRealOctalFallback)
{
    std::string out = CompileToBemsh("double f(void) { return 1099511627776.0; }");
    EXPECT_NE(out.find("сч =в'6450000000000000'"), std::string::npos); // exact octal word
    EXPECT_EQ(out.find("=е'"), std::string::npos);                     // no decimal (would overflow)
}
