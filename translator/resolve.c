#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Unique_ids.h" // Assume this provides make_named_temporary
#include "ast.h"

// Simple hash table for StringMap
#define HASH_TABLE_SIZE 1024

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

typedef struct HashNode {
    char *key;
    void *value;
    struct HashNode *next;
} HashNode;

typedef struct {
    HashNode *buckets[HASH_TABLE_SIZE];
} HashTable;

HashTable *type_table;
HashTable *symbol_table;

// Hash function
unsigned int hash(const char *str)
{
    unsigned int hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    return hash % HASH_TABLE_SIZE;
}

// Hash table operations
HashTable *create_hash_table()
{
    HashTable *table = malloc(sizeof(HashTable));
    memset(table->buckets, 0, sizeof(table->buckets));
    return table;
}

void hash_table_insert(HashTable *table, const char *key, void *value)
{
    unsigned int index    = hash(key);
    HashNode *node        = malloc(sizeof(HashNode));
    node->key             = strdup(key);
    node->value           = value;
    node->next            = table->buckets[index];
    table->buckets[index] = node;
}

void *hash_table_find(HashTable *table, const char *key)
{
    unsigned int index = hash(key);
    HashNode *node     = table->buckets[index];
    while (node) {
        if (strcmp(node->key, key) == 0)
            return node->value;
        node = node->next;
    }
    return NULL;
}

void hash_table_free(HashTable *table)
{
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        HashNode *node = table->buckets[i];
        while (node) {
            HashNode *next = node->next;
            free(node->key);
            free(node->value); // Assumes value is dynamically allocated
            free(node);
            node = next;
        }
    }
    free(table);
}

// Copy identifier map
HashTable *copy_identifier_map(HashTable *m)
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

// Copy struct map
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
Type *resolve_type(Type *t)
{
    if (!t)
        return t;
    switch (t->kind) {
    case TYPE_STRUCT: {
        StructEntry *entry = hash_table_find(type_table, t->u.struct_t.name);
        if (!entry) {
            fprintf(stderr, "Undeclared structure type %s\n", t->u.struct_t.name);
            exit(1);
        }
        Type *new_type            = malloc(sizeof(Type));
        *new_type                 = *t;
        new_type->u.struct_t.name = strdup(entry->unique_tag);
        return new_type;
    }
    case TYPE_POINTER: {
        Type *new_type             = malloc(sizeof(Type));
        *new_type                  = *t;
        new_type->u.pointer.target = resolve_type(t->u.pointer.target);
        return new_type;
    }
    case TYPE_ARRAY: {
        Type *new_type            = malloc(sizeof(Type));
        *new_type                 = *t;
        new_type->u.array.element = resolve_type(t->u.array.element);
        new_type->u.array.size    = resolve_expr(t->u.array.size);
        return new_type;
    }
    case TYPE_FUNCTION: {
        Type *new_type                   = malloc(sizeof(Type));
        *new_type                        = *t;
        new_type->u.function.return_type = resolve_type(t->u.function.return_type);
        Param *p                         = t->u.function.params;
        Param *new_params = NULL, *last = NULL;
        while (p) {
            Param *new_param = malloc(sizeof(Param));
            *new_param       = *p;
            new_param->type  = resolve_type(p->type);
            new_param->next  = NULL;
            if (last)
                last->next = new_param;
            else
                new_params = new_param;
            last = new_param;
            p    = p->next;
        }
        new_type->u.function.params = new_params;
        return new_type;
    }
    default:
        return t; // Other types unchanged
    }
}

// Resolve expression
Expr *resolve_expr(Expr *e)
{
    if (!e)
        return e;
    Expr *new_expr = malloc(sizeof(Expr));
    *new_expr      = *e;
    switch (e->kind) {
    case EXPR_LITERAL:
        return e; // No resolution needed
    case EXPR_VAR: {
        VarEntry *entry = hash_table_find(symbol_table, e->u.var);
        if (!entry) {
            fprintf(stderr, "Undeclared variable %s\n", e->u.var);
            exit(1);
        }
        new_expr->u.var = strdup(entry->unique_name);
        return new_expr;
    }
    case EXPR_UNARY_OP: {
        new_expr->u.unary_op.expr = resolve_expr(e->u.unary_op.expr);
        return new_expr;
    }
    case EXPR_BINARY_OP: {
        new_expr->u.binary_op.left  = resolve_expr(e->u.binary_op.left);
        new_expr->u.binary_op.right = resolve_expr(e->u.binary_op.right);
        return new_expr;
    }
    case EXPR_ASSIGN: {
        new_expr->u.assign.target = resolve_expr(e->u.assign.target);
        new_expr->u.assign.value  = resolve_expr(e->u.assign.value);
        return new_expr;
    }
    case EXPR_COND: {
        new_expr->u.cond.condition = resolve_expr(e->u.cond.condition);
        new_expr->u.cond.then_expr = resolve_expr(e->u.cond.then_expr);
        new_expr->u.cond.else_expr = resolve_expr(e->u.cond.else_expr);
        return new_expr;
    }
    case EXPR_CAST: {
        new_expr->u.cast.type = resolve_type(e->u.cast.type);
        new_expr->u.cast.expr = resolve_expr(e->u.cast.expr);
        return new_expr;
    }
    case EXPR_CALL: {
        VarEntry *entry = hash_table_find(symbol_table, e->u.call.func->u.var);
        if (!entry) {
            fprintf(stderr, "Undeclared function %s\n", e->u.call.func->u.var);
            exit(1);
        }
        Expr *new_func        = malloc(sizeof(Expr));
        *new_func             = *e->u.call.func;
        new_func->u.var       = strdup(entry->unique_name);
        new_expr->u.call.func = new_func;
        Expr *args            = e->u.call.args;
        Expr *new_args = NULL, *last = NULL;
        while (args) {
            Expr *new_arg = resolve_expr(args);
            new_arg->next = NULL;
            if (last)
                last->next = new_arg;
            else
                new_args = new_arg;
            last = new_arg;
            args = args->next;
        }
        new_expr->u.call.args = new_args;
        return new_expr;
    }
    case EXPR_COMPOUND: {
        new_expr->u.compound_literal.type = resolve_type(e->u.compound_literal.type);
        InitItem *item                    = e->u.compound_literal.init;
        while (item) {
            item->init = resolve_initializer(item->init);
            item       = item->next;
        }
        return new_expr;
    }
    case EXPR_FIELD_ACCESS: {
        new_expr->u.field_access.expr = resolve_expr(e->u.field_access.expr);
        return new_expr;
    }
    case EXPR_PTR_ACCESS: {
        new_expr->u.ptr_access.expr = resolve_expr(e->u.ptr_access.expr);
        return new_expr;
    }
    case EXPR_POST_INC: {
        new_expr->u.post_inc = resolve_expr(e->u.post_inc);
        return new_expr;
    }
    case EXPR_POST_DEC: {
        new_expr->u.post_dec = resolve_expr(e->u.post_dec);
        return new_expr;
    }
    case EXPR_SIZEOF_EXPR: {
        new_expr->u.sizeof_expr = resolve_expr(e->u.sizeof_expr);
        return new_expr;
    }
    case EXPR_SIZEOF_TYPE: {
        new_expr->u.sizeof_type = resolve_type(e->u.sizeof_type);
        return new_expr;
    }
    case EXPR_ALIGNOF: {
        new_expr->u.align_of = resolve_type(e->u.align_of);
        return new_expr;
    }
    case EXPR_GENERIC: {
        new_expr->u.generic.controlling_expr = resolve_expr(e->u.generic.controlling_expr);
        GenericAssoc *assoc                  = e->u.generic.associations;
        while (assoc) {
            if (assoc->kind == GENERIC_ASSOC_TYPE) {
                assoc->u.type_assoc.type = resolve_type(assoc->u.type_assoc.type);
                assoc->u.type_assoc.expr = resolve_expr(assoc->u.type_assoc.expr);
            } else {
                assoc->u.default_assoc = resolve_expr(assoc->u.default_assoc);
            }
            assoc = assoc->next;
        }
        return new_expr;
    }
    default:
        fprintf(stderr, "Unknown expression kind %d\n", e->kind);
        exit(1);
    }
}

// Resolve initializer
Initializer *resolve_initializer(Initializer *init)
{
    if (!init)
        return init;
    switch (init->kind) {
    case INITIALIZER_SINGLE:
        init->u.expr = resolve_expr(init->u.expr);
        return init;
    case INITIALIZER_COMPOUND: {
        InitItem *item = init->u.items;
        while (item) {
            item->init             = resolve_initializer(item->init);
            Designator *designator = item->designators;
            while (designator) {
                if (designator->kind == DESIGNATOR_ARRAY) {
                    designator->u.expr = resolve_expr(designator->u.expr);
                }
                designator = designator->next;
            }
            item = item->next;
        }
        return init;
    }
    }
    return init;
}

// Resolve for init
ForInit *resolve_for_init(ForInit *init)
{
    if (!init)
        return init;
    ForInit *new_init = malloc(sizeof(ForInit));
    *new_init         = *init;
    switch (init->kind) {
    case FOR_INIT_EXPR:
        new_init->u.expr = resolve_expr(init->u.expr);
        break;
    case FOR_INIT_DECL:
        // Defer declaration handling to resolve_statement
        break;
    }
    return new_init;
}

// Resolve statement
Stmt *resolve_statement(Stmt *s)
{
    if (!s)
        return s;
    Stmt *new_stmt = malloc(sizeof(Stmt));
    *new_stmt      = *s;
    switch (s->kind) {
    case STMT_EXPR:
        new_stmt->u.expr = resolve_expr(s->u.expr);
        return new_stmt;
    case STMT_COMPOUND: {
        HashTable *old_symbol_table = symbol_table;
        HashTable *old_type_table   = type_table;
        symbol_table                = copy_identifier_map(symbol_table);
        type_table                  = copy_type_table(type_table);
        DeclOrStmt *ds              = s->u.compound;
        DeclOrStmt *new_ds = NULL, *last = NULL;
        while (ds) {
            DeclOrStmt *new_item = malloc(sizeof(DeclOrStmt));
            *new_item            = *ds;
            if (ds->kind == DECL_OR_STMT_STMT) {
                new_item->u.stmt = resolve_statement(ds->u.stmt);
            } else {
                // Handle declarations (simplified)
                Declaration *decl = ds->u.decl;
                if (decl->kind == DECL_VAR) {
                    InitDeclarator *id = decl->u.var.declarators;
                    while (id) {
                        VarEntry *entry = hash_table_find(symbol_table, id->name);
                        if (entry && entry->from_current_scope && !entry->has_linkage) {
                            fprintf(stderr, "Duplicate variable declaration %s\n", id->name);
                            exit(1);
                        }
                        VarEntry *new_entry           = malloc(sizeof(VarEntry));
                        new_entry->unique_name        = make_named_temporary(id->name);
                        new_entry->from_current_scope = 1;
                        new_entry->has_linkage =
                            (decl->u.var.specifiers->storage == STORAGE_CLASS_EXTERN);
                        hash_table_insert(symbol_table, id->name, new_entry);
                        id->name = strdup(new_entry->unique_name);
                        id->type = resolve_type(id->type);
                        if (id->init) {
                            id->init = resolve_initializer(id->init);
                        }
                        id = id->next;
                    }
                }
            }
            new_item->next = NULL;
            if (last)
                last->next = new_item;
            else
                new_ds = new_item;
            last = new_item;
            ds   = ds->next;
        }
        new_stmt->u.compound = new_ds;
        hash_table_free(symbol_table);
        hash_table_free(type_table);
        symbol_table = old_symbol_table;
        type_table   = old_type_table;
        return new_stmt;
    }
    case STMT_IF: {
        new_stmt->u.if_stmt.condition = resolve_expr(s->u.if_stmt.condition);
        new_stmt->u.if_stmt.then_stmt = resolve_statement(s->u.if_stmt.then_stmt);
        new_stmt->u.if_stmt.else_stmt = resolve_statement(s->u.if_stmt.else_stmt);
        return new_stmt;
    }
    case STMT_SWITCH: {
        new_stmt->u.switch_stmt.expr = resolve_expr(s->u.switch_stmt.expr);
        new_stmt->u.switch_stmt.body = resolve_statement(s->u.switch_stmt.body);
        return new_stmt;
    }
    case STMT_WHILE: {
        new_stmt->u.while_stmt.condition = resolve_expr(s->u.while_stmt.condition);
        new_stmt->u.while_stmt.body      = resolve_statement(s->u.while_stmt.body);
        return new_stmt;
    }
    case STMT_DO_WHILE: {
        new_stmt->u.do_while.body      = resolve_statement(s->u.do_while.body);
        new_stmt->u.do_while.condition = resolve_expr(s->u.do_while.condition);
        return new_stmt;
    }
    case STMT_FOR: {
        HashTable *old_symbol_table = symbol_table;
        HashTable *old_type_table   = type_table;
        symbol_table                = copy_identifier_map(symbol_table);
        type_table                  = copy_type_table(type_table);
        new_stmt->u.for_stmt.init   = resolve_for_init(s->u.for_stmt.init);
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
                    VarEntry *new_entry           = malloc(sizeof(VarEntry));
                    new_entry->unique_name        = make_named_temporary(id->name);
                    new_entry->from_current_scope = 1;
                    new_entry->has_linkage =
                        (decl->u.var.specifiers->storage == STORAGE_CLASS_EXTERN);
                    hash_table_insert(symbol_table, id->name, new_entry);
                    id->name = strdup(new_entry->unique_name);
                    id->type = resolve_type(id->type);
                    if (id->init) {
                        id->init = resolve_initializer(id->init);
                    }
                    id = id->next;
                }
            }
        }
        new_stmt->u.for_stmt.condition = resolve_expr(s->u.for_stmt.condition);
        new_stmt->u.for_stmt.update    = resolve_expr(s->u.for_stmt.update);
        new_stmt->u.for_stmt.body      = resolve_statement(s->u.for_stmt.body);
        hash_table_free(symbol_table);
        hash_table_free(type_table);
        symbol_table = old_symbol_table;
        type_table   = old_type_table;
        return new_stmt;
    }
    case STMT_GOTO:
        // Label resolution not handled; assume valid
        return new_stmt;
    case STMT_CONTINUE:
    case STMT_BREAK:
        return new_stmt; // No resolution needed
    case STMT_RETURN:
        new_stmt->u.expr = resolve_expr(s->u.expr);
        return new_stmt;
    case STMT_LABELED: {
        new_stmt->u.labeled.stmt = resolve_statement(s->u.labeled.stmt);
        return new_stmt;
    }
    case STMT_CASE: {
        new_stmt->u.case_stmt.expr = resolve_expr(s->u.case_stmt.expr);
        new_stmt->u.case_stmt.stmt = resolve_statement(s->u.case_stmt.stmt);
        return new_stmt;
    }
    case STMT_DEFAULT: {
        new_stmt->u.default_stmt = resolve_statement(s->u.default_stmt);
        return new_stmt;
    }
    default:
        fprintf(stderr, "Unknown statement kind %d\n", s->kind);
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
    VarEntry *new_entry           = malloc(sizeof(VarEntry));
    new_entry->unique_name        = strdup(fd->u.function.name);
    new_entry->from_current_scope = 1;
    new_entry->has_linkage        = 1;
    hash_table_insert(symbol_table, fd->u.function.name, new_entry);

    fd->u.function.type = resolve_type(fd->u.function.type);
    if (fd->u.function.body) {
        HashTable *inner_symbol_table = copy_identifier_map(symbol_table);
        HashTable *inner_type_table   = copy_type_table(type_table);
        HashTable *old_symbol_table   = symbol_table;
        HashTable *old_type_table     = type_table;
        symbol_table                  = inner_symbol_table;
        type_table                    = inner_type_table;

        Param *p = fd->u.function.type->u.function.params;
        while (p) {
            VarEntry *param_entry           = malloc(sizeof(VarEntry));
            param_entry->unique_name        = make_named_temporary(p->name);
            param_entry->from_current_scope = 1;
            param_entry->has_linkage        = 0;
            hash_table_insert(symbol_table, p->name, param_entry);
            p->name = strdup(param_entry->unique_name);
            p       = p->next;
        }

        fd->u.function.body = resolve_statement(fd->u.function.body);

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
        unique_tag                           = make_named_temporary(tag);
        StructEntry *new_entry               = malloc(sizeof(StructEntry));
        new_entry->unique_tag                = unique_tag;
        new_entry->struct_from_current_scope = 1;
        hash_table_insert(type_table, tag, new_entry);
    }
    d->u.var.specifiers->type->u.struct_t.name = unique_tag;
    Field *f                                   = d->u.var.specifiers->type->u.struct_t.fields;
    while (f) {
        f->type = resolve_type(f->type);
        f       = f->next;
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
            VarEntry *entry           = malloc(sizeof(VarEntry));
            entry->unique_name        = strdup(decl->u.declaration->u.var.declarators->name);
            entry->from_current_scope = 1;
            entry->has_linkage        = 1;
            hash_table_insert(symbol_table, decl->u.declaration->u.var.declarators->name, entry);
            decl->u.declaration->u.var.declarators->type =
                resolve_type(decl->u.declaration->u.var.declarators->type);
            if (decl->u.declaration->u.var.declarators->init) {
                decl->u.declaration->u.var.declarators->init =
                    resolve_initializer(decl->u.declaration->u.var.declarators->init);
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
