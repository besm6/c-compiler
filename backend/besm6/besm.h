#ifndef BESM_H
#define BESM_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations for self-referential linked-list nodes
typedef struct Besm_Instr Besm_Instr;
typedef struct Besm_Block Besm_Block;
typedef struct Besm_Func Besm_Func;
typedef struct Besm_DataItem Besm_DataItem;
typedef struct Besm_DataSection Besm_DataSection;

typedef enum {
    // Load/store and index-register transfer
    BESM_MEM_XTA, // XTA(mem_addr addr) -- load A from memory
    BESM_MEM_ATX, // ATX(mem_addr addr) -- store A to memory
    BESM_MEM_STX, // STX(mem_addr addr) -- store A; pop stack
    BESM_MEM_XTS, // XTS(mem_addr addr) -- push A; load from memory
    BESM_MEM_ITA, // ITA(int ireg) -- A = M[ireg]
    BESM_MEM_ATI, // ATI(int ireg) -- M[ireg] = A[15:1]
    BESM_MEM_ITS, // ITS(int ireg) -- push A; A = M[ireg]
    BESM_MEM_STI, // STI(int ireg) -- M[ireg] = A[15:1]; pop
    BESM_MEM_MTJ, // MTJ(mreg src, int dst_j) -- M[dst_j] = M[src]

    // Floating-point arithmetic
    BESM_ARITH_ADD,    // ADD(mem_addr addr)    -- A + X
    BESM_ARITH_SUB,    // SUB(mem_addr addr)    -- A - X
    BESM_ARITH_RSUB,   // RSUB(mem_addr addr)   -- X - A
    BESM_ARITH_ABSSUB, // ABSSUB(mem_addr addr) -- |A| - |X|
    BESM_ARITH_MUL,    // MUL(mem_addr addr)    -- A * X
    BESM_ARITH_DIV,    // DIV(mem_addr addr)    -- A / X
    BESM_ARITH_CNEG,   // CNEG(mem_addr addr)   -- cond. negate A

    // Logical and bit-manipulation
    BESM_LOG_AAX, // AAX(mem_addr addr) -- A & X
    BESM_LOG_AOX, // AOX(mem_addr addr) -- A | X
    BESM_LOG_AEX, // AEX(mem_addr addr) -- Y=A; A ^= X
    BESM_LOG_ARX, // ARX(mem_addr addr) -- A + X end-around
    BESM_LOG_APX, // APX(mem_addr addr) -- pack bits
    BESM_LOG_AUX, // AUX(mem_addr addr) -- unpack bits
    BESM_LOG_ACX, // ACX(mem_addr addr) -- popcount(A) + X
    BESM_LOG_ANX, // ANX(mem_addr addr) -- highest bit pos + X

    // Exponent, shift, and mode-register
    BESM_EXP_EADDX,   // EADDX(mem_addr addr)   -- exponent += X_exp - 64
    BESM_EXP_ESUBX,   // ESUBX(mem_addr addr)   -- exponent -= X_exp - 64
    BESM_EXP_SHIFTX,  // SHIFTX(mem_addr addr)  -- shift by X_exp - 64
    BESM_EXP_SETRMEM, // SETRMEM(mem_addr addr) -- R = X[47:42]
    BESM_EXP_GETR,    // GETR(int mask)         -- A = (R & mask) << 41
    BESM_EXP_YTA,     // YTA(int yoffset)       -- get younger bits
    BESM_EXP_EADDN,   // EADDN(int n_imm)       -- exponent += n_imm - 64
    BESM_EXP_ESUBN,   // ESUBN(int n_imm)       -- exponent -= n_imm - 64
    BESM_EXP_SHIFTN,  // SHIFTN(int n_imm)      -- shift by n_imm - 64
    BESM_EXP_SETR,    // SETR(int value)        -- R = value (0..63)

    // Index-register manipulation
    BESM_REG_VTM,   // VTM(mreg dst, int value)   -- M[dst] = value
    BESM_REG_UTM,   // UTM(mreg dst, int value)   -- M[dst] += value
    BESM_REG_JADDM, // JADDM(mreg src, int dst_j) -- M[dst_j] += M[src]

    // Address modification (C register)
    BESM_MOD_UTC, // UTC(mem_addr addr) -- C = EA (set C immediately)
    BESM_MOD_WTC, // WTC(mem_addr addr) -- C = mem[EA][15:1]

    // Control flow
    BESM_BRANCH_UZA,  // UZA(mem_addr addr)         -- branch if ω = 0
    BESM_BRANCH_U1A,  // U1A(mem_addr addr)         -- branch if ω ≠ 0
    BESM_BRANCH_UJ,   // UJ(mem_addr addr)          -- unconditional jump
    BESM_BRANCH_VJM,  // VJM(mreg link, target tgt) -- call subroutine
    BESM_BRANCH_VZM,  // VZM(mreg test, target tgt) -- branch if M[test] = 0
    BESM_BRANCH_V1M,  // V1M(mreg test, target tgt) -- branch if M[test] ≠ 0
    BESM_BRANCH_VLM,  // VLM(mreg cnt, target tgt)  -- loop (inc. + branch)
    BESM_BRANCH_CALL, // ,call, name -- declare + call external
    BESM_BRANCH_STOP, // STOP                       -- halt processor

    // Assembly directives
    BESM_STMT_LABEL, // name: ,bss,        — label definition point
    BESM_STMT_NAME,  // name: ,name,       — subprogram name
    BESM_STMT_BASE,  //   reg ,base, name  — relocatable basing
    BESM_STMT_SUBP,  // name: ,subp,       — declare external subprogram
    BESM_STMT_ENTRY, //       ,entry, name — export global symbol
    BESM_STMT_END,   //       ,end,        — end of subprogram
} Besm_InstrKind;

struct Besm_Instr {
    struct Besm_Instr *next;
    Besm_InstrKind kind;
    unsigned reg; // index-register 0..15, optional
    int addr;     // offset, optional
    char *name;   // symbolic name, optional (heap-owned)
    // TODO: star plus offset
    // TODO: literal address with value (int, uns, real)
};

//
// block = Block(string? name, instr* body)
//
struct Besm_Block {
    struct Besm_Block *next;
    char *name; // nullable (heap-owned)
    Besm_Instr *body;
};

//
// call_conv
//
typedef enum {
    BESM_CC_BESM6_C,  // BESM6_C: standard Dubna C ABI
    BESM_CC_INTERNAL, // INTERNAL: compiler-internal, no ABI discipline
} Besm_CallConv;

//
// func = Func(string name, call_conv cc, block* blocks)
//
struct Besm_Func {
    struct Besm_Func *next;
    char *name; // heap-owned
    Besm_CallConv cc;
    Besm_Block *blocks;
};

//
// data_item — Madlen data directives (linked list)
//
typedef enum {
    BESM_DATA_INT,    // DI_Int(int value)     -- ,INT, value
    BESM_DATA_REAL,   // DI_Real(double value) -- ,REAL, value
    BESM_DATA_LOG,    // DI_Log(int value)     -- ,LOG, value (logical constant)
    BESM_DATA_BSS,    // DI_Bss(int words)     -- ,BSS, count (zero words)
    BESM_DATA_EQU,    // DI_Equ(int value)     -- ,EQU, value (symbolic constant)
    BESM_DATA_REF,    // DI_Ref(string name)   -- word-wide label reference
    BESM_DATA_STRING, // DI_String(string s)   -- one word per char, null-terminated
} Besm_DataItemKind;

struct Besm_DataItem {
    struct Besm_DataItem *next;
    Besm_DataItemKind kind;
    union {
        int int_val;      // DI_Int
        double real_val;  // DI_Real
        int log_val;      // DI_Log
        int bss_words;    // DI_Bss
        int equ_val;      // DI_Equ
        char *ref_name;   // DI_Ref (heap-owned)
        char *string_val; // DI_String (heap-owned)
    } u;
};

//
// section_kind
//
typedef enum {
    BESM_SK_CODE, // SK_Code: executable subprogram text
    BESM_SK_DATA, // SK_Data: initialised data
    BESM_SK_BSS,  // SK_Bss: zero-initialised storage
} Besm_SectionKind;

//
// data_section = DataSection(section_kind kind, string? name, data_item* items)
//
struct Besm_DataSection {
    struct Besm_DataSection *next;
    Besm_SectionKind kind;
    char *name; // nullable (heap-owned)
    Besm_DataItem *items;
};

//
// module = Module(string name, func* funcs, data_section* sections)
//
typedef struct {
    char *name; // heap-owned
    Besm_Func *funcs;
    Besm_DataSection *sections;
} Besm_Module;

//
// Allocate
//
Besm_Instr *besm_new_instr(Besm_InstrKind kind);
Besm_Block *besm_new_block(void);
Besm_Func *besm_new_func(const char *name, Besm_CallConv cc);
Besm_DataItem *besm_new_data_item(Besm_DataItemKind kind);
Besm_DataSection *besm_new_data_section(Besm_SectionKind kind);
Besm_Module *besm_new_module(const char *name);

//
// Deallocate
//
void besm_free_instr(Besm_Instr *instr);
void besm_free_block(Besm_Block *block);
void besm_free_func(Besm_Func *func);
void besm_free_data_item(Besm_DataItem *item);
void besm_free_data_section(Besm_DataSection *section);
void besm_free_module(Besm_Module *module);

//
// Generate a unique internal label: writes "prefix.N" into buf (N auto-increments).
//
void mad_fresh_label(char *buf, size_t n, const char *prefix);

//
// Emit Madlen assembly
//
void emit_madlen_instr(FILE *out, const Besm_Instr *instr);
void emit_madlen_block(FILE *out, const Besm_Block *block);
void emit_madlen_func(FILE *out, const Besm_Func *func);
void emit_madlen_data_item(FILE *out, const Besm_DataItem *item);
void emit_madlen_data_section(FILE *out, const Besm_DataSection *section);
void emit_madlen_module(FILE *out, const Besm_Module *module);

#ifdef __cplusplus
}
#endif

#endif // BESM_H
