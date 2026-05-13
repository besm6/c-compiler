#ifndef TAC_YAML_H
#define TAC_YAML_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include "tac.h"

void tac_export_yaml(FILE *fd, const Tac_TopLevel *tl);

#ifdef __cplusplus
}
#endif

#endif // TAC_YAML_H
