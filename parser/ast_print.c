#include <string.h>

#include "ast.h"

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

static const char *type_kind_str[] = {
    [TYPE_VOID] = "void",     [TYPE_CHAR] = "char",        [TYPE_SHORT] = "short",
    [TYPE_INT] = "int",       [TYPE_LONG] = "long",        [TYPE_FLOAT] = "float",
    [TYPE_DOUBLE] = "double",
    [TYPE_BOOL] = "_Bool",    [TYPE_COMPLEX] = "_Complex", [TYPE_IMAGINARY] = "_Imaginary",
    [TYPE_ATOMIC] = "_Atomic"
};

static const char *binary_op_kind_str[] = {
    [BINARY_MUL] = "*",          [BINARY_DIV] = "/",      [BINARY_MOD] = "%",
    [BINARY_ADD] = "+",          [BINARY_SUB] = "-",      [BINARY_LEFT_SHIFT] = "<<",
    [BINARY_RIGHT_SHIFT] = ">>", [BINARY_LT] = "<",       [BINARY_GT] = ">",
    [BINARY_LE] = "<=",          [BINARY_GE] = ">=",      [BINARY_EQ] = "==",
    [BINARY_NE] = "!=",          [BINARY_BIT_AND] = "&",  [BINARY_BIT_XOR] = "^",
    [BINARY_BIT_OR] = "|",       [BINARY_LOG_AND] = "&&", [BINARY_LOG_OR] = "||"
};

static const char *unary_op_kind_str[] = {
    [UNARY_ADDRESS] = "&",   [UNARY_DEREF] = "*",    [UNARY_PLUS] = "+",     [UNARY_NEG] = "-",
    [UNARY_BIT_NOT] = "~",   [UNARY_LOG_NOT] = "!",  [UNARY_PRE_INC] = "++", [UNARY_PRE_DEC] = "--",
};

// Forward declarations
static void print_expr(FILE *fd, Expr *expr, int indent);
static void print_stmt(FILE *fd, Stmt *stmt, int indent);
static void print_declaration(FILE *fd, Declaration *decl, int indent);
static void print_external_decl(FILE *fd, ExternalDecl *ext, int indent);

// Print Type
static void print_type(FILE *fd, Type *type, int indent)
{
    if (!type) {
        print_indent(fd, indent);
        fprintf(fd, "Type: null\n");
        return;
    }
    print_indent(fd, indent);
    fprintf(fd, "Type: %s\n", type_kind_str[type->kind]);
    if (type->qualifiers) {
        print_indent(fd, indent + 2);
        fprintf(fd, "Qualifiers: ");
        TypeQualifier *qual = type->qualifiers;
        while (qual) {
            switch (qual->kind) {
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
            }
            qual = qual->next;
        }
        fprintf(fd, "\n");
    }
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
        fprintf(fd, "float %f\n", lit->u.float_val);
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

// Print Expr
static void print_expr(FILE *fd, Expr *expr, int indent)
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
        print_expr(fd, expr->u.binary_op.left, indent + 2);
        print_expr(fd, expr->u.binary_op.right, indent + 2);
        break;
    case EXPR_UNARY_OP:
        print_indent(fd, indent + 2);
        fprintf(fd, "Operator: %s\n", unary_op_kind_str[expr->u.unary_op.op->kind]);
        print_expr(fd, expr->u.unary_op.expr, indent + 2);
        break;
    case EXPR_POST_INC:
    case EXPR_POST_DEC:
        print_expr(fd, expr->u.post_inc, indent + 2);
        break;
    case EXPR_CALL:
        print_expr(fd, expr->u.call.func, indent + 2);
        print_indent(fd, indent + 2);
        fprintf(fd, "Arguments:\n");
        for (Expr *arg = expr->u.call.args; arg; arg = arg->next) {
            print_expr(fd, arg, indent + 4);
        }
        break;
    case EXPR_CAST:
        print_type(fd, expr->u.cast.type, indent + 2);
        print_expr(fd, expr->u.cast.expr, indent + 2);
        break;
    case EXPR_COMPOUND:
        print_indent(fd, indent + 2);
        fprintf(fd, "Elements:\n");
        for (Initializer *init = expr->u.compound; init; init = init->next) {
            print_indent(fd, indent + 4);
            fprintf(fd, "Initializer:\n");
            if (init->kind == INITIALIZER_SINGLE) {
                print_expr(fd, init->u.expr, indent + 6);
            } else {
                print_indent(fd, indent + 6);
                fprintf(fd, "List: (not implemented)\n");
            }
        }
        break;
    case EXPR_SIZEOF_EXPR:
        print_expr(fd, expr->u.sizeof_expr, indent + 2);
        break;
    case EXPR_SIZEOF_TYPE:
        print_type(fd, expr->u.sizeof_type, indent + 2);
        break;
    case EXPR_ALIGNOF:
        print_type(fd, expr->u.alignof, indent + 2);
        break;
    case EXPR_GENERIC:
        print_expr(fd, expr->u.generic.control, indent + 2);
        print_indent(fd, indent + 2);
        fprintf(fd, "Associations:\n");
        for (GenericAssoc *assoc = expr->u.generic.assocs; assoc; assoc = assoc->next) {
            print_indent(fd, indent + 4);
            fprintf(fd, "Assoc:\n");
            print_type(fd, assoc->type, indent + 6);
            print_expr(fd, assoc->expr, indent + 6);
        }
        break;
    }
    if (expr->next) {
        print_indent(fd, indent);
        fprintf(fd, "Next Expr:\n");
        print_expr(fd, expr->next, indent + 2);
    }
}

// Print Declarator
static void print_declarator(FILE *fd, Declarator *decl, int indent)
{
    if (!decl) {
        print_indent(fd, indent);
        fprintf(fd, "Declarator: null\n");
        return;
    }
    print_indent(fd, indent);
    fprintf(fd, "Declarator:\n");
    if (decl->kind == DECLARATOR_NAMED) {
        print_indent(fd, indent + 2);
        fprintf(fd, "Name: \"%s\"\n", decl->u.named.name);
        for (DeclSuffix *suffix = decl->u.named.suffixes; suffix; suffix = suffix->next) {
            print_indent(fd, indent + 2);
            fprintf(fd, "Suffix: ");
            switch (suffix->kind) {
            case SUFFIX_ARRAY:
                fprintf(fd, "Array\n");
                if (suffix->u.array.size) {
                    print_expr(fd, suffix->u.array.size, indent + 4);
                }
                break;
            case SUFFIX_FUNCTION:
                fprintf(fd, "Function\n");
                print_indent(fd, indent + 4);
                fprintf(fd, "Parameters:\n");
                if (suffix->u.function.params->is_empty) {
                    print_indent(fd, indent + 6);
                    fprintf(fd, "Empty\n");
                } else {
                    for (Param *param = suffix->u.function.params->u.params; param;
                         param        = param->next) {
                        print_indent(fd, indent + 6);
                        fprintf(fd, "Param:\n");
                        print_type(fd, param->type, indent + 8);
                        print_indent(fd, indent + 8);
                        fprintf(fd, "Name: \"%s\"\n", param->name);
                    }
                }
                break;
            }
        }
    } else {
        print_indent(fd, indent + 2);
        fprintf(fd, "Abstract\n");
    }
}

// Print Initializer
static void print_initializer(FILE *fd, Initializer *init, int indent)
{
    if (!init) {
        print_indent(fd, indent);
        fprintf(fd, "Initializer: null\n");
        return;
    }
    print_indent(fd, indent);
    fprintf(fd, "Initializer:\n");
    if (init->kind == INITIALIZER_SINGLE) {
        print_expr(fd, init->u.expr, indent + 2);
    } else {
        print_indent(fd, indent + 2);
        fprintf(fd, "List: (not implemented)\n");
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
    if (spec->type_specs) {
        print_indent(fd, indent + 2);
        fprintf(fd, "TypeSpec: ");
        switch (spec->type_specs->kind) {
        case TYPE_SPEC_BASIC:
            fprintf(fd, "%s\n", type_kind_str[spec->type_specs->u.basic->kind]);
            if (spec->type_specs->u.basic->kind == TYPE_SIGNED ||
                spec->type_specs->u.basic->kind == TYPE_UNSIGNED) {
                print_indent(fd, indent + 4);
                fprintf(fd, "Signedness: %s\n",
                        spec->type_specs->u.basic->u.char_t.signedness == SIGNED_SIGNED
                            ? "signed"
                            : "unsigned");
            }
            break;
        case TYPE_SPEC_STRUCT:
            fprintf(fd, "struct %s\n",
                    spec->type_specs->u.struct_spec.name ? spec->type_specs->u.struct_spec.name
                                                         : "(anonymous)");
            for (StructField *field = spec->type_specs->u.struct_spec.fields; field;
                 field              = field->next) {
                print_indent(fd, indent + 4);
                fprintf(fd, "Field:\n");
                if (field->is_anonymous) {
                    print_indent(fd, indent + 6);
                    fprintf(fd, "Anonymous\n");
                } else {
                    print_indent(fd, indent + 6);
                    fprintf(fd, "Name: \"%s\"\n", field->u.named.name);
                    print_type(fd, field->u.named.type, indent + 6);
                }
            }
            break;
        case TYPE_SPEC_UNION:
            fprintf(fd, "union %s\n",
                    spec->type_specs->u.struct_spec.name ? spec->type_specs->u.struct_spec.name
                                                         : "(anonymous)");
            for (StructField *field = spec->type_specs->u.struct_spec.fields; field;
                 field              = field->next) {
                print_indent(fd, indent + 4);
                fprintf(fd, "Field:\n");
                if (field->is_anonymous) {
                    print_indent(fd, indent + 6);
                    fprintf(fd, "Anonymous\n");
                } else {
                    print_indent(fd, indent + 6);
                    fprintf(fd, "Name: \"%s\"\n", field->u.named.name);
                    print_type(fd, field->u.named.type, indent + 6);
                }
            }
            break;
        case TYPE_SPEC_ENUM:
            fprintf(fd, "enum %s\n",
                    spec->type_specs->u.enum_spec.name ? spec->type_specs->u.enum_spec.name
                                                       : "(anonymous)");
            for (Enumerator *e = spec->type_specs->u.enum_spec.enumerators; e; e = e->next) {
                print_indent(fd, indent + 4);
                fprintf(fd, "Enumerator: \"%s\"\n", e->name);
                if (e->value) {
                    print_expr(fd, e->value, indent + 6);
                }
            }
            break;
        case TYPE_SPEC_TYPEDEF_NAME:
            fprintf(fd, "typedef %s\n", spec->type_specs->u.typedef_name);
            break;
        case TYPE_SPEC_ATOMIC:
            fprintf(fd, "_Atomic\n");
            print_type(fd, spec->type_specs->u.atomic->u.atomic.base, indent + 4);
            break;
        }
    }
    if (spec->qualifiers) {
        print_indent(fd, indent + 2);
        fprintf(fd, "Qualifiers: ");
        TypeQualifier *qual = spec->qualifiers;
        while (qual) {
            switch (qual->kind) {
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
            }
            qual = qual->next;
        }
        fprintf(fd, "\n");
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
            print_expr(fd, spec->align_spec->u.expr, indent + 4);
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
    fprintf(fd, "InitDeclarator:\n");
    print_declarator(fd, id->declarator, indent + 2);
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
        print_expr(fd, decl->u.static_assert.condition, indent + 4);
        print_indent(fd, indent + 2);
        fprintf(fd, "Message: \"%s\"\n", decl->u.static_assert.message);
        break;
    case DECL_EMPTY:
        fprintf(fd, "Empty\n");
        print_decl_spec(fd, decl->u.var.specifiers, indent + 2);
        break;
    }
}

// Print Stmt
static void print_stmt(FILE *fd, Stmt *stmt, int indent)
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
        print_expr(fd, stmt->u.expr, indent + 2);
        break;
    case STMT_IF:
        print_indent(fd, indent + 2);
        fprintf(fd, "Condition:\n");
        print_expr(fd, stmt->u.if_stmt.condition, indent + 4);
        print_indent(fd, indent + 2);
        fprintf(fd, "Then:\n");
        print_stmt(fd, stmt->u.if_stmt.then_stmt, indent + 4);
        if (stmt->u.if_stmt.else_stmt) {
            print_indent(fd, indent + 2);
            fprintf(fd, "Else:\n");
            print_stmt(fd, stmt->u.if_stmt.else_stmt, indent + 4);
        }
        break;
    case STMT_SWITCH:
        print_indent(fd, indent + 2);
        fprintf(fd, "Expression:\n");
        print_expr(fd, stmt->u.switch_stmt.expr, indent + 4);
        print_indent(fd, indent + 2);
        fprintf(fd, "Body:\n");
        print_stmt(fd, stmt->u.switch_stmt.body, indent + 4);
        break;
    case STMT_WHILE:
        print_indent(fd, indent + 2);
        fprintf(fd, "Condition:\n");
        print_expr(fd, stmt->u.while_stmt.condition, indent + 4);
        print_indent(fd, indent + 2);
        fprintf(fd, "Body:\n");
        print_stmt(fd, stmt->u.while_stmt.body, indent + 4);
        break;
    case STMT_DO_WHILE:
        print_indent(fd, indent + 2);
        fprintf(fd, "Body:\n");
        print_stmt(fd, stmt->u.do_while.body, indent + 4);
        print_indent(fd, indent + 2);
        fprintf(fd, "Condition:\n");
        print_expr(fd, stmt->u.do_while.condition, indent + 4);
        break;
    case STMT_FOR:
        print_indent(fd, indent + 2);
        fprintf(fd, "Init:\n");
        if (stmt->u.for_stmt.init->kind == FOR_INIT_EXPR) {
            print_expr(fd, stmt->u.for_stmt.init->u.expr, indent + 4);
        } else {
            print_declaration(fd, stmt->u.for_stmt.init->u.decl, indent + 4);
        }
        print_indent(fd, indent + 2);
        fprintf(fd, "Condition:\n");
        print_expr(fd, stmt->u.for_stmt.condition, indent + 4);
        print_indent(fd, indent + 2);
        fprintf(fd, "Update:\n");
        print_expr(fd, stmt->u.for_stmt.update, indent + 4);
        print_indent(fd, indent + 2);
        fprintf(fd, "Body:\n");
        print_stmt(fd, stmt->u.for_stmt.body, indent + 4);
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
        print_expr(fd, stmt->u.expr, indent + 4);
        break;
    case STMT_LABELED:
        print_indent(fd, indent + 2);
        fprintf(fd, "Label: \"%s\"\n", stmt->u.labeled.label);
        print_stmt(fd, stmt->u.labeled.stmt, indent + 2);
        break;
    case STMT_CASE:
        print_indent(fd, indent + 2);
        fprintf(fd, "Case:\n");
        print_expr(fd, stmt->u.case_stmt.expr, indent + 4);
        print_stmt(fd, stmt->u.case_stmt.stmt, indent + 4);
        break;
    case STMT_DEFAULT:
        print_indent(fd, indent + 2);
        fprintf(fd, "Default:\n");
        print_stmt(fd, stmt->u.default_stmt, indent + 4);
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
                print_stmt(fd, item->u.stmt, indent + 6);
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
    fprintf(fd, "ExternalDecl: ");
    switch (ext->kind) {
    case EXTERNAL_DECL_FUNCTION:
        fprintf(fd, "Function\n");
        print_decl_spec(fd, ext->u.function.specifiers, indent + 2);
        print_declarator(fd, ext->u.function.declarator, indent + 2);
        print_stmt(fd, ext->u.function.body, indent + 2);
        break;
    case EXTERNAL_DECL_DECLARATION:
        fprintf(fd, "Declaration\n");
        print_declaration(fd, ext->u.declaration, indent + 2);
        break;
    }
    if (ext->next) {
        print_indent(fd, indent);
        fprintf(fd, "Next ExternalDecl:\n");
        print_external_decl(fd, ext->next, indent + 2);
    }
}

// Main print function
void print_ast(FILE *fd, Program *program)
{
    if (!program) {
        fprintf(fd, "Program: null\n");
        return;
    }
    fprintf(fd, "Program:\n");
    print_external_decl(fd, program->decls, 2);
}
