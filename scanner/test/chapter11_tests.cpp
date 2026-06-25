//
// Chapter 11 — Long integers: invalid lexical input.
// Imported from "Writing a C Compiler" (tests/chapter_11/invalid_lex).
// Each program ends an integer constant with a malformed suffix the scanner
// must reject.  Only one 'l'/'L' (or a matched-case 'll'/'LL') is permitted, so
// a mixed-case 'lL' or a triple 'LLL' is invalid.
//
#include "scan_fixture.h"

// return 0lL; — 'lL' mixes case in the long-long suffix, which is invalid.
TEST(ScannerChapter11, InvalidSuffix_Neg)
{
    EXPECT_DEATH(LexToEnd("int main(void) {\n    return 0lL;\n}\n"),
                 "invalid suffix on numeric constant");
}

// return 0LLL; — a triple 'LLL' suffix is never valid.
TEST(ScannerChapter11, InvalidSuffix2_Neg)
{
    EXPECT_DEATH(LexToEnd("int main(void) {\n    return 0LLL;\n}\n"),
                 "invalid suffix on numeric constant");
}
