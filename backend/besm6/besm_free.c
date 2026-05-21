#include <stdlib.h>

#include "besm.h"
#include "xalloc.h"

// Free the heap string inside an embedded Besm_MemAddr, if any.
static void free_mem_addr(Besm_MemAddr *addr)
{
    if (addr->kind == BESM_MEM_ADDR_LABEL || addr->kind == BESM_MEM_ADDR_SEG_LABEL) {
        xfree(addr->u.name);
    }
}

// Free the heap string inside an embedded Besm_Target, if any.
static void free_target(Besm_Target *tgt)
{
    if (tgt->kind == BESM_TARGET_LABEL) {
        xfree(tgt->u.name);
    }
}

void besm_free_instr(Besm_Instr *instr)
{
    if (!instr)
        return;
    switch (instr->kind) {
    case BESM_INSTR_MEM:
        switch (instr->u.mem.kind) {
        case BESM_MEM_XTA:
        case BESM_MEM_ATX:
        case BESM_MEM_STX:
        case BESM_MEM_XTS:
            free_mem_addr(&instr->u.mem.u.addr);
            break;
        default:
            break;
        }
        break;
    case BESM_INSTR_ARITH:
        free_mem_addr(&instr->u.arith.addr);
        break;
    case BESM_INSTR_LOG:
        free_mem_addr(&instr->u.log_.addr);
        break;
    case BESM_INSTR_EXP:
        switch (instr->u.exp.kind) {
        case BESM_EXP_EADDX:
        case BESM_EXP_ESUBX:
        case BESM_EXP_SHIFTX:
        case BESM_EXP_SETRMEM:
            free_mem_addr(&instr->u.exp.u.addr);
            break;
        default:
            break;
        }
        break;
    case BESM_INSTR_REG:
        break;
    case BESM_INSTR_MOD:
        free_mem_addr(&instr->u.mod.addr);
        break;
    case BESM_INSTR_BRANCH:
        switch (instr->u.branch.kind) {
        case BESM_BRANCH_UZA:
        case BESM_BRANCH_U1A:
        case BESM_BRANCH_UJ:
            free_mem_addr(&instr->u.branch.u.addr);
            break;
        case BESM_BRANCH_VJM:
        case BESM_BRANCH_VZM:
        case BESM_BRANCH_V1M:
        case BESM_BRANCH_VLM:
            free_target(&instr->u.branch.u.jump.tgt);
            break;
        case BESM_BRANCH_STOP:
            break;
        }
        break;
    case BESM_INSTR_EXTRA:
        if (instr->u.extra.kind == BESM_EXTRA_ESYS) {
            free_mem_addr(&instr->u.extra.u.esys.addr);
        } else {
            free_mem_addr(&instr->u.extra.u.addr);
        }
        break;
    case BESM_INSTR_LABEL:
    case BESM_INSTR_NAME:
    case BESM_INSTR_CALL:
    case BESM_INSTR_SUBP:
    case BESM_INSTR_ENTRY:
        xfree(instr->u.name);
        break;
    case BESM_INSTR_REL:
    case BESM_INSTR_END:
        break;
    }
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

void besm_free_data_item(Besm_DataItem *item)
{
    if (!item)
        return;
    if (item->kind == BESM_DATA_REF) {
        xfree(item->u.ref_name);
    } else if (item->kind == BESM_DATA_STRING) {
        xfree(item->u.string_val);
    }
    besm_free_data_item(item->next);
    xfree(item);
}

void besm_free_data_section(Besm_DataSection *section)
{
    if (!section)
        return;
    xfree(section->name);
    besm_free_data_item(section->items);
    besm_free_data_section(section->next);
    xfree(section);
}

void besm_free_module(Besm_Module *module)
{
    if (!module)
        return;
    xfree(module->name);
    besm_free_func(module->funcs);
    besm_free_data_section(module->sections);
    xfree(module);
}
