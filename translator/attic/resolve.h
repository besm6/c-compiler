#ifndef RESOLVE_H
#define RESOLVE_H

#include "ast.h"
#include "hash_table.h"

extern HashTable *type_table;
extern HashTable *symbol_table;

void resolve_type(Type *t);
void resolve_expr(Expr *e);
void resolve_statement(Stmt *s);
void resolve_initializer(Initializer *init);
void resolve_for_init(ForInit *init);
void resolve_function_declaration(ExternalDecl *fd);
void resolve_structure_declaration(Declaration *d);
void resolve_global_declaration(ExternalDecl *decl);
Program *resolve(Program *program);

#endif
