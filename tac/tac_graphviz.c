#include <stdio.h>

#include "tac.h"

void tac_fprint_dot(FILE *fd, const Tac_TopLevel *tl)
{
    if (!tl || tl->kind != TAC_TOPLEVEL_FUNCTION) {
        fprintf(fd, "digraph TAC { empty [label=\"no function\"]; }\n");
        return;
    }
    fprintf(fd, "digraph TAC {\n");
    fprintf(fd, "  labelloc=\"t\";\n  label=\"function %s\";\n",
            tl->u.function.name ? tl->u.function.name : "?");
    int id = 0;
    for (const Tac_Instruction *in = tl->u.function.body; in; in = in->next) {
        fprintf(fd, "  n%d [shape=box, label=\"kind=%d\"];\n", id, (int)in->kind);
        if (in->next) {
            fprintf(fd, "  n%d -> n%d;\n", id, id + 1);
        }
        id++;
    }
    fprintf(fd, "}\n");
}
