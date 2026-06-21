#include <stdlib.h>

#include "besm.h"
#include "xalloc.h"

void besm_free_instr(Besm_Instr *instr)
{
    if (!instr)
        return;
    xfree(instr->name);
    xfree(instr->label);
    besm_free_instr(instr->next);
    xfree(instr);
}

void besm_free_block(Besm_Block *block)
{
    if (!block)
        return;
    xfree(block->name);
    besm_free_instr(block->body);
    besm_free_block(block->next);
    xfree(block);
}

void besm_free_func(Besm_Func *func)
{
    if (!func)
        return;
    xfree(func->name);
    besm_free_block(func->blocks);
    besm_free_func(func->next);
    xfree(func);
}

void besm_free_data_section(Besm_DataSection *section)
{
    if (!section)
        return;
    xfree(section->name);
    besm_free_instr(section->items);
    besm_free_data_section(section->next);
    xfree(section);
}

void besm_free_module(Besm_Module *module)
{
    if (!module)
        return;
    xfree(module->name);
    xfree(module->comment);
    besm_free_func(module->funcs);
    besm_free_data_section(module->sections);
    xfree(module);
}
