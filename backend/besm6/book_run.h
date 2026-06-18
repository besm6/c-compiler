//
// Shared helper for the imported "Writing a C Compiler" run tests
// (chapterNN_run_tests.cpp).
//
// The book's positive programs define `int main(void)` and are validated by
// their return value (an exit code, or 0 on success for self-checking tests).
// The BESM-6 libc entry point calls `void program()`, so we wrap each program
// with a program() that prints main()'s return value; the expected value is the
// stdout of the same wrapped source compiled and run with the host compiler.
//
#ifndef BESM6_BOOK_RUN_H
#define BESM6_BOOK_RUN_H

#include <string>

#include "codegen_test.h"

// Wrap a book program so program() prints `main()`'s return value as "%d\n".
inline std::string WrapMain(const std::string &program)
{
    return "int printf(const char *format, ...);\n" + program +
           "\nvoid program(void) { printf(\"%d\\n\", main()); }\n";
}

#endif // BESM6_BOOK_RUN_H
