#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Unique_ids.h" // Assume this provides make_named_temporary
#include "ast.h"
#include "hash_table.h"

typedef struct {
    char *key;
    char *unique_name;
    int from_current_scope;
    int has_linkage;
} VarEntry;

typedef struct {
    char *key;
    char *unique_tag;
    int struct_from_current_scope;
} StructEntry;

HashTable *type_table;
HashTable *symbol_table;

// Insert into type_table
static void type_table_insert(const char *key, const char *unique_tag,
                              int struct_from_current_scope)
{
    StructEntry *entry               = malloc(sizeof(StructEntry));
    entry->unique_tag                = strdup(unique_tag);
    entry->struct_from_current_scope = struct_from_current_scope;
    hash_table_insert(type_table, key, entry);
}

// Insert into symbol_table
static void symbol_table_insert(const char *key, const char *unique_name, int from_current_scope,
                                int has_linkage)
{
    VarEntry *entry           = malloc(sizeof(VarEntry));
    entry->unique_name        = strdup(unique_name);
    entry->from_current_scope = from_current_scope;
    entry->has_linkage        = has_linkage;
    hash_table_insert(symbol_table, key, entry);
}

// Copy symbol table
HashTable *copy_symbol_table(HashTable *m)
{
    HashTable *new_map = create_hash_table();
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        HashNode *node = m->buckets[i];
        while (node) {
            VarEntry *entry               = node->value;
            VarEntry *new_entry           = malloc(sizeof(VarEntry));
            new_entry->unique_name        = strdup(entry->unique_name);
            new_entry->from_current_scope = 0; // Reset to false
            new_entry->has_linkage        = entry->has_linkage;
            hash_table_insert(new_map, node->key, new_entry);
            node = node->next;
        }
    }
    return new_map;
}

// Copy type table
HashTable *copy_type_table(HashTable *m)
{
    HashTable *new_map = create_hash_table();
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        HashNode *node = m->buckets[i];
        while (node) {
            StructEntry *entry                   = node->value;
            StructEntry *new_entry               = malloc(sizeof(StructEntry));
            new_entry->unique_tag                = strdup(entry->unique_tag);
            new_entry->struct_from_current_scope = 0; // Reset to false
            hash_table_insert(new_map, node->key, new_entry);
            node = node->next;
        }
    }
    return new_map;
}

// Resolve type
void resolve_type(Type *t)
{
    if (!t)
        return;
    switch (t->kind) {
    case TYPE_STRUCT: {
        StructEntry *entry = hash_table_find(type_table, t->u.struct_t.name);
        if (!entry) {
            fprintf(stderr, "Undeclared structure type %s\n", t->u.struct_t.name);
            exit(1);
        }
        free(t->u.struct_t.name);
        t->u.struct_t.name = strdup(entry->unique_tag);
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

// Resolve expression
void resolve_expr(Expr *e)
{
    if (!e)
        return;
    switch (e->kind) {
    case EXPR_LITERAL:
        return; // No resolution needed
    case EXPR_VAR: {
        VarEntry *entry = hash_table_find(symbol_table, e->u.var);
        if (!entry) {
            fprintf(stderr, "Undeclared variable %s\n", e->u.var);
            exit(1);
        }
        free(e->u.var);
        e->u.var = strdup(entry->unique_name);
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
            fprintf(stderr, "Function call must be a variable\n");
            exit(1);
        }
        VarEntry *entry = hash_table_find(symbol_table, e->u.call.func->u.var);
        if (!entry) {
            fprintf(stderr, "Undeclared function %s\n", e->u.call.func->u.var);
            exit(1);
        }
        free(e->u.call.func->u.var);
        e->u.call.func->u.var = strdup(entry->unique_name);
        Expr *args            = e->u.call.args;
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
        fprintf(stderr, "Unknown expression kind %d\n", e->kind);
        exit(1);
    }
}

// Resolve initializer
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

// Resolve for init
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

// Resolve statement
void resolve_statement(Stmt *s)
{
    if (!s)
        return;
    switch (s->kind) {
    case STMT_EXPR:
        resolve_expr(s->u.expr);
        return;
    case STMT_COMPOUND: {
        HashTable *old_symbol_table = symbol_table;
        HashTable *old_type_table   = type_table;
        symbol_table                = copy_symbol_table(symbol_table);
        type_table                  = copy_type_table(type_table);
        DeclOrStmt *ds              = s->u.compound;
        while (ds) {
            if (ds->kind == DECL_OR_STMT_STMT) {
                resolve_statement(ds->u.stmt);
            } else {
                Declaration *decl = ds->u.decl;
                if (decl->kind == DECL_VAR) {
                    InitDeclarator *id = decl->u.var.declarators;
                    while (id) {
                        VarEntry *entry = hash_table_find(symbol_table, id->name);
                        if (entry && entry->from_current_scope && !entry->has_linkage) {
                            fprintf(stderr, "Duplicate variable declaration %s\n", id->name);
                            exit(1);
                        }
                        symbol_table_insert(
                            id->name, make_named_temporary(id->name), 1,
                            decl->u.var.specifiers->storage == STORAGE_CLASS_EXTERN);
                        free(id->name);
                        id->name = strdup(
                            ((VarEntry *)hash_table_find(symbol_table, id->name))->unique_name);
                        resolve_type(id->type);
                        if (id->init) {
                            resolve_initializer(id->init);
                        }
                        id = id->next;
                    }
                }
            }
            ds = ds->next;
        }
        hash_table_free(symbol_table);
        hash_table_free(type_table);
        symbol_table = old_symbol_table;
        type_table   = old_type_table;
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
        HashTable *old_symbol_table = symbol_table;
        HashTable *old_type_table   = type_table;
        symbol_table                = copy_symbol_table(symbol_table);
        type_table                  = copy_type_table(type_table);
        resolve_for_init(s->u.for_stmt.init);
        if (s->u.for_stmt.init && s->u.for_stmt.init->kind == FOR_INIT_DECL) {
            Declaration *decl = s->u.for_stmt.init->u.decl;
            if (decl->kind == DECL_VAR) {
                InitDeclarator *id = decl->u.var.declarators;
                while (id) {
                    VarEntry *entry = hash_table_find(symbol_table, id->name);
                    if (entry && entry->from_current_scope && !entry->has_linkage) {
                        fprintf(stderr, "Duplicate variable declaration %s\n", id->name);
                        exit(1);
                    }
                    symbol_table_insert(id->name, make_named_temporary(id->name), 1,
                                        decl->u.var.specifiers->storage == STORAGE_CLASS_EXTERN);
                    free(id->name);
                    id->name =
                        strdup(((VarEntry *)hash_table_find(symbol_table, id->name))->unique_name);
                    resolve_type(id->type);
                    if (id->init) {
                        resolve_initializer(id->init);
                    }
                    id = id->next;
                }
            }
        }
        resolve_expr(s->u.for_stmt.condition);
        resolve_expr(s->u.for_stmt.update);
        resolve_statement(s->u.for_stmt.body);
        hash_table_free(symbol_table);
        hash_table_free(type_table);
        symbol_table = old_symbol_table;
        type_table   = old_type_table;
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
        fprintf(stderr, "Unknown statement kind %d\n", e->kind);
        exit(1);
    }
}

// Resolve function declaration
void resolve_function_declaration(ExternalDecl *fd)
{
    VarEntry *entry = hash_table_find(symbol_table, fd->u.function.name);
    if (entry && entry->from_current_scope && !entry->has_linkage) {
        fprintf(stderr, "Duplicate declaration %s\n", fd->u.function.name);
        exit(1);
    }
    symbol_table_insert(fd->u.function.name, fd->u.function.name, 1, 1);

    resolve_type(fd->u.function.type);

    if (fd->u.function.body) {
        HashTable *inner_symbol_table = copy_symbol_table(symbol_table);
        HashTable *inner_type_table   = copy_type_table(type_table);
        HashTable *old_symbol_table   = symbol_table;
        HashTable *old_type_table     = type_table;
        symbol_table                  = inner_symbol_table;
        type_table                    = inner_type_table;

        Param *p = fd->u.function.type->u.function.params;
        while (p) {
            symbol_table_insert(p->name, make_named_temporary(p->name), 1, 0);
            free(p->name);
            p->name = strdup(((VarEntry *)hash_table_find(symbol_table, p->name))->unique_name);
            resolve_type(p->type);
            p = p->next;
        }

        resolve_statement(fd->u.function.body);

        hash_table_free(symbol_table);
        hash_table_free(type_table);
        symbol_table = old_symbol_table;
        type_table   = old_type_table;
    }
}

// Resolve structure declaration
void resolve_structure_declaration(Declaration *d)
{
    char *tag          = d->u.var.specifiers->type->u.struct_t.name;
    StructEntry *entry = hash_table_find(type_table, tag);
    char *unique_tag;
    if (entry && entry->struct_from_current_scope) {
        unique_tag = strdup(entry->unique_tag);
    } else {
        unique_tag = make_named_temporary(tag);
        type_table_insert(tag, unique_tag, 1);
    }
    free(d->u.var.specifiers->type->u.struct_t.name);
    d->u.var.specifiers->type->u.struct_t.name = unique_tag;
    Field *f                                   = d->u.var.specifiers->type->u.struct_t.fields;
    while (f) {
        resolve_type(f->type);
        f = f->next;
    }
}

// Resolve global declaration
void resolve_global_declaration(ExternalDecl *decl)
{
    switch (decl->kind) {
    case EXTERNAL_DECL_FUNCTION:
        resolve_function_declaration(decl);
        break;
    case EXTERNAL_DECL_DECLARATION:
        if (decl->u.declaration->kind == DECL_VAR) {
            InitDeclarator *id = decl->u.declaration->u.var.declarators;
            symbol_table_insert(id->name, id->name, 1, 1);
            free(id->name);
            id->name = strdup(((VarEntry *)hash_table_find(symbol_table, id->name))->unique_name);
            resolve_type(id->type);
            if (id->init) {
                resolve_initializer(id->init);
            }
        } else if (decl->u.declaration->kind == DECL_EMPTY &&
                   decl->u.declaration->u.empty.type->kind == TYPE_STRUCT) {
            resolve_structure_declaration(decl->u.declaration);
        }
        break;
    }
}

// Main resolve function
Program *resolve(Program *program)
{
    type_table   = create_hash_table();
    symbol_table = create_hash_table();

    ExternalDecl *decl = program->decls;
    while (decl) {
        resolve_global_declaration(decl);
        decl = decl->next;
    }

    return program;
}
