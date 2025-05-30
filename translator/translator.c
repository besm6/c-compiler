
#include "translator.h"
#include "ast.h"

// Enable debug output
int translator_debug;

/* Main parsing function */
Tac_Program *translate(FILE *input)
{
    if (translator_debug) {
        printf("--- %s()\n", __func__);
    }
    Program *ast = import_ast(fileno(input));

    // TODO: translate AST into TAC.
    print_program(stdout, ast);
    free_program(ast);
    return NULL;
}
