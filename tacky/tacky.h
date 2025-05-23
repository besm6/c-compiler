#ifndef TACKY_H
#define TACKY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

// Forward declarations
typedef struct Tac_Val Tac_Val;
typedef struct Tac_Instruction Tac_Instruction;
typedef struct Tac_Type Tac_Type;
typedef struct Tac_StaticInit Tac_StaticInit;
typedef struct Tac_TopLevel Tac_TopLevel;
typedef struct Tac_Identifier Tac_Identifier;

// Program: TopLevel* decls (linked list)
typedef struct {
    Tac_TopLevel *decls; // Head of TopLevel linked list
} Tac_Program;

// Identifier for identifier* sequences
typedef struct Tac_Identifier {
    struct Tac_Identifier *next; // First field
    char *value;
} Tac_Identifier;

// TopLevel: Function | StaticVariable | StaticConstant
typedef enum {
    TAC_TOPLEVEL_FUNCTION,
    TAC_TOPLEVEL_STATIC_VARIABLE,
    TAC_TOPLEVEL_STATIC_CONSTANT
} Tac_TopLevelKind;

typedef struct Tac_TopLevel {
    struct Tac_TopLevel *next; // First field for linked list
    Tac_TopLevelKind kind;
    union {
        struct {
            char *name;
            bool global;
            Tac_Identifier *params; // Head of identifier linked list
            Tac_Instruction *body;  // Head of Instruction linked list
        } function;
        struct {
            char *name;
            bool global;
            Tac_Type *type;
            Tac_StaticInit *init_list; // Head of StaticInit linked list
        } static_variable;
        struct {
            char *name;
            Tac_Type *type;
            Tac_StaticInit *init;
        } static_constant;
    } u;
} Tac_TopLevel;

// Instruction: Various kinds
typedef enum {
    TAC_INSTRUCTION_RETURN,
    TAC_INSTRUCTION_SIGN_EXTEND,
    TAC_INSTRUCTION_TRUNCATE,
    TAC_INSTRUCTION_ZERO_EXTEND,
    TAC_INSTRUCTION_DOUBLE_TO_INT,
    TAC_INSTRUCTION_DOUBLE_TO_UINT,
    TAC_INSTRUCTION_INT_TO_DOUBLE,
    TAC_INSTRUCTION_UINT_TO_DOUBLE,
    TAC_INSTRUCTION_UNARY,
    TAC_INSTRUCTION_BINARY,
    TAC_INSTRUCTION_COPY,
    TAC_INSTRUCTION_GET_ADDRESS,
    TAC_INSTRUCTION_LOAD,
    TAC_INSTRUCTION_STORE,
    TAC_INSTRUCTION_ADD_PTR,
    TAC_INSTRUCTION_COPY_TO_OFFSET,
    TAC_INSTRUCTION_COPY_FROM_OFFSET,
    TAC_INSTRUCTION_JUMP,
    TAC_INSTRUCTION_JUMP_IF_ZERO,
    TAC_INSTRUCTION_JUMP_IF_NOT_ZERO,
    TAC_INSTRUCTION_LABEL,
    TAC_INSTRUCTION_FUN_CALL
} Tac_InstructionKind;

typedef enum { TAC_UNARY_COMPLEMENT, TAC_UNARY_NEGATE, TAC_UNARY_NOT } Tac_UnaryOperator;

typedef enum {
    TAC_BINARY_ADD,
    TAC_BINARY_SUBTRACT,
    TAC_BINARY_MULTIPLY,
    TAC_BINARY_DIVIDE,
    TAC_BINARY_REMAINDER,
    TAC_BINARY_EQUAL,
    TAC_BINARY_NOT_EQUAL,
    TAC_BINARY_LESS_THAN,
    TAC_BINARY_LESS_OR_EQUAL,
    TAC_BINARY_GREATER_THAN,
    TAC_BINARY_GREATER_OR_EQUAL,
    TAC_BINARY_BITWISE_AND,
    TAC_BINARY_BITWISE_OR,
    TAC_BINARY_BITWISE_XOR,
    TAC_BINARY_LEFT_SHIFT,
    TAC_BINARY_RIGHT_SHIFT
} Tac_BinaryOperator;

typedef struct Tac_Instruction {
    struct Tac_Instruction *next; // First field for linked list
    Tac_InstructionKind kind;
    union {
        struct {
            Tac_Val *src;
        } return_;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
        } sign_extend;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
        } truncate;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
        } zero_extend;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
        } double_to_int;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
        } double_to_uint;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
        } int_to_double;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
        } uint_to_double;
        struct {
            Tac_UnaryOperator op;
            Tac_Val *src;
            Tac_Val *dst;
        } unary;
        struct {
            Tac_BinaryOperator op;
            Tac_Val *src1;
            Tac_Val *src2;
            Tac_Val *dst;
        } binary;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
        } copy;
        struct {
            Tac_Val *src;
            Tac_Val *dst;
        } get_address;
        struct {
            Tac_Val *src_ptr;
            Tac_Val *dst;
        } load;
        struct {
            Tac_Val *src;
            Tac_Val *dst_ptr;
        } store;
        struct {
            Tac_Val *ptr;
            Tac_Val *index;
            int scale;
            Tac_Val *dst;
        } add_ptr;
        struct {
            Tac_Val *src;
            char *dst;
            int offset;
        } copy_to_offset;
        struct {
            char *src;
            int offset;
            Tac_Val *dst;
        } copy_from_offset;
        struct {
            char *target;
        } jump;
        struct {
            Tac_Val *condition;
            char *target;
        } jump_if_zero;
        struct {
            Tac_Val *condition;
            char *target;
        } jump_if_not_zero;
        struct {
            char *name;
        } label;
        struct {
            char *fun_name;
            Tac_Val *args;
            Tac_Val *dst;
        } fun_call; // args is head of Val linked list
    } u;
} Tac_Instruction;

// Val: Constant | Var
typedef enum { TAC_VAL_CONSTANT, TAC_VAL_VAR } Tac_ValKind;

typedef struct Tac_Val {
    struct Tac_Val *next; // First field for linked list
    Tac_ValKind kind;
    union {
        struct Tac_Const *constant;
        char *var_name;
    } u;
} Tac_Val;

// Const: Various constant types
typedef enum {
    TAC_CONST_INT,
    TAC_CONST_LONG,
    TAC_CONST_UINT,
    TAC_CONST_ULONG,
    TAC_CONST_DOUBLE,
    TAC_CONST_CHAR,
    TAC_CONST_UCHAR
} Tac_ConstKind;

typedef struct Tac_Const {
    Tac_ConstKind kind;
    union {
        int int_val;
        long long_val;
        unsigned int uint_val;
        unsigned long ulong_val;
        double double_val;
        int char_val;
        unsigned char uchar_val;
    } u;
} Tac_Const;

// Type: Various type kinds
typedef enum {
    TAC_TYPE_CHAR,
    TAC_TYPE_SCHAR,
    TAC_TYPE_UCHAR,
    TAC_TYPE_INT,
    TAC_TYPE_LONG,
    TAC_TYPE_UINT,
    TAC_TYPE_ULONG,
    TAC_TYPE_DOUBLE,
    TAC_TYPE_VOID,
    TAC_TYPE_FUN_TYPE,
    TAC_TYPE_POINTER,
    TAC_TYPE_ARRAY,
    TAC_TYPE_STRUCTURE
} Tac_TypeKind;

typedef struct Tac_Type {
    struct Tac_Type *next; // First field for linked list
    Tac_TypeKind kind;
    union {
        struct {
            Tac_Type *params;
            Tac_Type *ret;
        } fun_type; // params is head of Type linked list
        struct {
            Tac_Type *referenced;
        } pointer;
        struct {
            Tac_Type *element;
            int size;
        } array;
        struct {
            char *tag;
        } structure;
    } u;
} Tac_Type;

// StaticInit: Various initialization kinds
typedef enum {
    TAC_STATIC_INIT_INT,
    TAC_STATIC_INIT_LONG,
    TAC_STATIC_INIT_UINT,
    TAC_STATIC_INIT_ULONG,
    TAC_STATIC_INIT_CHAR,
    TAC_STATIC_INIT_UCHAR,
    TAC_STATIC_INIT_DOUBLE,
    TAC_STATIC_INIT_ZERO,
    TAC_STATIC_INIT_STRING,
    TAC_STATIC_INIT_POINTER
} Tac_StaticInitKind;

typedef struct Tac_StaticInit {
    struct Tac_StaticInit *next; // First field for linked list
    Tac_StaticInitKind kind;
    union {
        int int_val;
        long long_val;
        unsigned int uint_val;
        unsigned long ulong_val;
        int char_val;
        unsigned char uchar_val;
        double double_val;
        int zero_bytes;
        struct {
            char *val;
            bool null_terminated;
        } string;
        char *pointer_name;
    } u;
} Tac_StaticInit;

#ifdef __cplusplus
}
#endif

#endif // TACKY_H
