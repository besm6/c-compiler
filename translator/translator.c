
#include "translator.h"
#include "ast.h"
#include "wio.h"

// Enable debug output
int translator_debug;

//
// Translate AST from given file into TAC tree.
//
Tac_Program *translate(FILE *fd)
{
    if (translator_debug) {
        printf("--- %s()\n", __func__);
    }
    WFILE input;
    ast_import_open(&input, fileno(fd));
    for (;;) {
        ExternalDecl *decl = import_external_decl(&input);
        if (!decl)
            break;

        // TODO: translate AST into TAC.
        print_external_decl(stdout, decl, 0);
        free_external_decl(decl);
    }
    wclose(&input);
    return NULL;
}
