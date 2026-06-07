#include <stdlib.h>

#include "besm.h"
#include "xalloc.h"

Besm_Instr *besm_new_instr(Besm_InstrKind kind)
{
    Besm_Instr *instr = (Besm_Instr *)xalloc(sizeof(Besm_Instr), __func__, __FILE__, __LINE__);
    instr->kind       = kind;
    return instr;
}

Besm_Block *besm_new_block(void)
{
    Besm_Block *block = (Besm_Block *)xalloc(sizeof(Besm_Block), __func__, __FILE__, __LINE__);
    return block;
}

Besm_Func *besm_new_func(const char *name, Besm_CallConv cc)
{
    Besm_Func *func = (Besm_Func *)xalloc(sizeof(Besm_Func), __func__, __FILE__, __LINE__);
    if (name) {
        func->name = xstrdup(name);
    }
    func->cc = cc;
    return func;
}

Besm_DataSection *besm_new_data_section(Besm_SectionKind kind)
{
    Besm_DataSection *section =
        (Besm_DataSection *)xalloc(sizeof(Besm_DataSection), __func__, __FILE__, __LINE__);
    section->kind = kind;
    return section;
}

Besm_Module *besm_new_module(const char *name)
{
    Besm_Module *module =
        (Besm_Module *)xalloc(sizeof(Besm_Module), __func__, __FILE__, __LINE__);
    if (name) {
        module->name = xstrdup(name);
    }
    return module;
}
