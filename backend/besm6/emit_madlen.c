#include <stdio.h>
#include <string.h>

#include "besm.h"

void mad_fresh_label(char *buf, size_t n, const char *prefix)
{
    static int counter = 0;
    snprintf(buf, n, "%s.%d", prefix, counter++);
}

//
// Fill buf with the address-field string for a mem_addr.
// The reg field is emitted separately into the M column; only the offset/name
// goes into buf.  buf is zeroed by callers so an empty address needs no write.
//
static void mem_addr_str(char *buf, size_t n, const Besm_MemAddr *addr)
{
    switch (addr->kind) {
    case BESM_MEM_ADDR_REG:
    case BESM_MEM_ADDR_SEG_REG:
        if (addr->u.offset)
            snprintf(buf, n, "%d", addr->u.offset);
        break;
    case BESM_MEM_ADDR_LABEL:
    case BESM_MEM_ADDR_SEG_LABEL:
        snprintf(buf, n, "%s", addr->u.name);
        break;
    }
}

static void target_str(char *buf, size_t n, const Besm_Target *tgt)
{
    if (tgt->kind == BESM_TARGET_LABEL)
        snprintf(buf, n, "%s", tgt->u.name);
    else
        snprintf(buf, n, "%d", tgt->u.offset);
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

// ---------------------------------------------------------------------------
// Sub-instruction emitters (all static)
// ---------------------------------------------------------------------------

static void emit_mem_instr(FILE *out, const Besm_MemInstr *mem)
{
    char addr[64] = "";
    switch (mem->kind) {
    case BESM_MEM_XTA:
        mem_addr_str(addr, sizeof(addr), &mem->u.addr);
        emit_line(out, NULL, mem->u.addr.reg.num, "xta", addr);
        break;
    case BESM_MEM_ATX:
        mem_addr_str(addr, sizeof(addr), &mem->u.addr);
        emit_line(out, NULL, mem->u.addr.reg.num, "atx", addr);
        break;
    case BESM_MEM_STX:
        mem_addr_str(addr, sizeof(addr), &mem->u.addr);
        emit_line(out, NULL, mem->u.addr.reg.num, "stx", addr);
        break;
    case BESM_MEM_XTS:
        mem_addr_str(addr, sizeof(addr), &mem->u.addr);
        emit_line(out, NULL, mem->u.addr.reg.num, "xts", addr);
        break;
    case BESM_MEM_ITA:
        snprintf(addr, sizeof(addr), "%d", mem->u.ireg);
        emit_line(out, NULL, 0, "ita", addr);
        break;
    case BESM_MEM_ATI:
        snprintf(addr, sizeof(addr), "%d", mem->u.ireg);
        emit_line(out, NULL, 0, "ati", addr);
        break;
    case BESM_MEM_ITS:
        snprintf(addr, sizeof(addr), "%d", mem->u.ireg);
        emit_line(out, NULL, 0, "its", addr);
        break;
    case BESM_MEM_STI:
        snprintf(addr, sizeof(addr), "%d", mem->u.ireg);
        emit_line(out, NULL, 0, "sti", addr);
        break;
    case BESM_MEM_MTJ:
        snprintf(addr, sizeof(addr), "%d", mem->u.mtj.dst_j);
        emit_line(out, NULL, mem->u.mtj.src.num, "mtj", addr);
        break;
    }
}

static void emit_arith_instr(FILE *out, const Besm_ArithInstr *arith)
{
    char addr[64] = "";
    const char *mnem;
    mem_addr_str(addr, sizeof(addr), &arith->addr);
    switch (arith->kind) {
    case BESM_ARITH_ADD:    mnem = "a+x"; break;
    case BESM_ARITH_SUB:    mnem = "a-x"; break;
    case BESM_ARITH_RSUB:   mnem = "x-a"; break;
    case BESM_ARITH_ABSSUB: mnem = "amx"; break;
    case BESM_ARITH_MUL:    mnem = "a*x"; break;
    case BESM_ARITH_DIV:    mnem = "a/x"; break;
    case BESM_ARITH_CNEG:   mnem = "avx"; break;
    default:                mnem = "";    break;
    }
    emit_line(out, NULL, arith->addr.reg.num, mnem, addr);
}

static void emit_log_instr(FILE *out, const Besm_LogInstr *log)
{
    char addr[64] = "";
    const char *mnem;
    mem_addr_str(addr, sizeof(addr), &log->addr);
    switch (log->kind) {
    case BESM_LOG_AAX: mnem = "aax"; break;
    case BESM_LOG_AOX: mnem = "aox"; break;
    case BESM_LOG_AEX: mnem = "aex"; break;
    case BESM_LOG_ARX: mnem = "arx"; break;
    case BESM_LOG_APX: mnem = "apx"; break;
    case BESM_LOG_AUX: mnem = "aux"; break;
    case BESM_LOG_ACX: mnem = "acx"; break;
    case BESM_LOG_ANX: mnem = "anx"; break;
    default:           mnem = "";    break;
    }
    emit_line(out, NULL, log->addr.reg.num, mnem, addr);
}

static void emit_exp_instr(FILE *out, const Besm_ExpInstr *exp)
{
    char addr[64] = "";
    const char *mnem;
    switch (exp->kind) {
    case BESM_EXP_EADDX:
        mem_addr_str(addr, sizeof(addr), &exp->u.addr);
        emit_line(out, NULL, exp->u.addr.reg.num, "e+x", addr);
        return;
    case BESM_EXP_ESUBX:
        mem_addr_str(addr, sizeof(addr), &exp->u.addr);
        emit_line(out, NULL, exp->u.addr.reg.num, "e-x", addr);
        return;
    case BESM_EXP_SHIFTX:
        mem_addr_str(addr, sizeof(addr), &exp->u.addr);
        emit_line(out, NULL, exp->u.addr.reg.num, "asx", addr);
        return;
    case BESM_EXP_SETRMEM:
        mem_addr_str(addr, sizeof(addr), &exp->u.addr);
        emit_line(out, NULL, exp->u.addr.reg.num, "xtr", addr);
        return;
    case BESM_EXP_GETR:   snprintf(addr, sizeof(addr), "%d", exp->u.imm); mnem = "rte"; break;
    case BESM_EXP_YTA:    snprintf(addr, sizeof(addr), "%d", exp->u.imm); mnem = "yta"; break;
    case BESM_EXP_EADDN:  snprintf(addr, sizeof(addr), "%d", exp->u.imm); mnem = "e+n"; break;
    case BESM_EXP_ESUBN:  snprintf(addr, sizeof(addr), "%d", exp->u.imm); mnem = "e-n"; break;
    case BESM_EXP_SHIFTN: snprintf(addr, sizeof(addr), "%d", exp->u.imm); mnem = "asn"; break;
    case BESM_EXP_SETR:   snprintf(addr, sizeof(addr), "%d", exp->u.imm); mnem = "ntr"; break;
    default:              mnem = ""; break;
    }
    emit_line(out, NULL, 0, mnem, addr);
}

static void emit_reg_instr(FILE *out, const Besm_RegInstr *reg)
{
    char addr[64] = "";
    switch (reg->kind) {
    case BESM_REG_VTM:
        snprintf(addr, sizeof(addr), "%d", reg->u.vtm.value);
        emit_line(out, NULL, reg->u.vtm.dst.num, "vtm", addr);
        break;
    case BESM_REG_UTM:
        if (reg->u.vtm.value)
            snprintf(addr, sizeof(addr), "%d", reg->u.vtm.value);
        emit_line(out, NULL, reg->u.vtm.dst.num, "utm", addr);
        break;
    case BESM_REG_JADDM:
        snprintf(addr, sizeof(addr), "%d", reg->u.jaddm.dst_j);
        emit_line(out, NULL, reg->u.jaddm.src.num, "j+m", addr);
        break;
    }
}

static void emit_mod_instr(FILE *out, const Besm_ModInstr *mod)
{
    char addr[64] = "";
    const char *mnem;
    mem_addr_str(addr, sizeof(addr), &mod->addr);
    switch (mod->kind) {
    case BESM_MOD_UTC: mnem = "utc"; break;
    case BESM_MOD_WTC: mnem = "wtc"; break;
    default:           mnem = "";    break;
    }
    emit_line(out, NULL, mod->addr.reg.num, mnem, addr);
}

static void emit_branch_instr(FILE *out, const Besm_BranchInstr *branch)
{
    char addr[64] = "";
    switch (branch->kind) {
    case BESM_BRANCH_UZA:
        mem_addr_str(addr, sizeof(addr), &branch->u.addr);
        emit_line(out, NULL, branch->u.addr.reg.num, "uza", addr);
        break;
    case BESM_BRANCH_U1A:
        mem_addr_str(addr, sizeof(addr), &branch->u.addr);
        emit_line(out, NULL, branch->u.addr.reg.num, "u1a", addr);
        break;
    case BESM_BRANCH_UJ:
        mem_addr_str(addr, sizeof(addr), &branch->u.addr);
        emit_line(out, NULL, branch->u.addr.reg.num, "uj", addr);
        break;
    case BESM_BRANCH_VJM:
        target_str(addr, sizeof(addr), &branch->u.jump.tgt);
        emit_line(out, NULL, branch->u.jump.reg.num, "vjm", addr);
        break;
    case BESM_BRANCH_VZM:
        target_str(addr, sizeof(addr), &branch->u.jump.tgt);
        emit_line(out, NULL, branch->u.jump.reg.num, "vzm", addr);
        break;
    case BESM_BRANCH_V1M:
        target_str(addr, sizeof(addr), &branch->u.jump.tgt);
        emit_line(out, NULL, branch->u.jump.reg.num, "v1m", addr);
        break;
    case BESM_BRANCH_VLM:
        target_str(addr, sizeof(addr), &branch->u.jump.tgt);
        emit_line(out, NULL, branch->u.jump.reg.num, "vlm", addr);
        break;
    case BESM_BRANCH_STOP:
        emit_line(out, NULL, 0, "stop", "");
        break;
    }
}

static void emit_extra_instr(FILE *out, const Besm_ExtraInstr *extra)
{
    char mnem_buf[8];
    char addr[64] = "";
    const char *mnem;
    const Besm_MemAddr *a;

    if (extra->kind == BESM_EXTRA_ESYS) {
        snprintf(mnem_buf, sizeof(mnem_buf), "*%02o", extra->u.esys.opcode);
        mnem = mnem_buf;
        a    = &extra->u.esys.addr;
    } else {
        switch (extra->kind) {
        case BESM_EXTRA_ESQRT:  mnem = "*50"; break;
        case BESM_EXTRA_ESIN:   mnem = "*51"; break;
        case BESM_EXTRA_ECOS:   mnem = "*52"; break;
        case BESM_EXTRA_EATAN:  mnem = "*53"; break;
        case BESM_EXTRA_EASIN:  mnem = "*54"; break;
        case BESM_EXTRA_ELN:    mnem = "*55"; break;
        case BESM_EXTRA_EEXP:   mnem = "*56"; break;
        case BESM_EXTRA_ETAPE:  mnem = "*57"; break;
        case BESM_EXTRA_EIN:    mnem = "*60"; break;
        case BESM_EXTRA_EOUT:   mnem = "*61"; break;
        case BESM_EXTRA_ETXTIO: mnem = "*63"; break;
        case BESM_EXTRA_EPRINT: mnem = "*64"; break;
        case BESM_EXTRA_ETIME:  mnem = "*75"; break;
        case BESM_EXTRA_ETIMEH: mnem = "*76"; break;
        default:                mnem = "";    break;
        }
        a = &extra->u.addr;
    }
    mem_addr_str(addr, sizeof(addr), a);
    emit_line(out, NULL, a->reg.num, mnem, addr);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void emit_madlen_instr(FILE *out, const Besm_Instr *instr)
{
    for (; instr; instr = instr->next) {
        switch (instr->kind) {
        case BESM_INSTR_MEM:
            emit_mem_instr(out, &instr->u.mem);
            break;
        case BESM_INSTR_ARITH:
            emit_arith_instr(out, &instr->u.arith);
            break;
        case BESM_INSTR_LOG:
            emit_log_instr(out, &instr->u.log_);
            break;
        case BESM_INSTR_EXP:
            emit_exp_instr(out, &instr->u.exp);
            break;
        case BESM_INSTR_REG:
            emit_reg_instr(out, &instr->u.reg);
            break;
        case BESM_INSTR_MOD:
            emit_mod_instr(out, &instr->u.mod);
            break;
        case BESM_INSTR_BRANCH:
            emit_branch_instr(out, &instr->u.branch);
            break;
        case BESM_INSTR_EXTRA:
            emit_extra_instr(out, &instr->u.extra);
            break;
        case BESM_INSTR_LABEL:
            emit_line(out, instr->u.name, 0, "bss", "");
            break;
        case BESM_INSTR_NAME:
            emit_line(out, instr->u.name, 0, "name", "");
            break;
        case BESM_INSTR_REL:
            emit_line(out, NULL, 0, "rel", "");
            break;
        case BESM_INSTR_CALL:
            emit_line(out, NULL, 0, "call", instr->u.name);
            break;
        case BESM_INSTR_SUBP:
            emit_line(out, instr->u.name, 0, "subp", "");
            break;
        case BESM_INSTR_ENTRY:
            emit_line(out, NULL, 0, "entry", instr->u.name);
            break;
        case BESM_INSTR_END:
            emit_line(out, NULL, 0, "end", "");
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

//
// Emit one data item.  label is applied to the first output line only
// (for DI_String, which expands to multiple lines).
//
static void emit_data_item_labeled(FILE *out, const Besm_DataItem *item,
                                   const char *label)
{
    char addr[64] = "";
    switch (item->kind) {
    case BESM_DATA_INT:
        snprintf(addr, sizeof(addr), "%d", item->u.int_val);
        emit_line(out, label, 0, "int", addr);
        break;
    case BESM_DATA_REAL:
        snprintf(addr, sizeof(addr), "%g", (double)item->u.real_val);
        emit_line(out, label, 0, "real", addr);
        break;
    case BESM_DATA_OCT:
        snprintf(addr, sizeof(addr), "%o", (unsigned)item->u.oct_val);
        emit_line(out, label, 0, "oct", addr);
        break;
    case BESM_DATA_LOG:
        snprintf(addr, sizeof(addr), "%o", (unsigned)item->u.log_val);
        emit_line(out, label, 0, "log", addr);
        break;
    case BESM_DATA_BSS:
        if (item->u.bss_words)
            snprintf(addr, sizeof(addr), "%d", item->u.bss_words);
        emit_line(out, label, 0, "bss", addr);
        break;
    case BESM_DATA_EQU:
        snprintf(addr, sizeof(addr), "%d", item->u.equ_val);
        emit_line(out, label, 0, "equ", addr);
        break;
    case BESM_DATA_REF:
        emit_line(out, label, 0, "oct", item->u.ref_name);
        break;
    case BESM_DATA_STRING: {
        const char *s     = item->u.string_val;
        const char *first = label;
        while (*s) {
            snprintf(addr, sizeof(addr), "%d", (unsigned char)*s++);
            emit_line(out, first, 0, "int", addr);
            first = NULL;
        }
        emit_line(out, first, 0, "int", "0");
        break;
    }
    }
}

void emit_madlen_data_item(FILE *out, const Besm_DataItem *item)
{
    for (; item; item = item->next)
        emit_data_item_labeled(out, item, NULL);
}

void emit_madlen_data_section(FILE *out, const Besm_DataSection *section)
{
    for (; section; section = section->next) {
        const Besm_DataItem *item  = section->items;
        const char          *label = section->name;
        for (; item; item = item->next) {
            emit_data_item_labeled(out, item, label);
            label = NULL; // only first item gets the section label
        }
    }
}

void emit_madlen_module(FILE *out, const Besm_Module *module)
{
    if (module->name) {
        fprintf(out, "c Module: %s\n", module->name);
    }
    emit_madlen_func(out, module->funcs);
    emit_madlen_data_section(out, module->sections);
}
