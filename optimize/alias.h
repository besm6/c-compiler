#pragma once
#include "cfg.h"
#include "string_map.h"
#include "tac.h"

// Populates two freshly-initialised StringMaps:
//   static_names   — names of all TAC_TOPLEVEL_STATIC_VARIABLE entries
//   address_taken  — names of all Var operands of GET_ADDRESS instructions
// Caller must map_destroy() both maps when done.
void collect_alias_sets(const OptCfg *cfg, const Tac_TopLevel *toplevel,
                        StringMap *static_names, StringMap *address_taken);
