#include <stdio.h>
#include <string.h>

#include "besm.h"

void mad_fresh_label(char *buf, size_t n, const char *prefix)
{
    static int counter = 0;
    snprintf(buf, n, "%s.%d", prefix, counter++);
}

//
// Build the address-field string from (name, addr).
// name+addr → "name+N", name-addr → "name-N", name only → "name",
// addr only → "N", both zero/null → "" (buf already zeroed by caller).
//
static void addr_str(char *buf, size_t n, const char *name, int addr)
{
    if (name && addr > 0)
        snprintf(buf, n, "%s+%d", name, addr);
    else if (name && addr < 0)
        snprintf(buf, n, "%s-%d", name, -addr);
    else if (name)
        snprintf(buf, n, "%s", name);
    else if (addr)
        snprintf(buf, n, "%d", addr);
}

//
// Emit one Madlen statement line.
//
static void emit_line(FILE *out, const char *label, int mreg,
                      const char *mnem, const char *addr)
{
    if (label) {
        fprintf(out, " %8s:", label);
    } else {
        fprintf(out, "          ");
    }
    if (mreg) {
        fprintf(out, "%2d ", mreg);
    } else {
        fprintf(out, "   ");
    }
    fprintf(out, ",%s,", mnem);
    if (addr && addr[0])
        fprintf(out, " %s", addr);
    fputc('\n', out);
}

void emit_madlen_instr(FILE *out, const Besm_Instr *instr)
{
    for (; instr; instr = instr->next) {
        char a[64] = "";
        switch (instr->kind) {
        // Load / store
        case BESM_MEM_XTA:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "xta", a);
            break;
        case BESM_MEM_ATX:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "atx", a);
            break;
        case BESM_MEM_STX:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "stx", a);
            break;
        case BESM_MEM_XTS:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "xts", a);
            break;
        // Index-register transfer (addr = ireg number)
        case BESM_MEM_ITA:
            snprintf(a, sizeof(a), "%d", instr->addr);
            emit_line(out, NULL, 0, "ita", a);
            break;
        case BESM_MEM_ATI:
            snprintf(a, sizeof(a), "%d", instr->addr);
            emit_line(out, NULL, 0, "ati", a);
            break;
        case BESM_MEM_ITS:
            snprintf(a, sizeof(a), "%d", instr->addr);
            emit_line(out, NULL, 0, "its", a);
            break;
        case BESM_MEM_STI:
            snprintf(a, sizeof(a), "%d", instr->addr);
            emit_line(out, NULL, 0, "sti", a);
            break;
        // MTJ: reg=src, addr=dst_j
        case BESM_MEM_MTJ:
            snprintf(a, sizeof(a), "%d", instr->addr);
            emit_line(out, NULL, instr->reg, "mtj", a);
            break;

        // Floating-point arithmetic
        case BESM_ARITH_ADD:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "a+x", a);
            break;
        case BESM_ARITH_SUB:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "a-x", a);
            break;
        case BESM_ARITH_RSUB:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "x-a", a);
            break;
        case BESM_ARITH_ABSSUB:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "amx", a);
            break;
        case BESM_ARITH_MUL:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "a*x", a);
            break;
        case BESM_ARITH_DIV:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "a/x", a);
            break;
        case BESM_ARITH_CNEG:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "avx", a);
            break;

        // Logical / bit-manipulation
        case BESM_LOG_AAX:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "aax", a);
            break;
        case BESM_LOG_AOX:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "aox", a);
            break;
        case BESM_LOG_AEX:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "aex", a);
            break;
        case BESM_LOG_ARX:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "arx", a);
            break;
        case BESM_LOG_APX:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "apx", a);
            break;
        case BESM_LOG_AUX:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "aux", a);
            break;
        case BESM_LOG_ACX:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "acx", a);
            break;
        case BESM_LOG_ANX:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "anx", a);
            break;

        // Exponent / shift (memory operand)
        case BESM_EXP_EADDX:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "e+x", a);
            break;
        case BESM_EXP_ESUBX:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "e-x", a);
            break;
        case BESM_EXP_SHIFTX:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "asx", a);
            break;
        case BESM_EXP_SETRMEM:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "xtr", a);
            break;
        // Exponent / shift (immediate: addr = immediate value)
        case BESM_EXP_GETR:
            snprintf(a, sizeof(a), "%d", instr->addr);
            emit_line(out, NULL, 0, "rte", a);
            break;
        case BESM_EXP_YTA:
            snprintf(a, sizeof(a), "%d", instr->addr);
            emit_line(out, NULL, 0, "yta", a);
            break;
        case BESM_EXP_EADDN:
            snprintf(a, sizeof(a), "%d", instr->addr);
            emit_line(out, NULL, 0, "e+n", a);
            break;
        case BESM_EXP_ESUBN:
            snprintf(a, sizeof(a), "%d", instr->addr);
            emit_line(out, NULL, 0, "e-n", a);
            break;
        case BESM_EXP_SHIFTN:
            snprintf(a, sizeof(a), "%d", instr->addr);
            emit_line(out, NULL, 0, "asn", a);
            break;
        case BESM_EXP_SETR:
            snprintf(a, sizeof(a), "%d", instr->addr);
            emit_line(out, NULL, 0, "ntr", a);
            break;

        // Index-register manipulation (reg=dst, addr=value or dst_j)
        case BESM_REG_VTM:
            snprintf(a, sizeof(a), "%d", instr->addr);
            emit_line(out, NULL, instr->reg, "vtm", a);
            break;
        case BESM_REG_UTM:
            if (instr->addr)
                snprintf(a, sizeof(a), "%d", instr->addr);
            emit_line(out, NULL, instr->reg, "utm", a);
            break;
        case BESM_REG_JADDM:
            snprintf(a, sizeof(a), "%d", instr->addr);
            emit_line(out, NULL, instr->reg, "j+m", a);
            break;

        // C register
        case BESM_MOD_UTC:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "utc", a);
            break;
        case BESM_MOD_WTC:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "wtc", a);
            break;

        // Control flow
        case BESM_BRANCH_UZA:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "uza", a);
            break;
        case BESM_BRANCH_U1A:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "u1a", a);
            break;
        case BESM_BRANCH_UJ:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "uj", a);
            break;
        case BESM_BRANCH_VJM:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "vjm", a);
            break;
        case BESM_BRANCH_VZM:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "vzm", a);
            break;
        case BESM_BRANCH_V1M:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "v1m", a);
            break;
        case BESM_BRANCH_VLM:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "vlm", a);
            break;
        case BESM_BRANCH_STOP:
            emit_line(out, NULL, 0, "stop", "");
            break;

        // Assembly directives
        case BESM_STMT_LABEL:
            emit_line(out, instr->name, 0, "bss", "");
            break;
        case BESM_STMT_NAME:
            emit_line(out, instr->name, 0, "name", "");
            break;
        case BESM_STMT_BASE:
            emit_line(out, NULL, instr->reg, "base", instr->name);
            break;
        case BESM_BRANCH_CALL:
            emit_line(out, NULL, 0, "call", instr->name);
            break;
        case BESM_STMT_SUBP:
            emit_line(out, instr->name, 0, "subp", "");
            break;
        case BESM_STMT_ENTRY:
            emit_line(out, NULL, 0, "entry", instr->name);
            break;
        case BESM_STMT_END:
            emit_line(out, NULL, 0, "end", "");
            break;

        // Data section directives
        case BESM_DATA_LOG:
            snprintf(a, sizeof(a), "%llo", instr->log_val);
            emit_line(out, NULL, 0, "log", a);
            break;
        case BESM_DATA_BSS:
            if (instr->addr)
                snprintf(a, sizeof(a), "%d", instr->addr);
            emit_line(out, NULL, 0, "bss", a);
            break;
        case BESM_DATA_INT:
            snprintf(a, sizeof(a), "%d", instr->addr);
            emit_line(out, NULL, 0, "int", a);
            break;
        case BESM_DATA_REAL:
            snprintf(a, sizeof(a), "%g", instr->real_val);
            emit_line(out, NULL, 0, "real", a);
            break;
        case BESM_DATA_EQU:
            snprintf(a, sizeof(a), "%d", instr->addr);
            emit_line(out, NULL, 0, "equ", a);
            break;
        case BESM_DATA_REF:
            emit_line(out, NULL, 0, "oct", instr->name);
            break;
        case BESM_DATA_STRING: {
            const char *s = instr->name;
            while (*s) {
                snprintf(a, sizeof(a), "%d", (unsigned char)*s++);
                emit_line(out, NULL, 0, "int", a);
            }
            emit_line(out, NULL, 0, "int", "0");
            break;
        }
        case BESM_DATA_Z00:
            addr_str(a, sizeof(a), instr->name, instr->addr);
            emit_line(out, NULL, instr->reg, "z00", a);
            break;
        }
    }
}

void emit_madlen_block(FILE *out, const Besm_Block *block)
{
    for (; block; block = block->next)
        emit_madlen_instr(out, block->body);
}

void emit_madlen_func(FILE *out, const Besm_Func *func)
{
    for (; func; func = func->next)
        emit_madlen_block(out, func->blocks);
}

void emit_madlen_data_section(FILE *out, const Besm_DataSection *section)
{
    for (; section; section = section->next) {
        emit_line(out, section->name, 0, "name", "");
        emit_madlen_instr(out, section->items);
        emit_line(out, NULL, 0, "end", "");
    }
}

void emit_madlen_module(FILE *out, const Besm_Module *module)
{
    // Separator.
    fprintf(out, "c\n");

    emit_madlen_func(out, module->funcs);
    emit_madlen_data_section(out, module->sections);
}
