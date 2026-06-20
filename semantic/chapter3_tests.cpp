//
// Chapter 3 — Binary Operators: semantic errors.
// Imported from "Writing a C Compiler" (tests/chapter_3/invalid_parse).
//
// The book classifies malformed_paren.c as a *parse* error because its grammar
// only allows a call on an identifier.  Our parser is more permissive — it parses
// a call on any postfix expression — so "2 (- 3)" parses cleanly and the error
// surfaces in the type checker as "called object is not a function".
//
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include "typecheck_fixture.h"

// fatal_error() for the semantic-book-tests executable.  The compiler libraries
// call it; the regular semantic-tests binary defines its own copy (in
// typecheck_tests.cpp), so the book executable needs this one.
extern "C" _Noreturn void fatal_error(const char *message, ...)
{
    fprintf(stderr, "Fatal error: ");
    va_list ap;
    va_start(ap, message);
    vfprintf(stderr, message, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

// return 2 (- 3); — calling an integer constant as if it were a function.
TEST_F(PipelineTest, Chapter3_MalformedParen_Neg)
{
    EXPECT_DEATH(RunPipeline("int main(void) {\n    return 2 (- 3);\n}\n"),
                 "Expression is not a function or function pointer");
}
