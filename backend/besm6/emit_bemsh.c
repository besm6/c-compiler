#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "besm.h"
#include "internal.h"

//
// Bemsh (Cyrillic autocode) assembly emitter.
//
// Bemsh is the second Dubna-monitor dialect, so this emitter is structurally a port of
// emit_madlen.c: the same IR traversal and the same five-shape operand classifier
// (besm_operand_shape), differing only in spelling.  The three Bemsh-specific choices are:
//
//   * Cyrillic mnemonics/directives (besm_cyr_mnem[] below), emitted as readable lowercase
//     UTF-8 — Bemsh is case-insensitive, so lowercase is fine and keeps golden files
//     legible.  The mnemonic/directive text is NOT run through utf8_to_koi7; only program
//     string-literal *data* is KOI-7-encoded, and that happens upstream in static.c.
//   * The index register is written parenthesized *after* the address (`уиа масс(13)`),
//     not as a separate field before the mnemonic as in Madlen.
//   * Bemsh column form: the label begins in column 1 (blank column 1 = no label);
//     fields are space-separated.
//
// Name mangling is provisional here (see bemsh_sanitize): the real ≤6-char, letter-first,
// collision-safe scheme and the Bemsh-libc helper-symbol map are task B2; dubna
// run-integration is task B3.  Several SPECIAL renderings (call, внешн binding, финиш
// entry operand, Z00 displacement) are provisional golden-file forms, refined under B2/B3.
//

// Cyrillic machine-instruction mnemonics indexed by Besm_InstrKind — the Bemsh counterpart
// of besm_latin_mnem[].  NULL for BESM_SHAPE_SPECIAL kinds (directives / data / UTM / CALL /
// BASE), whose Bemsh spelling lives in emit_bemsh_special.
static const char *const besm_cyr_mnem[] = {
    [BESM_MEM_XTA] = "сч",     [BESM_MEM_ATX] = "зп",     [BESM_MEM_STX] = "зпм",
    [BESM_MEM_XTS] = "счм",    [BESM_MEM_ITA] = "счи",    [BESM_MEM_ATI] = "уи",
    [BESM_MEM_ITS] = "счим",   [BESM_MEM_STI] = "уим",    [BESM_MEM_MTJ] = "уии",

    [BESM_ARITH_ADD] = "сл",   [BESM_ARITH_SUB] = "вч",   [BESM_ARITH_RSUB] = "вчоб",
    [BESM_ARITH_ABSSUB] = "вчаб", [BESM_ARITH_MUL] = "умн", [BESM_ARITH_DIV] = "дел",
    [BESM_ARITH_CNEG] = "знак",

    [BESM_LOG_AAX] = "и",      [BESM_LOG_AOX] = "или",    [BESM_LOG_AEX] = "нтж",
    [BESM_LOG_ARX] = "слц",    [BESM_LOG_APX] = "сбр",    [BESM_LOG_AUX] = "рзб",
    [BESM_LOG_ACX] = "чед",    [BESM_LOG_ANX] = "нед",

    [BESM_EXP_EADDX] = "слп",  [BESM_EXP_ESUBX] = "вчп",  [BESM_EXP_SHIFTX] = "сд",
    [BESM_EXP_SETRMEM] = "рж", [BESM_EXP_GETR] = "счрж",  [BESM_EXP_YTA] = "счмр",
    [BESM_EXP_EADDN] = "слпа", [BESM_EXP_ESUBN] = "вчпа", [BESM_EXP_SHIFTN] = "сда",
    [BESM_EXP_SETR] = "ржа",

    [BESM_REG_VTM] = "уиа",    [BESM_REG_UTM] = "слиа",   [BESM_REG_JADDM] = "сли",

    [BESM_MOD_UTC] = "мода",   [BESM_MOD_WTC] = "мод",

    [BESM_BRANCH_UZA] = "по",  [BESM_BRANCH_U1A] = "пе",  [BESM_BRANCH_UJ] = "пб",
    [BESM_BRANCH_VJM] = "пв",  [BESM_BRANCH_VZM] = "пио", [BESM_BRANCH_V1M] = "пино",
    [BESM_BRANCH_VLM] = "цикл", [BESM_BRANCH_STOP] = "стоп",

    // Directives / data / UTM / CALL / BASE are BESM_SHAPE_SPECIAL — no shared entry.
    [BESM_DATA_Z00] = NULL,
};

// Format a double as the decimal-number field of a Bemsh Е (floating-point) constant.
// Like Madlen, Bemsh requires a mandatory decimal point ("2." not "2").  Copied from
// emit_madlen.c's mad_format_real so the two emitters stay decoupled.
static void bemsh_format_real(char *buf, size_t n, double val)
{
    // 13 significant digits round-trip a BESM-6 40-bit mantissa exactly.
    snprintf(buf, n, "%.13g", val);
    char *e = strpbrk(buf, "eE");
    if (!strchr(buf, '.') && !e) {
        double a = val < 0 ? -val : val;
        if (a >= 1099511627776.0) { // 2^40 — bare integer mantissa overflows the field
            snprintf(buf, n, "%.12e", val);
            e = strpbrk(buf, "eE");
        }
    }
    if (strchr(buf, '.'))
        return; // already has a decimal point
    size_t len = strlen(buf);
    if (e) {
        size_t pos = (size_t)(e - buf);
        if (len + 1 < n) {
            memmove(e + 1, e, len - pos + 1); // shift exponent (incl NUL) right by one
            *e = '.';
        }
    } else if (len + 1 < n) {
        buf[len]     = '.';
        buf[len + 1] = '\0';
    }
}

// Provisional Bemsh identifier sanitizer (B1 placeholder; the real ≤6-char, letter-first,
// collision-safe mangler and the Bemsh-libc helper-symbol map are task B2).  The IR carries
// runtime helpers as `b$ret`/`b$save` and temporaries/labels as `%…`; Bemsh labels are
// letters+digits only, so replace the `$`/`/`/`%` separators with `_` and truncate to 6.
// (In Dubna output the `_` renders as `Ю` — that is normal.)
static void bemsh_sanitize(char *dst, size_t n, const char *src)
{
    size_t lim = n - 1 < 6 ? n - 1 : 6;
    size_t i   = 0;
    for (; *src && i < lim; src++, i++) {
        char c = *src;
        if (c == '_' || c == '$' || c == '/' || c == '%')
            c = '_';
        dst[i] = c;
    }
    dst[i] = '\0';
}

//
// Build the address-field string from (name, addr): "name+N" / "name-N" / "name" / "N".
// Names beginning with '=' are Bemsh literal-command operands, passed through verbatim.
//
static void addr_str(char *buf, size_t n, const char *name, int addr)
{
    char sname[8];
    const char *aname = name;
    if (name && name[0] != '=') {
        bemsh_sanitize(sname, sizeof(sname), name);
        aname = sname;
    }
    if (name && addr > 0)
        snprintf(buf, n, "%s+%d", aname, addr);
    else if (name && addr < 0)
        snprintf(buf, n, "%s-%d", aname, -addr);
    else if (name)
        snprintf(buf, n, "%s", aname);
    else if (addr)
        snprintf(buf, n, "%d", addr);
}

//
// Format instruction `i`'s address operand.  A structural constant (`i->konst`) becomes a
// Bemsh literal command: `=в'<octal>'` for an integer (type В = octal; besm_const_word
// already yields the 48-bit word as an octal pattern), `=е'<value>'` for a real (type Е).
//
static void bemsh_operand(char *buf, size_t n, const Besm_Instr *i)
{
    if (i->konst) {
        Besm_ConstWord w = besm_const_word(i->konst);
        if (w.is_real) {
            char num[48];
            bemsh_format_real(num, sizeof(num), w.real_val);
            snprintf(buf, n, "=е'%s'", num);
        } else {
            snprintf(buf, n, "=в'%" PRIo64 "'", w.word);
        }
        return;
    }
    addr_str(buf, n, i->name, i->addr);
}

//
// Emit one Bemsh statement line: label in column 1 (6-wide, blank if none), then the
// mnemonic, then the operand.  The index register (mreg) is appended parenthesized to the
// operand — `уиа масс(13)`, or `(13)` when there is no address.
//
static void emit_line(FILE *out, const char *label, int mreg, const char *mnem,
                      const char *addr)
{
    char operand[80] = "";
    if (addr && addr[0])
        snprintf(operand, sizeof(operand), "%s", addr);
    if (mreg) {
        size_t len = strlen(operand);
        snprintf(operand + len, sizeof(operand) - len, "(%d)", mreg);
    }

    if (label) {
        char sl[8];
        bemsh_sanitize(sl, sizeof(sl), label);
        fprintf(out, "%-6s", sl);
    } else {
        fprintf(out, "      ");
    }
    fprintf(out, " %s", mnem);
    if (operand[0])
        fprintf(out, " %s", operand);
    fputc('\n', out);
}

//
// Emit a BESM_SHAPE_SPECIAL instruction — the kinds whose Bemsh spelling is not a plain
// machine mnemonic + regular operand: UTM (operand suppressed when zero), CALL/BASE, the
// assembler directives, and the data pseudo-ops.
//
static void emit_bemsh_special(FILE *out, const Besm_Instr *instr)
{
    char a[80] = "";
    switch (instr->kind) {
    // Index-register add (stack extension): bare `слиа` when the delta is zero.
    case BESM_REG_UTM:
        if (instr->addr)
            snprintf(a, sizeof(a), "%d", instr->addr);
        emit_line(out, NULL, instr->reg, "слиа", a);
        break;

    // Provisional call rendering — Bemsh has no `call` macro; real call lowering is B3.
    case BESM_BRANCH_CALL:
        bemsh_sanitize(a, sizeof(a), instr->name);
        emit_line(out, NULL, 0, "пв", a);
        break;
    case BESM_STMT_BASE:
        bemsh_sanitize(a, sizeof(a), instr->name);
        emit_line(out, NULL, instr->reg, "употр", a);
        break;

    // Assembly directives — the defined name goes in the label field (column 1).
    case BESM_STMT_LABEL: // define a code label = current address counter (`*`)
        emit_line(out, instr->name, 0, "экв", "*");
        break;
    case BESM_STMT_NAME: // open a named subprogram (auto entry point)
        emit_line(out, instr->name, 0, "старт", "");
        break;
    case BESM_STMT_SUBP: { // external reference — provisional `.label` (search-all) binding
        char ext[8];
        bemsh_sanitize(ext, sizeof(ext), instr->name);
        snprintf(a, sizeof(a), ".%s", ext);
        emit_line(out, instr->name, 0, "внешн", a);
        break;
    }
    case BESM_STMT_ENTRY: // declare an additional entry point
        bemsh_sanitize(a, sizeof(a), instr->name);
        emit_line(out, NULL, 0, "входн", a);
        break;
    case BESM_STMT_END:
        emit_line(out, NULL, 0, "финиш", "");
        break;

    // Data section directives.
    case BESM_DATA_LOG:
        snprintf(a, sizeof(a), "в'%" PRIo64 "'", instr->log_val);
        emit_line(out, instr->name, 0, "конд", a);
        break;
    case BESM_DATA_BSS:
        if (instr->addr)
            snprintf(a, sizeof(a), "%d", instr->addr);
        emit_line(out, instr->name, 0, "пам", a);
        break;
    case BESM_DATA_INT:
        snprintf(a, sizeof(a), "ф'%d'", instr->addr);
        emit_line(out, instr->name, 0, "конд", a);
        break;
    case BESM_DATA_REAL: {
        char num[48];
        bemsh_format_real(num, sizeof(num), instr->real_val);
        snprintf(a, sizeof(a), "е'%s'", num);
        emit_line(out, instr->name, 0, "конд", a);
        break;
    }
    case BESM_DATA_EQU:
        snprintf(a, sizeof(a), "%d", instr->addr);
        emit_line(out, NULL, 0, "экв", a);
        break;
    case BESM_DATA_REF: {
        char ref[8];
        bemsh_sanitize(ref, sizeof(ref), instr->name);
        snprintf(a, sizeof(a), "а(%s)", ref);
        emit_line(out, NULL, 0, "конд", a);
        break;
    }
    case BESM_DATA_STRING: {
        const char *s = instr->name;
        while (*s) {
            snprintf(a, sizeof(a), "ф'%d'", (unsigned char)*s++);
            emit_line(out, NULL, 0, "конд", a);
        }
        emit_line(out, NULL, 0, "конд", "ф'0'");
        break;
    }
    case BESM_DATA_Z00: { // provisional address-constant word (fat-pointer fidelity: B2/B3)
        char body[64] = "";
        addr_str(body, sizeof(body), instr->name, instr->addr);
        snprintf(a, sizeof(a), "а(%s)", body);
        emit_line(out, instr->label, 0, "конд", a);
        break;
    }

    default:
        fatal_error("emit_bemsh_special: unhandled instruction kind %d", (int)instr->kind);
    }
}

static void emit_bemsh_instr(FILE *out, const Besm_Instr *instr)
{
    for (; instr; instr = instr->next) {
        char a[80]       = "";
        Besm_InstrKind k = instr->kind;
        switch (besm_operand_shape(k)) {
        case BESM_SHAPE_MEM:
            bemsh_operand(a, sizeof(a), instr);
            emit_line(out, NULL, instr->reg, besm_cyr_mnem[k], a);
            break;
        case BESM_SHAPE_IMM0:
            snprintf(a, sizeof(a), "%d", instr->addr);
            emit_line(out, NULL, 0, besm_cyr_mnem[k], a);
            break;
        case BESM_SHAPE_IMMR:
            // `уиа` (VTM) carries a symbolic address when it loads a global's address into
            // an index register; otherwise the operand is a plain decimal immediate.
            if (instr->name)
                bemsh_operand(a, sizeof(a), instr);
            else
                snprintf(a, sizeof(a), "%d", instr->addr);
            emit_line(out, NULL, instr->reg, besm_cyr_mnem[k], a);
            break;
        case BESM_SHAPE_NONE:
            emit_line(out, NULL, 0, besm_cyr_mnem[k], "");
            break;
        case BESM_SHAPE_SPECIAL:
            emit_bemsh_special(out, instr);
            break;
        }
    }
}

static void emit_bemsh_block(FILE *out, const Besm_Block *block)
{
    for (; block; block = block->next)
        emit_bemsh_instr(out, block->body);
}

static void emit_bemsh_func(FILE *out, const Besm_Func *func)
{
    for (; func; func = func->next)
        emit_bemsh_block(out, func->blocks);
}

static void emit_bemsh_data_section(FILE *out, const Besm_DataSection *section)
{
    for (; section; section = section->next) {
        emit_line(out, section->name, 0, "старт", "");
        emit_bemsh_instr(out, section->items);
        emit_line(out, NULL, 0, "финиш", "");
    }
}

void emit_bemsh_module(FILE *out, const Besm_Module *module)
{
    // A full-line comment is a `*` in column 1 (Bemsh).
    if (module->comment)
        fprintf(out, "* %s\n", module->comment);
    else
        fprintf(out, "*\n");

    emit_bemsh_func(out, module->funcs);
    emit_bemsh_data_section(out, module->sections);
}
