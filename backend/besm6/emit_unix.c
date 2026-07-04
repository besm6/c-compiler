#include <stdio.h>
#include <string.h>

#include "abi.h"
#include "besm.h"
#include "internal.h"

//
// Unix (b6as) assembler emitter.  Renders the dialect-agnostic Besm_Module in the
// AT&T-style syntax accepted by b6as (docs/Besm6_Unix_Assembler.md): segment directives
// (.text/.data/.bss), a `[modreg] mnem operand` line format, `#`-pool constant operands,
// and a *translation* of the Madlen externals/relocation model into b6as's .globl/label/
// .word model.  The four regular operand shapes and the mnemonic table are shared with the
// Madlen emitter (besm_mnem.c); only the framing, the name sanitizer, and the special/
// directive handling differ.
//
// Runtime-helper names are canonical `$` in the IR (b$ret, b$save, …); b6as accepts `$` in
// names, so they pass through unchanged (whereas the Madlen emitter lowers `$`→`/`).
//

// The current output segment; b6as starts in .text.  A segment directive is emitted only
// when the segment must change (set_segment).
typedef enum {
    SEG_NONE,
    SEG_TEXT,
    SEG_DATA,
    SEG_BSS,
} SegKind;

static void set_segment(FILE *out, SegKind *cur, SegKind want)
{
    if (*cur == want)
        return;
    *cur = want;
    switch (want) {
    case SEG_TEXT:
        fprintf(out, "\t.text\n");
        break;
    case SEG_DATA:
        fprintf(out, "\t.data\n");
        break;
    case SEG_BSS:
        fprintf(out, "\t.bss\n");
        break;
    case SEG_NONE:
        break;
    }
}

// Sanitize a b6as identifier.  b6as names may contain letters, digits, `_`, `.`, and `$`.
// Map `%`→`.` (compiler-generated labels, which b6as strips with -X) and, defensively,
// `/`→`$` (no `/` should reach here once helper names are canonical `$`).  No truncation.
static void unix_sanitize(char *dst, size_t n, const char *src)
{
    size_t i = 0;
    for (; *src && i + 1 < n; src++, i++) {
        char c = *src;
        if (c == '%')
            c = '.';
        else if (c == '/')
            c = '$';
        dst[i] = c;
    }
    dst[i] = '\0';
}

// Build the address-field string from (name, addr): "name+N" / "name-N" / "name" / "N" /
// "" (empty when both are zero/null; buf must be pre-zeroed by the caller).
static void unix_addr(char *buf, size_t n, const char *name, int addr)
{
    char sname[64] = "";
    if (name)
        unix_sanitize(sname, sizeof(sname), name);
    if (name && addr > 0)
        snprintf(buf, n, "%s+%d", sname, addr);
    else if (name && addr < 0)
        snprintf(buf, n, "%s-%d", sname, -addr);
    else if (name)
        snprintf(buf, n, "%s", sname);
    else if (addr)
        snprintf(buf, n, "%d", addr);
}

// Format instruction `i`'s address operand.  A structural constant (`i->konst`) becomes a
// b6as `#`-pool operand: `#0octal` for an integer, `#<real>` for a floating value (the `#`
// operator pools+deduplicates the word in the const segment).  Otherwise the (name, addr)
// pair is rendered by unix_addr.
static void unix_operand(char *buf, size_t n, const Besm_Instr *i)
{
    if (i->konst) {
        Besm_ConstWord w = besm_const_word(i->konst);
        if (w.is_real) {
            char num[48];
            mad_format_real(num, sizeof(num), w.real_val);
            snprintf(buf, n, "#%s", num);
        } else {
            snprintf(buf, n, "#0%llo", w.word);
        }
        return;
    }
    unix_addr(buf, n, i->name, i->addr);
}

// Emit a label definition at column 0: `name:`.
static void emit_ulabel(FILE *out, const char *name)
{
    char s[64];
    unix_sanitize(s, sizeof(s), name);
    fprintf(out, "%s:\n", s);
}

// Emit a `.globl name` directive.
static void emit_uglobl(FILE *out, const char *name)
{
    char s[64];
    unix_sanitize(s, sizeof(s), name);
    fprintf(out, "\t.globl %s\n", s);
}

// Emit a tab-indented directive: `.dir operand` (or bare `.dir` when operand is empty).
static void emit_udir(FILE *out, const char *dir, const char *operand)
{
    if (operand && operand[0])
        fprintf(out, "\t%s %s\n", dir, operand);
    else
        fprintf(out, "\t%s\n", dir);
}

// Emit one tab-indented instruction line: `[modreg ]mnem[ operand]`.
static void emit_uinstr(FILE *out, int mreg, const char *mnem, const char *operand)
{
    fputc('\t', out);
    if (mreg)
        fprintf(out, "%d ", mreg);
    fputs(mnem, out);
    if (operand && operand[0])
        fprintf(out, " %s", operand);
    fputc('\n', out);
}

// Emit a Z00 pointer-init word, consuming the z00a/z00b pair.  `z00a` is the high half
// (carries the optional datum `label` and, for a fat pointer, a KOI-7 byte-offset marker in
// `reg`); the following `z00b` carries the address (`name`+word offset in `addr`).  A plain
// pointer (z00a.reg == 0) coalesces into one relocatable `.word name+off`; a fat pointer
// emits the raw long opcode `@00` per half.  Returns the last node consumed (z00b) so the
// caller's loop advances past it.
static const Besm_Instr *emit_unix_z00(FILE *out, const Besm_Instr *z00a, SegKind *cur)
{
    set_segment(out, cur, SEG_DATA);
    const Besm_Instr *z00b = z00a->next;
    char a[64] = "";
    if (z00b)
        unix_addr(a, sizeof(a), z00b->name, z00b->addr);
    if (z00a->label)
        emit_ulabel(out, z00a->label);
    if (z00a->reg == 0) {
        // Plain pointer: high half is zero → single relocatable .word.
        emit_udir(out, ".word", a);
    } else {
        // Fat pointer: byte-offset marker rides the leading modreg of the high half.
        emit_uinstr(out, (int)z00a->reg, "@00", "");
        emit_uinstr(out, 0, "@00", a);
    }
    return z00b ? z00b : z00a;
}

// Emit a BESM_SHAPE_SPECIAL instruction — the UTM/CALL irregular machine ops, the assembler
// directives, and the data pseudo-ops.  Externals/relocation are translated (not
// transliterated) into the b6as model.  Returns the last node consumed (normally `instr`;
// for a Z00 pair, its z00b) so the caller advances correctly.
static const Besm_Instr *emit_unix_special(FILE *out, const Besm_Instr *instr, SegKind *cur)
{
    char a[64] = "";
    switch (instr->kind) {
    // Index-register add: bare `utm` when the delta is zero.
    case BESM_REG_UTM:
        set_segment(out, cur, SEG_TEXT);
        if (instr->addr)
            snprintf(a, sizeof(a), "%d", instr->addr);
        emit_uinstr(out, (int)instr->reg, "utm", a);
        break;

    // A direct call is `13 vjm name` (,call, ≡ 13 ,vjm,, link register r13); b6as
    // auto-declares the undefined callee as external, so no .globl is needed.
    case BESM_BRANCH_CALL:
        set_segment(out, cur, SEG_TEXT);
        unix_sanitize(a, sizeof(a), instr->name);
        emit_uinstr(out, REG_RET, "vjm", a);
        break;

    // Subprogram name / secondary entry → export the symbol and define the label.
    case BESM_STMT_NAME:
        set_segment(out, cur, SEG_TEXT);
        emit_uglobl(out, instr->name);
        emit_ulabel(out, instr->name);
        break;
    case BESM_STMT_ENTRY:
        emit_uglobl(out, instr->name);
        emit_ulabel(out, instr->name);
        break;
    case BESM_STMT_LABEL:
        emit_ulabel(out, instr->name);
        break;

    // SUBP (external declaration), END (subprogram terminator), and BASE (relocatable
    // basing) have no b6as equivalent — undefined names auto-extern, segments are implicit,
    // and relocation is handled by the linker.  Drop them.
    case BESM_STMT_SUBP:
    case BESM_STMT_END:
    case BESM_STMT_BASE:
        break;

    // Data words.
    case BESM_DATA_LOG:
        set_segment(out, cur, SEG_DATA);
        if (instr->name)
            emit_ulabel(out, instr->name);
        snprintf(a, sizeof(a), "0%llo", instr->log_val);
        emit_udir(out, ".word", a);
        break;
    case BESM_DATA_REAL:
        set_segment(out, cur, SEG_DATA);
        if (instr->name)
            emit_ulabel(out, instr->name);
        mad_format_real(a, sizeof(a), instr->real_val);
        emit_udir(out, ".word", a);
        break;
    case BESM_DATA_INT:
        set_segment(out, cur, SEG_DATA);
        if (instr->name)
            emit_ulabel(out, instr->name);
        snprintf(a, sizeof(a), "%d", instr->addr);
        emit_udir(out, ".word", a);
        break;
    case BESM_DATA_BSS:
        set_segment(out, cur, SEG_BSS);
        if (instr->name)
            emit_ulabel(out, instr->name);
        if (instr->addr)
            fprintf(out, "\t. = . + %d\n", instr->addr);
        break;
    case BESM_DATA_Z00:
        return emit_unix_z00(out, instr, cur);

    default:
        fatal_error("emit_unix_special: unhandled instruction kind %d", (int)instr->kind);
    }
    return instr;
}

static void emit_unix_instr(FILE *out, const Besm_Instr *instr, SegKind *cur)
{
    while (instr) {
        char a[64]       = "";
        Besm_InstrKind k = instr->kind;
        switch (besm_operand_shape(k)) {
        case BESM_SHAPE_MEM:
            set_segment(out, cur, SEG_TEXT);
            unix_operand(a, sizeof(a), instr);
            emit_uinstr(out, (int)instr->reg, besm_latin_mnem[k], a);
            break;
        case BESM_SHAPE_IMM0:
            set_segment(out, cur, SEG_TEXT);
            snprintf(a, sizeof(a), "%d", instr->addr);
            emit_uinstr(out, 0, besm_latin_mnem[k], a);
            break;
        case BESM_SHAPE_IMMR:
            set_segment(out, cur, SEG_TEXT);
            snprintf(a, sizeof(a), "%d", instr->addr);
            emit_uinstr(out, (int)instr->reg, besm_latin_mnem[k], a);
            break;
        case BESM_SHAPE_NONE:
            set_segment(out, cur, SEG_TEXT);
            emit_uinstr(out, 0, besm_latin_mnem[k], "");
            break;
        case BESM_SHAPE_SPECIAL:
            instr = emit_unix_special(out, instr, cur);
            break;
        }
        instr = instr->next;
    }
}

static void emit_unix_func(FILE *out, const Besm_Func *func, SegKind *cur)
{
    for (; func; func = func->next)
        for (const Besm_Block *block = func->blocks; block; block = block->next)
            emit_unix_instr(out, block->body, cur);
}

static void emit_unix_data_section(FILE *out, const Besm_DataSection *section, SegKind *cur)
{
    for (; section; section = section->next) {
        SegKind seg = section->kind == BESM_SK_BSS    ? SEG_BSS
                      : section->kind == BESM_SK_CODE ? SEG_TEXT
                                                      : SEG_DATA;
        set_segment(out, cur, seg);
        if (section->name) {
            emit_uglobl(out, section->name);
            emit_ulabel(out, section->name);
        }
        emit_unix_instr(out, section->items, cur);
    }
}

void emit_unix_module(FILE *out, const Besm_Module *module)
{
    if (module->comment)
        fprintf(out, "// %s\n", module->comment);

    SegKind cur = SEG_NONE;
    emit_unix_func(out, module->funcs, &cur);
    emit_unix_data_section(out, module->sections, &cur);
}
