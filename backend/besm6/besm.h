#ifndef BESM_H
#define BESM_H

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations for self-referential linked-list nodes
typedef struct Besm_Instr Besm_Instr;
typedef struct Besm_Block Besm_Block;
typedef struct Besm_Func Besm_Func;
typedef struct Besm_DataItem Besm_DataItem;
typedef struct Besm_DataSection Besm_DataSection;

//
// mreg = MReg(int num)   -- num ∈ 0..15
//
typedef struct {
    int num;
} Besm_MReg;

//
// mem_addr — memory address operand (Format 1 / Format 2)
//
typedef enum {
    BESM_MEM_ADDR_REG,       // AddrReg(mreg reg, int offset)
    BESM_MEM_ADDR_LABEL,     // AddrLabel(mreg reg, string name)
    BESM_MEM_ADDR_SEG_REG,   // AddrSegReg(mreg reg, int offset)  -- S bit set
    BESM_MEM_ADDR_SEG_LABEL, // AddrSegLabel(mreg reg, string name) -- S bit set
} Besm_MemAddrKind;

typedef struct {
    Besm_MemAddrKind kind;
    Besm_MReg reg;
    union {
        int offset;  // AddrReg, AddrSegReg
        char *name;  // AddrLabel, AddrSegLabel (heap-owned)
    } u;
} Besm_MemAddr;

//
// target — branch / call target for VJM, VZM, V1M, VLM
//
typedef enum {
    BESM_TARGET_LABEL,  // TgtLabel(string name)
    BESM_TARGET_OFFSET, // TgtOffset(int offset)
} Besm_TargetKind;

typedef struct {
    Besm_TargetKind kind;
    union {
        char *name;  // TgtLabel (heap-owned)
        int offset;  // TgtOffset; 15-bit unsigned word address
    } u;
} Besm_Target;

//
// mem_instr — load/store and index-register transfer
//
typedef enum {
    BESM_MEM_XTA, // XTA(mem_addr addr) -- load A from memory
    BESM_MEM_ATX, // ATX(mem_addr addr) -- store A to memory
    BESM_MEM_STX, // STX(mem_addr addr) -- store A; pop stack
    BESM_MEM_XTS, // XTS(mem_addr addr) -- push A; load from memory
    BESM_MEM_ITA, // ITA(int ireg) -- A = M[ireg]
    BESM_MEM_ATI, // ATI(int ireg) -- M[ireg] = A[15:1]
    BESM_MEM_ITS, // ITS(int ireg) -- push A; A = M[ireg]
    BESM_MEM_STI, // STI(int ireg) -- M[ireg] = A[15:1]; pop
    BESM_MEM_MTJ, // MTJ(mreg src, int dst_j) -- M[dst_j] = M[src]
} Besm_MemInstrKind;

typedef struct {
    Besm_MemInstrKind kind;
    union {
        Besm_MemAddr addr; // XTA, ATX, STX, XTS
        int ireg;          // ITA, ATI, ITS, STI; ireg ∈ 0..15
        struct {
            Besm_MReg src;
            int dst_j;
        } mtj;
    } u;
} Besm_MemInstr;

//
// arith_instr — floating-point arithmetic
//
typedef enum {
    BESM_ARITH_ADD,    // ADD(mem_addr addr)    -- A + X
    BESM_ARITH_SUB,    // SUB(mem_addr addr)    -- A - X
    BESM_ARITH_RSUB,   // RSUB(mem_addr addr)   -- X - A
    BESM_ARITH_ABSSUB, // ABSSUB(mem_addr addr) -- |A| - |X|
    BESM_ARITH_MUL,    // MUL(mem_addr addr)    -- A * X
    BESM_ARITH_DIV,    // DIV(mem_addr addr)    -- A / X
    BESM_ARITH_CNEG,   // CNEG(mem_addr addr)   -- cond. negate A
} Besm_ArithInstrKind;

typedef struct {
    Besm_ArithInstrKind kind;
    Besm_MemAddr addr;
} Besm_ArithInstr;

//
// log_instr — logical and bit-manipulation
//
typedef enum {
    BESM_LOG_AAX, // AAX(mem_addr addr) -- A & X
    BESM_LOG_AOX, // AOX(mem_addr addr) -- A | X
    BESM_LOG_AEX, // AEX(mem_addr addr) -- Y=A; A ^= X
    BESM_LOG_ARX, // ARX(mem_addr addr) -- A + X end-around
    BESM_LOG_APX, // APX(mem_addr addr) -- pack bits
    BESM_LOG_AUX, // AUX(mem_addr addr) -- unpack bits
    BESM_LOG_ACX, // ACX(mem_addr addr) -- popcount(A) + X
    BESM_LOG_ANX, // ANX(mem_addr addr) -- highest bit pos + X
} Besm_LogInstrKind;

typedef struct {
    Besm_LogInstrKind kind;
    Besm_MemAddr addr;
} Besm_LogInstr;

//
// exp_instr — exponent, shift, and mode-register
//
typedef enum {
    BESM_EXP_EADDX,   // EADDX(mem_addr addr)  -- exponent += X_exp - 64
    BESM_EXP_ESUBX,   // ESUBX(mem_addr addr)  -- exponent -= X_exp - 64
    BESM_EXP_SHIFTX,  // SHIFTX(mem_addr addr) -- shift by X_exp - 64
    BESM_EXP_SETRMEM, // SETRMEM(mem_addr addr) -- R = X[47:42]
    BESM_EXP_GETR,    // GETR(int mask)        -- A = (R & mask) << 41
    BESM_EXP_YTA,     // YTA(int yoffset)      -- get younger bits
    BESM_EXP_EADDN,   // EADDN(int n_imm)      -- exponent += n_imm - 64
    BESM_EXP_ESUBN,   // ESUBN(int n_imm)      -- exponent -= n_imm - 64
    BESM_EXP_SHIFTN,  // SHIFTN(int n_imm)     -- shift by n_imm - 64
    BESM_EXP_SETR,    // SETR(int value)       -- R = value (0..63)
} Besm_ExpInstrKind;

typedef struct {
    Besm_ExpInstrKind kind;
    union {
        Besm_MemAddr addr; // EADDX, ESUBX, SHIFTX, SETRMEM
        int imm;           // GETR(mask), YTA(yoffset), EADDN/ESUBN/SHIFTN(n_imm), SETR(value)
    } u;
} Besm_ExpInstr;

//
// reg_instr — index-register manipulation
//
typedef enum {
    BESM_REG_VTM,   // VTM(mreg dst, int value)       -- M[dst] = value
    BESM_REG_UTM,   // UTM(mreg dst, int value)       -- M[dst] += value
    BESM_REG_JADDM, // JADDM(mreg src, int dst_j)    -- M[dst_j] += M[src]
} Besm_RegInstrKind;

typedef struct {
    Besm_RegInstrKind kind;
    union {
        struct {
            Besm_MReg dst;
            int value;
        } vtm; // VTM, UTM
        struct {
            Besm_MReg src;
            int dst_j;
        } jaddm;
    } u;
} Besm_RegInstr;

//
// mod_instr — C register (UTC / WTC)
//
typedef enum {
    BESM_MOD_UTC, // UTC(mem_addr addr) -- C = EA (set C immediately)
    BESM_MOD_WTC, // WTC(mem_addr addr) -- C = mem[EA][15:1]
} Besm_ModInstrKind;

typedef struct {
    Besm_ModInstrKind kind;
    Besm_MemAddr addr;
} Besm_ModInstr;

//
// branch_instr — control flow
//
typedef enum {
    BESM_BRANCH_UZA,  // UZA(mem_addr addr)            -- branch if ω = 0
    BESM_BRANCH_U1A,  // U1A(mem_addr addr)            -- branch if ω ≠ 0
    BESM_BRANCH_UJ,   // UJ(mem_addr addr)             -- unconditional jump
    BESM_BRANCH_VJM,  // VJM(mreg link, target tgt)   -- call subroutine
    BESM_BRANCH_VZM,  // VZM(mreg test, target tgt)   -- branch if M[test] = 0
    BESM_BRANCH_V1M,  // V1M(mreg test, target tgt)   -- branch if M[test] ≠ 0
    BESM_BRANCH_VLM,  // VLM(mreg cnt, target tgt)    -- loop (inc. + branch)
    BESM_BRANCH_STOP, // STOP                          -- halt processor
} Besm_BranchInstrKind;

typedef struct {
    Besm_BranchInstrKind kind;
    union {
        Besm_MemAddr addr; // UZA, U1A, UJ
        struct {
            Besm_MReg reg; // link/test/cnt register
            Besm_Target tgt;
        } jump; // VJM, VZM, V1M, VLM
        // STOP: no payload
    } u;
} Besm_BranchInstr;

//
// extra_instr — extracodes (system calls and math library)
//
typedef enum {
    BESM_EXTRA_ESQRT,  // ESQRT(mem_addr addr)  -- sqrt(A)
    BESM_EXTRA_ESIN,   // ESIN(mem_addr addr)   -- sin(A)
    BESM_EXTRA_ECOS,   // ECOS(mem_addr addr)   -- cos(A)
    BESM_EXTRA_EATAN,  // EATAN(mem_addr addr)  -- atan(A)
    BESM_EXTRA_EASIN,  // EASIN(mem_addr addr)  -- asin(A)
    BESM_EXTRA_ELN,    // ELN(mem_addr addr)    -- ln(A)
    BESM_EXTRA_EEXP,   // EEXP(mem_addr addr)   -- e^A
    BESM_EXTRA_ETAPE,  // ETAPE(mem_addr addr)  -- tape/file I/O
    BESM_EXTRA_EIN,    // EIN(mem_addr addr)    -- I/O input control
    BESM_EXTRA_EOUT,   // EOUT(mem_addr addr)   -- I/O output control
    BESM_EXTRA_ETXTIO, // ETXTIO(mem_addr addr) -- text I/O
    BESM_EXTRA_EPRINT, // EPRINT(mem_addr addr) -- formatted print
    BESM_EXTRA_ETIME,  // ETIME(mem_addr addr)  -- CPU time (1/50 s)
    BESM_EXTRA_ETIMEH, // ETIMEH(mem_addr addr) -- CPU time (µs)
    BESM_EXTRA_ESYS,   // ESYS(int opcode, mem_addr addr) -- other extracodes
} Besm_ExtraInstrKind;

typedef struct {
    Besm_ExtraInstrKind kind;
    union {
        Besm_MemAddr addr; // all except ESYS
        struct {
            int opcode;
            Besm_MemAddr addr;
        } esys;
    } u;
} Besm_ExtraInstr;

//
// instr — top-level instruction wrapper (linked list)
//
typedef enum {
    BESM_INSTR_MEM,    // IMem(mem_instr body)
    BESM_INSTR_ARITH,  // IArith(arith_instr body)
    BESM_INSTR_LOG,    // ILog(log_instr body)
    BESM_INSTR_EXP,    // IExp(exp_instr body)
    BESM_INSTR_REG,    // IReg(reg_instr body)
    BESM_INSTR_MOD,    // IMOD(mod_instr body)
    BESM_INSTR_BRANCH, // IBranch(branch_instr body)
    BESM_INSTR_EXTRA,  // IExtra(extra_instr body)
    BESM_INSTR_LABEL,  // ILabel(string name)
    BESM_INSTR_NAME,   // IName(string name)
    BESM_INSTR_REL,    // IRel  (no payload)
    BESM_INSTR_CALL,   // ICall(string name)
    BESM_INSTR_ENTRY,  // IEntry(string name)
    BESM_INSTR_END,    // IEnd  (no payload)
} Besm_InstrKind;

struct Besm_Instr {
    struct Besm_Instr *next;
    Besm_InstrKind kind;
    union {
        Besm_MemInstr mem;
        Besm_ArithInstr arith;
        Besm_LogInstr log_;
        Besm_ExpInstr exp;
        Besm_RegInstr reg;
        Besm_ModInstr mod;
        Besm_BranchInstr branch;
        Besm_ExtraInstr extra;
        char *name; // LABEL, NAME, CALL, ENTRY (heap-owned)
        // REL, END: no payload
    } u;
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
    BESM_DATA_INT,    // DI_Int(int value)    -- ,INT, value
    BESM_DATA_REAL,   // DI_Real(float value) -- ,REAL, value
    BESM_DATA_OCT,    // DI_Oct(int value)    -- ,OCT, value (raw 48-bit octal)
    BESM_DATA_LOG,    // DI_Log(int value)    -- ,LOG, value (logical constant)
    BESM_DATA_BSS,    // DI_Bss(int words)    -- ,BSS, count (zero words)
    BESM_DATA_EQU,    // DI_Equ(int value)    -- ,EQU, value (symbolic constant)
    BESM_DATA_REF,    // DI_Ref(string name)  -- word-wide label reference
    BESM_DATA_STRING, // DI_String(string s)  -- one word per char, null-terminated
} Besm_DataItemKind;

struct Besm_DataItem {
    struct Besm_DataItem *next;
    Besm_DataItemKind kind;
    union {
        int int_val;      // DI_Int
        float real_val;   // DI_Real
        int oct_val;      // DI_Oct
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
Besm_Func *besm_new_func(void);
Besm_DataItem *besm_new_data_item(Besm_DataItemKind kind);
Besm_DataSection *besm_new_data_section(Besm_SectionKind kind);
Besm_Module *besm_new_module(void);

//
// Deallocate
//
void besm_free_instr(Besm_Instr *instr);
void besm_free_block(Besm_Block *block);
void besm_free_func(Besm_Func *func);
void besm_free_data_item(Besm_DataItem *item);
void besm_free_data_section(Besm_DataSection *section);
void besm_free_module(Besm_Module *module);

#ifdef __cplusplus
}
#endif

#endif // BESM_H
