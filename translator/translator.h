//
// Internal types for translator.
//
#ifndef TACKER_H
#define TACKER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tac.h"

// Enable debug output
extern int translator_debug;
extern int import_debug;
extern int export_debug;
extern int wio_debug;

Tac_Program *translate(FILE *fd);

#ifdef GTEST_API_
//TODO
#endif

#ifdef __cplusplus
}
#endif

#endif /* TACKER_H */
