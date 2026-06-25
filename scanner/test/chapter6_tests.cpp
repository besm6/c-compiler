//
// Chapter 6 — Conditionals: invalid lexical input.
// Imported from "Writing a C Compiler" (tests/chapter_6/invalid_lex).
//
#include "scan_fixture.h"

// 0invalid_label: — a label may not start with a digit; '0invalid_label' is a
// single invalid numeric token, not '0' followed by an identifier.
TEST(ScannerChapter6, BadLabel_Neg)
{
    EXPECT_DEATH(LexToEnd(R"(int main(void) {
    0invalid_label:
        return 0;
}
)"),
                 "invalid suffix on numeric constant");
}
