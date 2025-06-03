#include "translator.h"
#include <stdarg.h>
#include <stdlib.h>

//
// Error handling
//
void _Noreturn fatal_error(const char *message, ...)
{
    fprintf(stderr, "Fatal error: ");

    va_list ap;
    va_start(ap, message);
    vfprintf(stderr, message, ap);
    va_end(ap);

    fprintf(stderr, "\n");
    exit(1);
}

//
// Resolve identifiers.
//
void resolve(ExternalDecl *ast) // cppcheck-suppress constParameterPointer
{
    //TODO
}

//
// Typecheck definitions and uses of functions and variables.
//
void typecheck(ExternalDecl *ast) // cppcheck-suppress constParameterPointer
{
    //TODO
}

//
// Annotate loops and break/continue statements.
//
void label_loops(ExternalDecl *ast) // cppcheck-suppress constParameterPointer
{
    //TODO
}

//
// Convert the AST to TAC.
//
Tac_TopLevel *translate(ExternalDecl *ast) // cppcheck-suppress constParameterPointer
{
    //TODO
    return NULL;
}
