#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "translator.h"
#include "symtab.h"
#include "typetab.h"

// Forward declarations
void resolve_expr(Expr *e);
void resolve_initializer(Initializer *init);

//
// Resolve type
//
void resolve_type(Type *t)
{
    if (!t)
        return;
    switch (t->kind) {
    case TYPE_STRUCT: {
        const StructDef *entry = typetab_find(t->u.struct_t.name);
        if (!entry) {
            fatal_error("Undeclared structure type %s", t->u.struct_t.name);
        }
        // Note: we do not rename structs
        break;
    }
    case TYPE_POINTER: {
        resolve_type(t->u.pointer.target);
        break;
    }
    case TYPE_ARRAY: {
        resolve_type(t->u.array.element);
        resolve_expr(t->u.array.size);
        break;
    }
    case TYPE_FUNCTION: {
        resolve_type(t->u.function.return_type);
        Param *p = t->u.function.params;
        while (p) {
            resolve_type(p->type);
            p = p->next;
        }
        break;
    }
    default:
        break; // Other types unchanged
    }
}

//
// Resolve expression
//
void resolve_expr(Expr *e)
{
    if (!e)
        return;
    switch (e->kind) {
    case EXPR_LITERAL:
        return; // No resolution needed
    case EXPR_VAR: {
        const Symbol *entry = symtab_get_opt(e->u.var);
        if (!entry) {
            fatal_error("Undeclared variable %s", e->u.var);
        }
        // Note: we do not rename variables
        return;
    }
    case EXPR_UNARY_OP: {
        resolve_expr(e->u.unary_op.expr);
        return;
    }
    case EXPR_BINARY_OP: {
        resolve_expr(e->u.binary_op.left);
        resolve_expr(e->u.binary_op.right);
        return;
    }
    case EXPR_ASSIGN: {
        resolve_expr(e->u.assign.target);
        resolve_expr(e->u.assign.value);
        return;
    }
    case EXPR_COND: {
        resolve_expr(e->u.cond.condition);
        resolve_expr(e->u.cond.then_expr);
        resolve_expr(e->u.cond.else_expr);
        return;
    }
    case EXPR_CAST: {
        resolve_type(e->u.cast.type);
        resolve_expr(e->u.cast.expr);
        return;
    }
    case EXPR_CALL: {
        if (e->u.call.func->kind != EXPR_VAR) {
            fatal_error("Function call must be a variable");
        }
        const Symbol *entry = symtab_get_opt(e->u.call.func->u.var);
        if (!entry) {
            fatal_error("Undeclared function %s", e->u.call.func->u.var);
        }
        // Note: we do not rename functions
        Expr *args = e->u.call.args;
        while (args) {
            resolve_expr(args);
            args = args->next;
        }
        return;
    }
    case EXPR_COMPOUND: {
        resolve_type(e->u.compound_literal.type);
        InitItem *item = e->u.compound_literal.init;
        while (item) {
            resolve_initializer(item->init);
            item = item->next;
        }
        return;
    }
    case EXPR_FIELD_ACCESS: {
        resolve_expr(e->u.field_access.expr);
        return;
    }
    case EXPR_PTR_ACCESS: {
        resolve_expr(e->u.ptr_access.expr);
        return;
    }
    case EXPR_POST_INC: {
        resolve_expr(e->u.post_inc);
        return;
    }
    case EXPR_POST_DEC: {
        resolve_expr(e->u.post_dec);
        return;
    }
    case EXPR_SIZEOF_EXPR: {
        resolve_expr(e->u.sizeof_expr);
        return;
    }
    case EXPR_SIZEOF_TYPE: {
        resolve_type(e->u.sizeof_type);
        return;
    }
    case EXPR_ALIGNOF: {
        resolve_type(e->u.align_of);
        return;
    }
    case EXPR_GENERIC: {
        resolve_expr(e->u.generic.controlling_expr);
        GenericAssoc *assoc = e->u.generic.associations;
        while (assoc) {
            if (assoc->kind == GENERIC_ASSOC_TYPE) {
                resolve_type(assoc->u.type_assoc.type);
                resolve_expr(assoc->u.type_assoc.expr);
            } else {
                resolve_expr(assoc->u.default_assoc);
            }
            assoc = assoc->next;
        }
        return;
    }
    default:
        fatal_error("Unknown expression kind %d", e->kind);
    }
}

//
// Resolve initializer
//
void resolve_initializer(Initializer *init)
{
    if (!init)
        return;
    switch (init->kind) {
    case INITIALIZER_SINGLE:
        resolve_expr(init->u.expr);
        break;
    case INITIALIZER_COMPOUND: {
        InitItem *item = init->u.items;
        while (item) {
            resolve_initializer(item->init);
            Designator *designator = item->designators;
            while (designator) {
                if (designator->kind == DESIGNATOR_ARRAY) {
                    resolve_expr(designator->u.expr);
                }
                designator = designator->next;
            }
            item = item->next;
        }
        break;
    }
    }
}

//
// Resolve for init
//
void resolve_for_init(ForInit *init)
{
    if (!init)
        return;
    switch (init->kind) {
    case FOR_INIT_EXPR:
        resolve_expr(init->u.expr);
        break;
    case FOR_INIT_DECL:
        // Handled in resolve_statement
        break;
    }
}

//
// Resolve declaration of local variables
//
void resolve_local_var_declaration(Declaration *decl)
{
    InitDeclarator *id = decl->u.var.declarators;
    while (id) {
        const Symbol *entry = symtab_get_opt(id->name);
        if (entry && !entry->has_linkage) {
            // Note: We do not allow shadowing of variables.
            // So there is no need to keep track of whether
            // a previous definition was in the current scope.
            fatal_error("Duplicate variable declaration %s", id->name);
        }
        bool has_linkage = decl->u.var.specifiers->storage == STORAGE_CLASS_EXTERN;
        symtab_add_automatic_var_linkage(id->name, has_linkage, scope_level);

        resolve_type(id->type);
        if (id->init) {
            resolve_initializer(id->init);
        }
        id = id->next;
    }
}

//
// Increase scope level.
//
static void scope_increment()
{
    scope_level++;
}

//
// Decrease scope level.
// Purge all symbols and types of the removed scope.
//
static void scope_decrement()
{
    scope_level--;
    symtab_purge(scope_level);
    typetab_purge(scope_level);
}

//
// Resolve statement
//
void resolve_statement(Stmt *s)
{
    if (!s)
        return;
    switch (s->kind) {
    case STMT_EXPR:
        resolve_expr(s->u.expr);
        return;
    case STMT_COMPOUND: {
        DeclOrStmt *ds = s->u.compound;
        scope_increment();
        while (ds) {
            if (ds->kind == DECL_OR_STMT_STMT) {
                resolve_statement(ds->u.stmt);
            } else {
                Declaration *decl = ds->u.decl;
                if (decl->kind == DECL_VAR) {
                    resolve_local_var_declaration(decl);
                }
            }
            ds = ds->next;
        }
        scope_decrement();
        return;
    }
    case STMT_IF: {
        resolve_expr(s->u.if_stmt.condition);
        resolve_statement(s->u.if_stmt.then_stmt);
        resolve_statement(s->u.if_stmt.else_stmt);
        return;
    }
    case STMT_SWITCH: {
        resolve_expr(s->u.switch_stmt.expr);
        resolve_statement(s->u.switch_stmt.body);
        return;
    }
    case STMT_WHILE: {
        resolve_expr(s->u.while_stmt.condition);
        resolve_statement(s->u.while_stmt.body);
        return;
    }
    case STMT_DO_WHILE: {
        resolve_statement(s->u.do_while.body);
        resolve_expr(s->u.do_while.condition);
        return;
    }
    case STMT_FOR: {
        scope_increment();
        resolve_for_init(s->u.for_stmt.init);
        if (s->u.for_stmt.init && s->u.for_stmt.init->kind == FOR_INIT_DECL) {
            Declaration *decl = s->u.for_stmt.init->u.decl;
            if (decl->kind == DECL_VAR) {
                resolve_local_var_declaration(decl);
            }
        }
        resolve_expr(s->u.for_stmt.condition);
        resolve_expr(s->u.for_stmt.update);
        resolve_statement(s->u.for_stmt.body);
        scope_decrement();
        return;
    }
    case STMT_GOTO:
        return; // Label resolution not handled
    case STMT_CONTINUE:
    case STMT_BREAK:
        return; // No resolution needed
    case STMT_RETURN:
        resolve_expr(s->u.expr);
        return;
    case STMT_LABELED: {
        resolve_statement(s->u.labeled.stmt);
        return;
    }
    case STMT_CASE: {
        resolve_expr(s->u.case_stmt.expr);
        resolve_statement(s->u.case_stmt.stmt);
        return;
    }
    case STMT_DEFAULT: {
        resolve_statement(s->u.default_stmt);
        return;
    }
    default:
        fatal_error("Unknown statement kind %d", s->kind);
    }
}

//
// Resolve function declaration
//
void resolve_function_declaration(ExternalDecl *fd)
{
    const Symbol *entry = symtab_get_opt(fd->u.function.name);
    if (entry && !entry->has_linkage) {
        // Note: We do not allow shadowing.
        // So there is no need to keep track of whether
        // a previous definition was in the current scope.
        fatal_error("Duplicate declaration %s", fd->u.function.name);
    }
    resolve_type(fd->u.function.type);
    symtab_add_fun(fd->u.function.name, fd->u.function.type, 1, fd->u.function.body != NULL);

    if (fd->u.function.body) {
        Param *p = fd->u.function.type->u.function.params;
        scope_increment();
        while (p) {
            resolve_type(p->type);
            symtab_add_automatic_var_type(p->name, p->type, scope_level);
            p = p->next;
        }
        resolve_statement(fd->u.function.body);
        scope_decrement();
    }
}

//
// Resolve structure declaration
//
void resolve_struct_decl(Declaration *d)
{
    if (!d->u.empty.type)
        return;
    TypeKind kind = d->u.empty.type->kind;
    if (kind != TYPE_STRUCT && kind != TYPE_UNION)
        return; // Ignore forward declarations

    const char *tag        = d->u.empty.type->u.struct_t.name;
    const StructDef *entry = typetab_find(tag);
    if (entry) {
        fatal_error("Re-declared structure type %s", tag);
    }

    //validate_struct_definition(tag, d->u.empty.type->u.struct_t.fields);

    // Build member definitions
    FieldDef *members     = NULL;
    FieldDef **tail       = &members;
    int current_size      = 0;
    int current_alignment = 1;
    for (Field *f = d->u.empty.type->u.struct_t.fields; f; f = f->next) {
        resolve_type(f->type);

        int member_alignment = get_alignment(f->type);
        int offset           = 0;
        if (kind == TYPE_STRUCT) {
            offset = round_away_from_zero(member_alignment, current_size);
        }
        *tail = new_member(f->name, clone_type(f->type, __func__, __FILE__, __LINE__), offset);
        tail  = &(*tail)->next;

        if (current_alignment < member_alignment) {
            current_alignment = member_alignment;
        }
        current_size = offset + get_size(f->type);
    }
    int size = round_away_from_zero(current_alignment, current_size);

    typetab_add_struct(tag, current_alignment, size, members, scope_level);
}

//
// Resolve global declaration
//
void resolve(ExternalDecl *decl)
{
    switch (decl->kind) {
    case EXTERNAL_DECL_FUNCTION:
        resolve_function_declaration(decl);
        break;
    case EXTERNAL_DECL_DECLARATION:
        switch (decl->u.declaration->kind) {
        case DECL_VAR: {
            //typecheck_file_scope_var_decl(decl->u.declaration);
            InitDeclarator *id = decl->u.declaration->u.var.declarators;

            //TODO: symbol_table_insert(id->name, id->name, 1, 1);
            resolve_type(id->type);
            if (id->init) {
                resolve_initializer(id->init);
            }
            break;
        }
        case DECL_EMPTY:
            // typecheck_struct_decl(decl->u.declaration);
            resolve_struct_decl(decl->u.declaration);
            break;
        case DECL_STATIC_ASSERT:
            // TODO: implement static assert.
            break;
        }
        break;
    }
}
