#include <string.h>

#include "ast.h"
#include "internal.h"

// Helper function to print indentation
static void print_indent(FILE *fd, int indent)
{
    fprintf(fd, "%*s", indent, "");
}

// Enum-to-string mappings
static const char *expr_kind_str[] = { [EXPR_VAR]         = "Variable",
                                       [EXPR_LITERAL]     = "Literal",
                                       [EXPR_BINARY_OP]   = "BinaryOp",
                                       [EXPR_UNARY_OP]    = "UnaryOp",
                                       [EXPR_POST_INC]    = "PostIncrement",
                                       [EXPR_POST_DEC]    = "PostDecrement",
                                       [EXPR_CALL]        = "Call",
                                       [EXPR_CAST]        = "Cast",
                                       [EXPR_COMPOUND]    = "Compound",
                                       [EXPR_SIZEOF_EXPR] = "SizeofExpr",
                                       [EXPR_SIZEOF_TYPE] = "SizeofType",
                                       [EXPR_ALIGNOF]     = "Alignof",
                                       [EXPR_GENERIC]     = "Generic" };

static const char *stmt_kind_str[] = { [STMT_EXPR] = "Expression",  [STMT_IF] = "If",
                                       [STMT_SWITCH] = "Switch",    [STMT_WHILE] = "While",
                                       [STMT_DO_WHILE] = "DoWhile", [STMT_FOR] = "For",
                                       [STMT_GOTO] = "Goto",        [STMT_CONTINUE] = "Continue",
                                       [STMT_BREAK] = "Break",      [STMT_RETURN] = "Return",
                                       [STMT_LABELED] = "Labeled",  [STMT_CASE] = "Case",
                                       [STMT_DEFAULT] = "Default",  [STMT_COMPOUND] = "Compound" };

const char *type_kind_str[] = {
    [TYPE_VOID] = "void",
    [TYPE_BOOL] = "_Bool",
    [TYPE_CHAR] = "char",
    [TYPE_SHORT] = "short",
    [TYPE_INT] = "int",
    [TYPE_LONG] = "long",
    [TYPE_LONG_LONG] = "long long",
    [TYPE_SIGNED] = "signed",
    [TYPE_UNSIGNED] = "unsigned",
    [TYPE_FLOAT] = "float",
    [TYPE_DOUBLE] = "double",
    [TYPE_LONG_DOUBLE] = "long double",
    [TYPE_COMPLEX] = "_Complex",
    [TYPE_IMAGINARY] = "_Imaginary",
    [TYPE_POINTER] = "ptr",
    [TYPE_ARRAY] = "array",
    [TYPE_FUNCTION] = "func",
    [TYPE_STRUCT] = "struct",
    [TYPE_UNION] = "union",
    [TYPE_ENUM] = "enum",
    [TYPE_TYPEDEF_NAME] = "typedef",
    [TYPE_ATOMIC] = "_Atomic",
};

static const char *binary_op_kind_str[] = {
    [BINARY_MUL] = "*",          [BINARY_DIV] = "/",      [BINARY_MOD] = "%",
    [BINARY_ADD] = "+",          [BINARY_SUB] = "-",      [BINARY_LEFT_SHIFT] = "<<",
    [BINARY_RIGHT_SHIFT] = ">>", [BINARY_LT] = "<",       [BINARY_GT] = ">",
    [BINARY_LE] = "<=",          [BINARY_GE] = ">=",      [BINARY_EQ] = "==",
    [BINARY_NE] = "!=",          [BINARY_BIT_AND] = "&",  [BINARY_BIT_XOR] = "^",
    [BINARY_BIT_OR] = "|",       [BINARY_LOG_AND] = "&&", [BINARY_LOG_OR] = "||"
};

static const char *assign_op_kind_str[] = {
    [ASSIGN_SIMPLE] = "=", [ASSIGN_MUL] = "*=", [ASSIGN_DIV] = "/=",   [ASSIGN_MOD] = "%=",
    [ASSIGN_ADD] = "+=",   [ASSIGN_SUB] = "-=", [ASSIGN_LEFT] = "<<=", [ASSIGN_RIGHT] = ">>=",
    [ASSIGN_AND] = "&=",   [ASSIGN_XOR] = "^=", [ASSIGN_OR] = "|=",
};

static const char *unary_op_kind_str[] = {
    [UNARY_ADDRESS] = "&", [UNARY_DEREF] = "*",   [UNARY_PLUS] = "+",     [UNARY_NEG] = "-",
    [UNARY_BIT_NOT] = "~", [UNARY_LOG_NOT] = "!", [UNARY_PRE_INC] = "++", [UNARY_PRE_DEC] = "--",
};

// Forward declarations
void print_expression(FILE *fd, Expr *expr, int indent);
void print_statement(FILE *fd, Stmt *stmt, int indent);
static void print_declaration(FILE *fd, Declaration *decl, int indent);
static void print_external_decl(FILE *fd, ExternalDecl *ext, int indent);
static void print_decl_spec(FILE *fd, DeclSpec *spec, int indent);

// Print Field structure
void print_field(FILE *fd, Field *field, int indent)
{
    print_indent(fd, indent);
    if (!field) {
        fprintf(fd, "Field: NULL\n");
        return;
    }
    fprintf(fd, "Field: %s\n", field->name ? field->name : "(anonymous)");
    print_type(fd, field->type, indent + 2);
    if (field->bitfield) {
        print_indent(fd, indent + 2);
        fprintf(fd, "Bitfield:\n");
        print_expression(fd, field->bitfield, indent + 4);
    }
}

// Print Param structure
void print_param(FILE *fd, Param *params, int indent)
{
    if (!params) {
        print_indent(fd, indent);
        fprintf(fd, "Param: NULL\n");
        return;
    }

    for (Param *p = params; p; p = p->next) {
        print_indent(fd, indent);
        fprintf(fd, "Param: %s\n", p->name ? p->name : "(no name)");
        print_type(fd, p->type, indent + 4);
        print_decl_spec(fd, p->specifiers, indent + 4);
    }
}

// Print TypeQualifier list
void print_type_qualifiers(FILE *fd, TypeQualifier *qualifiers, int indent)
{
    if (!qualifiers)
        return;
    print_indent(fd, indent);
    fprintf(fd, "Qualifiers:\n");
    for (TypeQualifier *q = qualifiers; q; q = q->next) {
        print_indent(fd, indent + 2);
        switch (q->kind) {
        case TYPE_QUALIFIER_CONST:
            fprintf(fd, "const ");
            break;
        case TYPE_QUALIFIER_RESTRICT:
            fprintf(fd, "restrict ");
            break;
        case TYPE_QUALIFIER_VOLATILE:
            fprintf(fd, "volatile ");
            break;
        case TYPE_QUALIFIER_ATOMIC:
            fprintf(fd, "_Atomic ");
            break;
        default:
            fprintf(fd, "unknown ");
            break;
        }
    }
    fprintf(fd, "\n");
}

// Print Type
void print_type(FILE *fd, const Type *type, int indent)
{
    if (!type) {
        print_indent(fd, indent);
        fprintf(fd, "Type: NULL\n");
        return;
    }

    print_indent(fd, indent);
    fprintf(fd, "Type: ");
    switch (type->kind) {
    case TYPE_VOID:
        fprintf(fd, "void\n");
        break;
    case TYPE_BOOL:
        fprintf(fd, "_Bool\n");
        break;
    case TYPE_CHAR:
        fprintf(fd, "char (%s)\n",
                type->u.integer.signedness == SIGNED_SIGNED ? "signed" : "unsigned");
        break;
    case TYPE_SHORT:
        fprintf(fd, "short (%s)\n",
                type->u.integer.signedness == SIGNED_SIGNED ? "signed" : "unsigned");
        break;
    case TYPE_INT:
        fprintf(fd, "int (%s)\n",
                type->u.integer.signedness == SIGNED_SIGNED ? "signed" : "unsigned");
        break;
    case TYPE_LONG:
            fprintf(fd, "long (%s)\n", type->u.integer.signedness == SIGNED_SIGNED ?  "signed" : "unsigned");
            break;
    case TYPE_FLOAT:
        fprintf(fd, "float\n");
        break;
    case TYPE_DOUBLE:
        fprintf(fd, "double\n");
        break;
    case TYPE_COMPLEX:
        fprintf(fd, "complex\n");
        print_indent(fd, indent + 1);
        fprintf(fd, "Base:\n");
        print_type(fd, type->u.complex.base, indent + 2);
        break;
    case TYPE_IMAGINARY:
        fprintf(fd, "imaginary\n");
        print_indent(fd, indent + 1);
        fprintf(fd, "Base:\n");
        print_type(fd, type->u.complex.base, indent + 2);
        break;
    case TYPE_POINTER:
        fprintf(fd, "pointer\n");
        print_indent(fd, indent + 1);
        fprintf(fd, "Target:\n");
        print_type(fd, type->u.pointer.target, indent + 2);
        print_type_qualifiers(fd, type->u.pointer.qualifiers, indent + 1);
        break;
    case TYPE_ARRAY:
        fprintf(fd, "array\n");
        print_indent(fd, indent + 1);
        fprintf(fd, "Element:\n");
        print_type(fd, type->u.array.element, indent + 2);
        if (type->u.array.size) {
            print_indent(fd, indent + 1);
            fprintf(fd, "Size:\n");
            print_expression(fd, type->u.array.size, indent + 2);
        }
        print_type_qualifiers(fd, type->u.array.qualifiers, indent + 1);
        print_indent(fd, indent + 1);
        fprintf(fd, "Static: %s\n", type->u.array.is_static ? "yes" : "no");
        break;
    case TYPE_FUNCTION:
        fprintf(fd, "function\n");
        print_indent(fd, indent + 1);
        fprintf(fd, "ReturnType:\n");
        print_type(fd, type->u.function.return_type, indent + 2);
        print_indent(fd, indent + 1);
        fprintf(fd, "Parameters:\n");
        print_param(fd, type->u.function.params, indent + 2);
        print_indent(fd, indent + 1);
        fprintf(fd, "Variadic: %s\n", type->u.function.variadic ? "yes" : "no");
        break;
    case TYPE_STRUCT:
        fprintf(fd, "struct %s\n", type->u.struct_t.name ? type->u.struct_t.name : "(anonymous)");
        if (type->u.struct_t.fields) {
            print_indent(fd, indent + 1);
            fprintf(fd, "Fields:\n");
            for (Field *f = type->u.struct_t.fields; f; f = f->next) {
                print_field(fd, f, indent + 2);
            }
        }
        break;
    case TYPE_UNION:
        fprintf(fd, "union %s\n", type->u.struct_t.name ? type->u.struct_t.name : "(anonymous)");
        if (type->u.struct_t.fields) {
            print_indent(fd, indent + 1);
            fprintf(fd, "Fields:\n");
            for (Field *f = type->u.struct_t.fields; f; f = f->next) {
                print_field(fd, f, indent + 2);
            }
        }
        break;
    case TYPE_ENUM:
        fprintf(fd, "enum %s\n", type->u.enum_t.name ? type->u.enum_t.name : "(anonymous)");
        if (type->u.enum_t.enumerators) {
            print_indent(fd, indent + 1);
            fprintf(fd, "Enumerators:\n");
            for (Enumerator *e = type->u.enum_t.enumerators; e; e = e->next) {
                print_indent(fd, indent + 2);
                fprintf(fd, "%s", e->name ? e->name : "(null)");
                if (e->value) {
                    fprintf(fd, " = ");
                    print_expression(fd, e->value, indent + 3);
                } else {
                    fprintf(fd, "\n");
                }
            }
        }
        break;
    case TYPE_TYPEDEF_NAME:
        fprintf(fd, "typedef %s\n",
                type->u.typedef_name.name ? type->u.typedef_name.name : "(null)");
        break;
    case TYPE_ATOMIC:
        fprintf(fd, "atomic\n");
        print_indent(fd, indent + 1);
        fprintf(fd, "Base:\n");
        print_type(fd, type->u.atomic.base, indent + 2);
        break;
    default:
        fprintf(fd, "unknown\n");
        break;
    }

    /* Print qualifiers for the type itself */
    print_type_qualifiers(fd, type->qualifiers, indent);
}

// Print Literal
static void print_literal(FILE *fd, Literal *lit, int indent)
{
    if (!lit) {
        print_indent(fd, indent);
        fprintf(fd, "Literal: null\n");
        return;
    }
    print_indent(fd, indent);
    fprintf(fd, "Literal: ");
    switch (lit->kind) {
    case LITERAL_INT:
        fprintf(fd, "int %d\n", lit->u.int_val);
        break;
    case LITERAL_FLOAT:
        fprintf(fd, "float %f\n", lit->u.real_val);
        break;
    case LITERAL_STRING:
        fprintf(fd, "string \"%s\"\n", lit->u.string_val);
        break;
    case LITERAL_CHAR:
        fprintf(fd, "char '%c'\n", lit->u.char_val);
        break;
    case LITERAL_ENUM:
        fprintf(fd, "enum %s\n", lit->u.enum_const);
        break;
    }
}

// Print Initializer
static void print_initializer(FILE *fd, Initializer *init, int indent)
{
    print_indent(fd, indent);
    if (!init) {
        fprintf(fd, "Initializer: null\n");
        return;
    }
    fprintf(fd, "Initializer:\n");
    if (init->kind == INITIALIZER_SINGLE) {
        print_expression(fd, init->u.expr, indent + 2);
    } else {
        print_indent(fd, indent + 2);
        fprintf(fd, "List: (not implemented)\n");
    }
}

// Print GenericAssoc
static void print_generic_assoc(FILE *fd, GenericAssoc *assoc, int indent)
{
    print_indent(fd, indent);
    if (!assoc) {
        fprintf(fd, "Assoc: null\n");
        return;
    }
    fprintf(fd, "Assoc:\n");
    if (assoc->kind == GENERIC_ASSOC_TYPE) {
        print_type(fd, assoc->u.type_assoc.type, indent + 2);
        print_expression(fd, assoc->u.type_assoc.expr, indent + 2);
    } else {
        print_expression(fd, assoc->u.default_assoc, indent + 2);
    }
}

// Print Expr
void print_expression(FILE *fd, Expr *expr, int indent)
{
    if (!expr) {
        print_indent(fd, indent);
        fprintf(fd, "Expr: null\n");
        return;
    }
    print_indent(fd, indent);
    fprintf(fd, "Expr (%s):\n", expr_kind_str[expr->kind]);
    switch (expr->kind) {
    case EXPR_VAR:
        print_indent(fd, indent + 2);
        fprintf(fd, "Name: \"%s\"\n", expr->u.var);
        break;
    case EXPR_LITERAL:
        print_literal(fd, expr->u.literal, indent + 2);
        break;
    case EXPR_BINARY_OP:
        print_indent(fd, indent + 2);
        fprintf(fd, "Operator: %s\n", binary_op_kind_str[expr->u.binary_op.op->kind]);
        print_expression(fd, expr->u.binary_op.left, indent + 2);
        print_expression(fd, expr->u.binary_op.right, indent + 2);
        break;
    case EXPR_UNARY_OP:
        print_indent(fd, indent + 2);
        fprintf(fd, "Operator: %s\n", unary_op_kind_str[expr->u.unary_op.op->kind]);
        print_expression(fd, expr->u.unary_op.expr, indent + 2);
        break;
    case EXPR_POST_INC:
    case EXPR_POST_DEC:
        print_expression(fd, expr->u.post_inc, indent + 2);
        break;
    case EXPR_CALL:
        print_expression(fd, expr->u.call.func, indent + 2);
        print_indent(fd, indent + 2);
        fprintf(fd, "Arguments:\n");
        for (Expr *arg = expr->u.call.args; arg; arg = arg->next) {
            print_expression(fd, arg, indent + 4);
        }
        break;
    case EXPR_CAST:
        print_type(fd, expr->u.cast.type, indent + 2);
        print_expression(fd, expr->u.cast.expr, indent + 2);
        break;
    case EXPR_COMPOUND:
        print_indent(fd, indent + 2);
        fprintf(fd, "Elements:\n");
        for (InitItem *init = expr->u.compound_literal.init; init; init = init->next) {
            print_initializer(fd, init->init, indent + 4);
        }
        break;
    case EXPR_SIZEOF_EXPR:
        print_expression(fd, expr->u.sizeof_expr, indent + 2);
        break;
    case EXPR_SIZEOF_TYPE:
        print_type(fd, expr->u.sizeof_type, indent + 2);
        break;
    case EXPR_ALIGNOF:
        print_type(fd, expr->u.align_of, indent + 2);
        break;
    case EXPR_GENERIC:
        print_expression(fd, expr->u.generic.controlling_expr, indent + 2);
        print_indent(fd, indent + 2);
        fprintf(fd, "Associations:\n");
        for (GenericAssoc *assoc = expr->u.generic.associations; assoc; assoc = assoc->next) {
            print_generic_assoc(fd, assoc, indent + 4);
        }
        break;
    case EXPR_ASSIGN:
        print_indent(fd, indent + 2);
        fprintf(fd, "Assign: %s\n", assign_op_kind_str[expr->u.assign.op->kind]);
        print_expression(fd, expr->u.assign.target, indent + 2);
        print_expression(fd, expr->u.assign.value, indent + 2);
        break;
    case EXPR_COND:
        print_indent(fd, indent + 2);
        fprintf(fd, "Cond:\n");
        print_expression(fd, expr->u.cond.condition, indent + 2);
        print_expression(fd, expr->u.cond.then_expr, indent + 2);
        print_expression(fd, expr->u.cond.else_expr, indent + 2);
        break;
    case EXPR_FIELD_ACCESS:
        print_indent(fd, indent + 2);
        fprintf(fd, "Field: .%s\n", expr->u.field_access.field);
        print_expression(fd, expr->u.field_access.expr, indent + 2);
        break;
    case EXPR_PTR_ACCESS:
        print_indent(fd, indent + 2);
        fprintf(fd, "Field: ->%s\n", expr->u.ptr_access.field);
        print_expression(fd, expr->u.ptr_access.expr, indent + 2);
        break;
    }
    if (expr->next) {
        print_indent(fd, indent);
        fprintf(fd, "Next Expr:\n");
        print_expression(fd, expr->next, indent + 2);
    }
}

// Print Pointer
void print_pointer(FILE *fd, Pointer *pointer, int indent)
{
    print_indent(fd, indent);
    fprintf(fd, "Pointer\n");
    print_type_qualifiers(fd, pointer->qualifiers, indent + 2);
}

// Print DeclaratorSuffix
void print_declarator_suffix(FILE *fd, DeclaratorSuffix *suffix, int indent)
{
    print_indent(fd, indent);
    fprintf(fd, "Suffix: ");
    switch (suffix->kind) {
    case SUFFIX_ARRAY:
        fprintf(fd, "Array\n");
        if (suffix->u.array.size) {
            print_expression(fd, suffix->u.array.size, indent + 2);
        }
        break;
    case SUFFIX_FUNCTION:
        fprintf(fd, "Function\n");
        print_indent(fd, indent + 2);
        fprintf(fd, "Parameters:\n");
        if (!suffix->u.function.params) {
            print_indent(fd, indent + 4);
            fprintf(fd, "Empty\n");
        } else {
            print_param(fd, suffix->u.function.params, indent + 4);
        }
        break;
    case SUFFIX_POINTER:
        fprintf(fd, "Pointer\n");
        print_indent(fd, indent + 2);
        fprintf(fd, "Indirections:\n");
        for (Pointer *pointer = suffix->u.pointer.pointers; pointer; pointer = pointer->next) {
            print_pointer(fd, pointer, indent + 4);
        }
        print_declarator_suffix(fd, suffix->u.pointer.suffix, indent + 2);
        break;
    }
}

// Print Declarator
void print_declarator(FILE *fd, Declarator *decl, int indent)
{
    print_indent(fd, indent);
    if (!decl) {
        fprintf(fd, "Declarator: null\n");
        return;
    }
    fprintf(fd, "Declarator: %s\n", decl->name ? decl->name : "(abstract)");
    for (Pointer *pointer = decl->pointers; pointer; pointer = pointer->next) {
        print_pointer(fd, pointer, indent + 4);
    }
    for (DeclaratorSuffix *suffix = decl->suffixes; suffix; suffix = suffix->next) {
        print_declarator_suffix(fd, suffix, indent + 2);
    }
}

// Print TypeSpec
void print_type_spec(FILE *fd, TypeSpec *spec, int indent)
{
    switch (spec->kind) {
    case TYPE_SPEC_BASIC:
        fprintf(fd, "%s\n", type_kind_str[spec->u.basic->kind]);
        break;
    case TYPE_SPEC_STRUCT:
        fprintf(fd, "struct %s\n",
                spec->u.struct_spec.name ? spec->u.struct_spec.name
                                                     : "(anonymous)");
        for (Field *field = spec->u.struct_spec.fields; field;
             field        = field->next) {
            print_field(fd, field, indent + 4);
        }
        break;
    case TYPE_SPEC_UNION:
        fprintf(fd, "union %s\n",
                spec->u.struct_spec.name ? spec->u.struct_spec.name
                                                     : "(anonymous)");
        for (Field *field = spec->u.struct_spec.fields; field;
             field        = field->next) {
            print_field(fd, field, indent + 4);
        }
        break;
    case TYPE_SPEC_ENUM:
        fprintf(fd, "enum %s\n",
                spec->u.enum_spec.name ? spec->u.enum_spec.name
                                                   : "(anonymous)");
        for (Enumerator *e = spec->u.enum_spec.enumerators; e; e = e->next) {
            print_indent(fd, indent + 4);
            fprintf(fd, "Enumerator: \"%s\"\n", e->name);
            if (e->value) {
                print_expression(fd, e->value, indent + 6);
            }
        }
        break;
    case TYPE_SPEC_TYPEDEF_NAME:
        fprintf(fd, "typedef %s\n", spec->u.typedef_name.name);
        break;
    case TYPE_SPEC_ATOMIC:
        fprintf(fd, "_Atomic\n");
        print_type(fd, spec->u.atomic.type, indent + 4);
        break;
    }
}

// Print DeclSpec
static void print_decl_spec(FILE *fd, DeclSpec *spec, int indent)
{
    if (!spec) {
        print_indent(fd, indent);
        fprintf(fd, "DeclSpec: null\n");
        return;
    }
    print_indent(fd, indent);
    fprintf(fd, "DeclSpec:\n");
    if (spec->storage) {
        print_indent(fd, indent + 2);
        fprintf(fd, "Storage: ");
        switch (spec->storage->kind) {
        case STORAGE_CLASS_TYPEDEF:
            fprintf(fd, "typedef\n");
            break;
        case STORAGE_CLASS_EXTERN:
            fprintf(fd, "extern\n");
            break;
        case STORAGE_CLASS_STATIC:
            fprintf(fd, "static\n");
            break;
        case STORAGE_CLASS_THREAD_LOCAL:
            fprintf(fd, "_Thread_local\n");
            break;
        case STORAGE_CLASS_AUTO:
            fprintf(fd, "auto\n");
            break;
        case STORAGE_CLASS_REGISTER:
            fprintf(fd, "register\n");
            break;
        }
    }
    if (spec->qualifiers) {
        print_type_qualifiers(fd, spec->qualifiers, indent + 2);
    }
    if (spec->func_specs) {
        print_indent(fd, indent + 2);
        fprintf(fd, "FunctionSpec: ");
        switch (spec->func_specs->kind) {
        case FUNC_SPEC_INLINE:
            fprintf(fd, "inline\n");
            break;
        case FUNC_SPEC_NORETURN:
            fprintf(fd, "_Noreturn\n");
            break;
        }
    }
    if (spec->align_spec) {
        print_indent(fd, indent + 2);
        fprintf(fd, "AlignSpec: ");
        if (spec->align_spec->kind == ALIGN_SPEC_TYPE) {
            fprintf(fd, "type\n");
            print_type(fd, spec->align_spec->u.type, indent + 4);
        } else {
            fprintf(fd, "expr\n");
            print_expression(fd, spec->align_spec->u.expr, indent + 4);
        }
    }
}

// Print InitDeclarator
static void print_init_declarator(FILE *fd, InitDeclarator *id, int indent)
{
    if (!id) {
        print_indent(fd, indent);
        fprintf(fd, "InitDeclarator: null\n");
        return;
    }
    print_indent(fd, indent);
    fprintf(fd, "InitDeclarator: %s\n", id->name ? id->name : "(abstract)");
    print_type(fd, id->type, indent + 2);
    if (id->init) {
        print_initializer(fd, id->init, indent + 2);
    }
    if (id->next) {
        print_indent(fd, indent);
        fprintf(fd, "Next InitDeclarator:\n");
        print_init_declarator(fd, id->next, indent + 2);
    }
}

// Print Declaration
static void print_declaration(FILE *fd, Declaration *decl, int indent)
{
    if (!decl) {
        print_indent(fd, indent);
        fprintf(fd, "Declaration: null\n");
        return;
    }
    print_indent(fd, indent);
    fprintf(fd, "Declaration: ");
    switch (decl->kind) {
    case DECL_VAR:
        fprintf(fd, "Variable\n");
        print_decl_spec(fd, decl->u.var.specifiers, indent + 2);
        print_init_declarator(fd, decl->u.var.declarators, indent + 2);
        break;
    case DECL_STATIC_ASSERT:
        fprintf(fd, "StaticAssert\n");
        print_indent(fd, indent + 2);
        fprintf(fd, "Condition:\n");
        print_expression(fd, decl->u.static_assrt.condition, indent + 4);
        print_indent(fd, indent + 2);
        fprintf(fd, "Message: \"%s\"\n", decl->u.static_assrt.message);
        break;
    case DECL_EMPTY:
        fprintf(fd, "Empty\n");
        print_decl_spec(fd, decl->u.empty.specifiers, indent + 2);
        print_type(fd, decl->u.empty.type, indent + 2);
        break;
    }
}

// Print Stmt
void print_statement(FILE *fd, Stmt *stmt, int indent)
{
    if (!stmt) {
        print_indent(fd, indent);
        fprintf(fd, "Stmt: null\n");
        return;
    }
    print_indent(fd, indent);
    fprintf(fd, "Stmt (%s):\n", stmt_kind_str[stmt->kind]);
    switch (stmt->kind) {
    case STMT_EXPR:
        print_expression(fd, stmt->u.expr, indent + 2);
        break;
    case STMT_IF:
        print_indent(fd, indent + 2);
        fprintf(fd, "Condition:\n");
        print_expression(fd, stmt->u.if_stmt.condition, indent + 4);
        print_indent(fd, indent + 2);
        fprintf(fd, "Then:\n");
        print_statement(fd, stmt->u.if_stmt.then_stmt, indent + 4);
        if (stmt->u.if_stmt.else_stmt) {
            print_indent(fd, indent + 2);
            fprintf(fd, "Else:\n");
            print_statement(fd, stmt->u.if_stmt.else_stmt, indent + 4);
        }
        break;
    case STMT_SWITCH:
        print_indent(fd, indent + 2);
        fprintf(fd, "Expression:\n");
        print_expression(fd, stmt->u.switch_stmt.expr, indent + 4);
        print_indent(fd, indent + 2);
        fprintf(fd, "Body:\n");
        print_statement(fd, stmt->u.switch_stmt.body, indent + 4);
        break;
    case STMT_WHILE:
        print_indent(fd, indent + 2);
        fprintf(fd, "Condition:\n");
        print_expression(fd, stmt->u.while_stmt.condition, indent + 4);
        print_indent(fd, indent + 2);
        fprintf(fd, "Body:\n");
        print_statement(fd, stmt->u.while_stmt.body, indent + 4);
        break;
    case STMT_DO_WHILE:
        print_indent(fd, indent + 2);
        fprintf(fd, "Body:\n");
        print_statement(fd, stmt->u.do_while.body, indent + 4);
        print_indent(fd, indent + 2);
        fprintf(fd, "Condition:\n");
        print_expression(fd, stmt->u.do_while.condition, indent + 4);
        break;
    case STMT_FOR:
        print_indent(fd, indent + 2);
        fprintf(fd, "Init:\n");
        if (stmt->u.for_stmt.init->kind == FOR_INIT_EXPR) {
            print_expression(fd, stmt->u.for_stmt.init->u.expr, indent + 4);
        } else {
            print_declaration(fd, stmt->u.for_stmt.init->u.decl, indent + 4);
        }
        print_indent(fd, indent + 2);
        fprintf(fd, "Condition:\n");
        print_expression(fd, stmt->u.for_stmt.condition, indent + 4);
        print_indent(fd, indent + 2);
        fprintf(fd, "Update:\n");
        print_expression(fd, stmt->u.for_stmt.update, indent + 4);
        print_indent(fd, indent + 2);
        fprintf(fd, "Body:\n");
        print_statement(fd, stmt->u.for_stmt.body, indent + 4);
        break;
    case STMT_GOTO:
        print_indent(fd, indent + 2);
        fprintf(fd, "Label: \"%s\"\n", stmt->u.goto_label);
        break;
    case STMT_CONTINUE:
        print_indent(fd, indent + 2);
        fprintf(fd, "Continue\n");
        break;
    case STMT_BREAK:
        print_indent(fd, indent + 2);
        fprintf(fd, "Break\n");
        break;
    case STMT_RETURN:
        print_indent(fd, indent + 2);
        fprintf(fd, "Return:\n");
        print_expression(fd, stmt->u.expr, indent + 4);
        break;
    case STMT_LABELED:
        print_indent(fd, indent + 2);
        fprintf(fd, "Label: \"%s\"\n", stmt->u.labeled.label);
        print_statement(fd, stmt->u.labeled.stmt, indent + 2);
        break;
    case STMT_CASE:
        print_indent(fd, indent + 2);
        fprintf(fd, "Case:\n");
        print_expression(fd, stmt->u.case_stmt.expr, indent + 4);
        print_statement(fd, stmt->u.case_stmt.stmt, indent + 4);
        break;
    case STMT_DEFAULT:
        print_indent(fd, indent + 2);
        fprintf(fd, "Default:\n");
        print_statement(fd, stmt->u.default_stmt, indent + 4);
        break;
    case STMT_COMPOUND:
        print_indent(fd, indent + 2);
        fprintf(fd, "Compound:\n");
        for (DeclOrStmt *item = stmt->u.compound; item; item = item->next) {
            print_indent(fd, indent + 4);
            fprintf(fd, "Item:\n");
            if (item->kind == DECL_OR_STMT_DECL) {
                print_declaration(fd, item->u.decl, indent + 6);
            } else {
                print_statement(fd, item->u.stmt, indent + 6);
            }
        }
        break;
    }
}

// Print ExternalDecl
static void print_external_decl(FILE *fd, ExternalDecl *ext, int indent)
{
    if (!ext) {
        print_indent(fd, indent);
        fprintf(fd, "ExternalDecl: null\n");
        return;
    }
    print_indent(fd, indent);
    fprintf(fd, "ExternalDecl:\n");
    print_indent(fd, indent + 2);
    switch (ext->kind) {
    case EXTERNAL_DECL_FUNCTION:
        fprintf(fd, "Function: %s\n", ext->u.function.name ? ext->u.function.name : "(anonymous)");
        print_type(fd, ext->u.function.type, indent + 4);
        print_decl_spec(fd, ext->u.function.specifiers, indent + 4);
        print_statement(fd, ext->u.function.body, indent + 4);
        break;
    case EXTERNAL_DECL_DECLARATION:
        fprintf(fd, "Declaration\n");
        print_declaration(fd, ext->u.declaration, indent + 4);
        break;
    }
    if (ext->next) {
        print_indent(fd, indent);
        fprintf(fd, "Next ExternalDecl:\n");
        print_external_decl(fd, ext->next, indent + 4);
    }
}

// Main print function
void print_program(FILE *fd, Program *program)
{
    if (!program) {
        fprintf(fd, "Program: null\n");
        return;
    }
    fprintf(fd, "Program:\n");
    print_external_decl(fd, program->decls, 2);
}
