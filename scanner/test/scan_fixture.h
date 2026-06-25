//
// Shared helpers for the imported "Writing a C Compiler" scanner tests
// (chapterNN_tests.cpp).  Negative tests feed a malformed program to the
// scanner, which reports a lexical error via lex_error() -> exit(1); the tests
// wrap LexToEnd() in EXPECT_DEATH and assert on the message text.
//
#ifndef SCANNER_SCAN_FIXTURE_H
#define SCANNER_SCAN_FIXTURE_H

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "scanner.h"

// Tokenize `src` to completion.  A lexical error aborts the process, so call
// this inside EXPECT_DEATH for inputs that must fail to lex.
inline void LexToEnd(const char *src)
{
    FILE *f = tmpfile();
    if (!f) {
        perror("tmpfile");
        exit(2);
    }
    fwrite(src, 1, strlen(src), f);
    rewind(f);
    init_scanner(f);
    while (yylex() != TOKEN_EOF) {
        // keep scanning
    }
    fclose(f);
}

#endif // SCANNER_SCAN_FIXTURE_H
