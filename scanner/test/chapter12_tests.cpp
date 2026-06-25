//
// Chapter 12 — Unsigned integers: invalid lexical input.
// Imported from "Writing a C Compiler" (tests/chapter_12/invalid_lex).
// Each program ends an integer constant with a malformed suffix the scanner
// must reject: an unsigned constant takes at most one 'u', and the long part
// is a single 'l'/'L' (or a matched-case 'll'/'LL') — so a doubled 'uu' or a
// stray 'lul' run is invalid.
//
#include "scan_fixture.h"

// return 0uu; — an unsigned suffix may carry only a single 'u'.
TEST(ScannerChapter12, InvalidSuffix_Neg)
{
    EXPECT_DEATH(LexToEnd("int main(void) {\n    return 0uu;\n}\n"),
                 "invalid suffix on numeric constant");
}

// return 0lul; — 'lul' is not a valid integer suffix.
TEST(ScannerChapter12, InvalidSuffix2_Neg)
{
    EXPECT_DEATH(LexToEnd("int main(void) {\n    return 0lul;\n}\n"),
                 "invalid suffix on numeric constant");
}
