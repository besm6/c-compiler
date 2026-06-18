//
// Type-checking for declarations.
//
#include <stdio.h>
#include <string.h>

#include "semantic.h"
#include "string_map.h"
#include "structtab.h"
#include "symtab.h"
#include "typecheck.h"
#include "typetab.h"
#include "xalloc.h"

static bool is_extern(const DeclSpec *spec)
{
    return spec && (spec->storage == STORAGE_CLASS_EXTERN);
}

static bool is_static(const DeclSpec *spec)
{
    return spec && (spec->storage == STORAGE_CLASS_STATIC);
}

// Reject two parameters with the same name in a function declaration or
// definition (C11 §6.7.6.3p9, §6.9.1p6).  Param lists are short, so a simple
// O(n^2) name comparison is fine.  The f(void) sentinel has no name and so is
// harmless here.
static void check_duplicate_params(const Type *fn_type)
{
    for (const Param *a = fn_type->u.function.params; a; a = a->next) {
        if (!a->name)
            continue;
        for (const Param *b = a->next; b; b = b->next) {
            if (b->name && strcmp(a->name, b->name) == 0) {
                fatal_error("Duplicate parameter name %s", a->name);
            }
        }
    }
}

// Register a function declaration (a prototype, not a definition) that arrives
// as a DECL_VAR declarator with function type.  Used for both file-scope
// prototypes ("int f(int);") and block-scope (local) function declarations.
// Functions have external linkage, so symtab_add_fun stores the symbol at file
// scope (level 0) regardless of where the declaration textually appears; this
// lets a later definition or sibling-scope declaration find it and so detects
// redefinitions and conflicting declarations across function bodies.
static void register_function_declaration(InitDeclarator *decl, const DeclSpec *specifiers)
{
    const Type *var_type = decl->type;
    if (decl->init) {
        fatal_error("Function declared with initializer");
    }
    validate_type(var_type);
    bool global = !is_static(specifiers);

    // Strip the void sentinel — same normalization as in typecheck_fn_decl so
    // call-site argument-count checks see zero params for f(void).
    Type *adj  = clone_type(var_type, __func__, __FILE__, __LINE__);
    Param *vsp = adj->u.function.params;
    if (vsp && !vsp->next && vsp->type->kind == TYPE_VOID && !vsp->name) {
        free_param(vsp);
        adj->u.function.params = NULL;
    }
    check_duplicate_params(adj);

    Symbol *existing = symtab_get_opt(decl->name);
    if (existing) {
        if (existing->kind != SYM_FUNC) {
            // A function declaration clashes with a non-function of the same
            // name (e.g. a variable) in scope.
            fatal_error("Duplicate variable declaration %s", decl->name);
        }
        if (!compatible_type(existing->type, adj)) {
            fatal_error("Conflicting declarations for function %s", decl->name);
        }
    }
    bool defined   = existing && existing->kind == SYM_FUNC && existing->u.func.defined;
    bool fn_global = (existing && existing->kind == SYM_FUNC) ? existing->u.func.global : global;
    symtab_add_fun(decl->name, adj, fn_global, defined);
    free_type(decl->type);
    decl->type = adj; // normalized type now owned by AST
}

// Validate struct members for uniqueness and completeness.
static void validate_struct_definition(const char *tag, const Field *members)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    if (structtab_exists(tag)) {
        fatal_error("Structure %s was already declared", tag);
    }

    // Check for duplicate member names.
    StringMap names;
    map_init(&names);
    for (const Field *m = members; m; m = m->next) {
        if (m->kind == FIELD_STATIC_ASSERT) {
            long val;
            if (!try_eval_const_int(m->u.static_assrt.condition, &val))
                fatal_error("_Static_assert condition is not a constant expression");
            if (!val) {
                if (m->u.static_assrt.message)
                    fatal_error("_Static_assert failed: %s", m->u.static_assrt.message);
                else
                    fatal_error("_Static_assert failed");
            }
            continue;
        }
        if (m->u.member.type->kind == TYPE_FUNCTION) {
            fatal_error("Can't declare structure member with function type");
        }
        if (!is_complete(m->u.member.type)) {
            fatal_error("Cannot declare structure member with incomplete type");
        }
        if (m->u.member.name && map_get(&names, m->u.member.name, NULL)) {
            fatal_error("Duplicate member %s in structure %s", m->u.member.name, tag);
        }
        if (m->u.member.name)
            map_insert(&names, m->u.member.name, 0, 0);
        validate_type(m->u.member.type);
    }
    map_destroy(&names);
}

// Register enum constants for an enum type definition.
static void register_enum_constants(const Type *enum_type)
{
    long next_val = 0;
    for (const Enumerator *e = enum_type->u.enum_t.enumerators; e; e = e->next) {
        long val;
        if (e->value) {
            if (!try_eval_const_int(e->value, &val))
                fatal_error("Enum constant '%s' has non-constant initializer", e->name);
        } else {
            val = next_val;
        }
        next_val = val + 1;
        symtab_add_enum_const(e->name, (int)val, scope_level);
    }
}

static int anon_struct_counter = 0;
static void register_inline_struct_defs(const Type *t);

// Register a struct/union type definition in the struct table.
// Precondition: t is TYPE_STRUCT or TYPE_UNION with non-NULL fields, not yet in structtab.
static void register_struct_type(const Type *t)
{
    // Anonymous structs have no tag; assign a unique synthetic one so structtab can store them.
    if (!t->u.struct_t.name) {
        char buf[32];
        snprintf(buf, sizeof(buf), "__anon_%d", ++anon_struct_counter);
        ((Type *)t)->u.struct_t.name = xstrdup(buf);
    }
    // Resolve typedef names in field types in-place (analogous to the anon-name cast above).
    for (Field *f = (Field *)t->u.struct_t.fields; f; f = f->next) {
        if (f->kind != FIELD_STATIC_ASSERT)
            f->u.member.type = resolve_typedef_names(f->u.member.type);
    }
    // Pre-register inline struct/union defs in member types so is_complete() finds them.
    for (const Field *f = t->u.struct_t.fields; f; f = f->next) {
        if (f->kind != FIELD_STATIC_ASSERT)
            register_inline_struct_defs(f->u.member.type);
    }
    TypeKind kind = t->kind;
    validate_struct_definition(t->u.struct_t.name, t->u.struct_t.fields);
    FieldDef *members     = NULL;
    FieldDef **tail       = &members;
    int current_size      = 0;
    int current_alignment = 1;
    for (const Field *f = t->u.struct_t.fields; f; f = f->next) {
        if (f->kind == FIELD_STATIC_ASSERT)
            continue; /* already evaluated in validate_struct_definition */
        int member_alignment = get_alignment(f->u.member.type);
        int offset           = 0;
        if (kind == TYPE_STRUCT)
            offset = round_away_from_zero(member_alignment, current_size);
        *tail = new_member(f->u.member.name,
                           clone_type(f->u.member.type, __func__, __FILE__, __LINE__), offset);
        tail  = &(*tail)->next;
        current_alignment =
            current_alignment > member_alignment ? current_alignment : member_alignment;
        current_size = offset + get_size(f->u.member.type);
    }
    int size = round_away_from_zero(current_alignment, current_size);
    structtab_add_struct(t->u.struct_t.name, current_alignment, size, members, scope_level);
}

// Register any inline struct/union definitions embedded in a type tree.
// Must be called before validate_type() so is_complete() finds them in structtab.
static void register_inline_struct_defs(const Type *t)
{
    if (!t)
        return;
    switch (t->kind) {
    case TYPE_STRUCT:
    case TYPE_UNION:
        if (t->u.struct_t.fields && !structtab_exists(t->u.struct_t.name))
            register_struct_type(t);
        break;
    case TYPE_ARRAY:
        register_inline_struct_defs(t->u.array.element);
        break;
    case TYPE_POINTER:
        register_inline_struct_defs(t->u.pointer.target);
        break;
    case TYPE_FUNCTION:
        register_inline_struct_defs(t->u.function.return_type);
        for (const Param *p = t->u.function.params; p; p = p->next)
            register_inline_struct_defs(p->type);
        break;
    default:
        break;
    }
}

// Type-check a struct/union/enum tag declaration.
static void typecheck_tag_decl(const Declaration *d)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    if (!d->u.empty.type)
        return;
    TypeKind kind = d->u.empty.type->kind;
    if (kind == TYPE_ENUM) {
        if (d->u.empty.type->u.enum_t.enumerators)
            register_enum_constants(d->u.empty.type);
        return;
    }
    if (kind != TYPE_STRUCT && kind != TYPE_UNION)
        return;
    if (!d->u.empty.type->u.struct_t.fields)
        return; // Forward declaration or tag reference — not a definition.
    register_struct_type(d->u.empty.type);
}

// Type-check a local variable declaration.
static void typecheck_local_var_decl(const Declaration *d)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    if (d->u.var.specifiers && d->u.var.specifiers->storage == STORAGE_CLASS_TYPEDEF) {
        for (InitDeclarator *decl = d->u.var.declarators; decl; decl = decl->next) {
            decl->type = resolve_typedef_names(decl->type);
            register_inline_struct_defs(decl->type);
            validate_type(decl->type);
            typetab_add(decl->name, decl->type, scope_level);
        }
        return;
    }
    for (InitDeclarator *decl = d->u.var.declarators; decl; decl = decl->next) {
        decl->type     = resolve_typedef_names(decl->type);
        Type *var_type = decl->type;
        // A block-scope function declaration ("int f(int);" inside a body) has
        // external linkage; register it like a file-scope prototype so later
        // definitions and sibling-scope declarations resolve against it.
        if (var_type->kind == TYPE_FUNCTION) {
            register_function_declaration(decl, d->u.var.specifiers);
            continue;
        }
        if (var_type->kind == TYPE_VOID) {
            fatal_error("No void declarations");
        }
        register_inline_struct_defs(var_type);
        validate_type(var_type);
        if (is_extern(d->u.var.specifiers)) {
            if (decl->init) {
                fatal_error("Initializer on local extern declaration");
            }
            const Symbol *existing = symtab_get_opt(decl->name);
            if (existing && existing->type->kind != var_type->kind) {
                fatal_error("Variable %s redeclared with different type", decl->name);
            }
            if (!existing) {
                symtab_add_static_var(decl->name, var_type, true, INIT_NONE, NULL);
            }
            continue;
        }
        if (!is_complete(var_type)) {
            fatal_error("Cannot define a variable with incomplete type");
        }
        if (is_static(d->u.var.specifiers)) {
            Tac_StaticInit *static_init = build_static_init(var_type, decl->init);
            symtab_add_static_var(decl->name, var_type, false, INIT_INITIALIZED, static_init);
            // Drop initializer
            free_initializer(decl->init);
            decl->init = NULL;
            continue;
        }
        const Symbol *dup = symtab_get_opt(decl->name);
        if (dup && (!dup->has_linkage || dup->kind == SYM_FUNC)) {
            // A no-linkage variable clashing with another no-linkage variable
            // (shadowing, forbidden by design) or with a function of the same
            // name in scope (external vs no linkage, C11 §6.7p3).
            fatal_error("Duplicate variable declaration %s", decl->name);
        }
        symtab_add_automatic_var_type(decl->name, var_type, scope_level);
        decl->init = typecheck_init(var_type, decl->init);
    }
}

// Type-check a function declaration/definition.
static void typecheck_fn_decl(ExternalDecl *d)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    d->u.function.type   = resolve_typedef_names(d->u.function.type);
    const Type *fun_type = d->u.function.type;
    validate_type(fun_type);
    Type *adjusted_type = clone_type(fun_type, __func__, __FILE__, __LINE__);
    if (fun_type->kind == TYPE_FUNCTION) {
        if (fun_type->u.function.return_type->kind == TYPE_ARRAY) {
            fatal_error("A function cannot return an array");
        }
        // In C, f(void) is represented as a single unnamed void param — it means no parameters.
        Param *p = adjusted_type->u.function.params;
        if (p && !p->next && p->type->kind == TYPE_VOID && !p->name) {
            free_param(p);
            adjusted_type->u.function.params = NULL;
            p                                = NULL;
        }
        while (p) {
            if (p->type->kind == TYPE_ARRAY) {
                Type *ptr = new_type(TYPE_POINTER, __func__, __FILE__, __LINE__);
                ptr->u.pointer.target =
                    clone_type(p->type->u.array.element, __func__, __FILE__, __LINE__);
                p->type = ptr;
            } else if (p->type->kind == TYPE_VOID) {
                fatal_error("No void params allowed");
            }
            p = p->next;
        }
    } else {
        fatal_error("Function has non-function type");
    }
    check_duplicate_params(adjusted_type);
    bool has_body            = d->u.function.body != NULL;
    const Param *params      = adjusted_type->u.function.params;
    bool all_params_complete = true;
    for (const Param *p = params; p; p = p->next) {
        if (!is_complete(p->type)) {
            all_params_complete = false;
            break;
        }
    }
    const Type *ret = fun_type->u.function.return_type;
    bool ret_ok     = (ret->kind == TYPE_VOID) || is_complete(ret);
    if (has_body && (!ret_ok || !all_params_complete)) {
        fatal_error("Can't define function with incomplete types");
    }
    bool global      = !is_static(d->u.function.specifiers);
    Symbol *existing = symtab_get_opt(d->u.function.name);
    bool defined     = has_body;
    if (existing) {
        if (existing->type->kind != fun_type->kind) {
            fatal_error("Redeclared function %s with different type", d->u.function.name);
        }
        if (existing->kind == SYM_FUNC) {
            if (!compatible_type(existing->type, adjusted_type)) {
                fatal_error("Conflicting declarations for function %s", d->u.function.name);
            }
            if (existing->u.func.defined && has_body) {
                fatal_error("Defined function %s twice", d->u.function.name);
            }
            if (existing->u.func.global && is_static(d->u.function.specifiers)) {
                fatal_error("Static function declaration follows non-static");
            }
            defined = has_body || existing->u.func.defined;
            global  = existing->u.func.global;
        }
    }
    symtab_add_fun(d->u.function.name, adjusted_type, global, defined);
    if (has_body) {
        if (d->u.function.param_decls) {
            fatal_error("Function parameters in K&R style are not supported");
        }
        scope_increment();
        for (const Param *p = params; p; p = p->next) {
            if (p->name) {
                const Symbol *dup = symtab_get_opt(p->name);
                if (dup && (!dup->has_linkage || dup->kind == SYM_FUNC)) {
                    // A parameter shadowing an enclosing-scope variable, or a
                    // file-scope function of the same name, is forbidden by the
                    // no-shadowing design (external vs no linkage, C11 §6.7p3).
                    fatal_error("Duplicate variable declaration %s", p->name);
                }
            }
            symtab_add_automatic_var_type(p->name, p->type, scope_level);
        }
        d->u.function.body =
            typecheck_statement(fun_type->u.function.return_type, d->u.function.body);
        scope_decrement();
    }
    free_type(d->u.function.type);
    d->u.function.type = adjusted_type; // Update type in place
}

// Type-check a local declaration.
void typecheck_local_decl(Declaration *d)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    switch (d->kind) {
    case DECL_VAR:
        typecheck_local_var_decl(d);
        break;
    case DECL_EMPTY:
        typecheck_tag_decl(d);
        break;
    default:
        fatal_error("Unsupported local declaration kind %d", d->kind);
    }
}

// Type-check a global (file-scope) variable declaration.
static void typecheck_file_scope_var_decl(Declaration *d)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
        print_declaration(stdout, d, 4);
    }
    if (d->u.var.specifiers && d->u.var.specifiers->storage == STORAGE_CLASS_TYPEDEF) {
        for (InitDeclarator *decl = d->u.var.declarators; decl; decl = decl->next) {
            decl->type = resolve_typedef_names(decl->type);
            register_inline_struct_defs(decl->type);
            validate_type(decl->type);
            typetab_add(decl->name, decl->type, scope_level);
        }
        return;
    }
    bool global = !is_static(d->u.var.specifiers);
    for (InitDeclarator *decl = d->u.var.declarators; decl; decl = decl->next) {
        decl->type     = resolve_typedef_names(decl->type);
        Type *var_type = decl->type;

        // A function prototype at file scope (e.g. "int f(int);") arrives here
        // as a DECL_VAR with a function type. Register it as SYM_FUNC so that
        // resolve() can find it when it later processes the definition, and so
        // has_linkage is set correctly to allow the redeclaration.
        if (var_type->kind == TYPE_FUNCTION) {
            register_function_declaration(decl, d->u.var.specifiers);
            continue;
        }

        if (var_type->kind == TYPE_VOID) {
            fatal_error("Void variables not allowed");
        }
        register_inline_struct_defs(var_type);
        validate_type(var_type);

        InitKind init_kind        = is_extern(d->u.var.specifiers) ? INIT_NONE : INIT_TENTATIVE;
        Tac_StaticInit *init_list = NULL;
        if (decl->init) {
            // Pre-register tentatively so the variable's own initializer can reference
            // it via sizeof (e.g. int foo = sizeof(foo); — valid C11 §6.2.1p7).
            if (!symtab_get_opt(decl->name)) {
                symtab_add_static_var(decl->name, var_type, global, INIT_TENTATIVE, NULL);
            }
            init_kind = INIT_INITIALIZED;
            init_list = build_static_init(var_type, decl->init);
        }
        if (!is_complete(var_type) && init_kind != INIT_NONE) {
            fatal_error("Can't define a variable with incomplete type");
        }
        Symbol *existing = symtab_get_opt(decl->name);
        if (existing) {
            if (existing->type->kind != var_type->kind) {
                fatal_error("Variable %s redeclared with different type", decl->name);
            }
            if (existing->kind == SYM_STATIC) {
                if (!is_extern(d->u.var.specifiers) && existing->u.static_var.global != global) {
                    fatal_error("Conflicting variable linkage");
                }
                if (existing->u.static_var.init_kind == INIT_INITIALIZED &&
                    init_kind == INIT_INITIALIZED) {
                    fatal_error("Conflicting global variable definition");
                }
                init_kind = existing->u.static_var.init_kind == INIT_INITIALIZED
                                ? existing->u.static_var.init_kind
                                : init_kind;
                init_list = existing->u.static_var.init_kind == INIT_INITIALIZED
                                ? existing->u.static_var.init_list
                                : init_list;
                global    = is_extern(d->u.var.specifiers) ? existing->u.static_var.global : global;
            }
        }
        symtab_add_static_var(decl->name, var_type, global, init_kind, init_list);

        // Drop initializer
        free_initializer(decl->init);
        decl->init = NULL;
    }
}

// Type-check a global declaration.
void typecheck_global_decl(ExternalDecl *d)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
        print_external_decl(stdout, d, 4);
    }
    switch (d->kind) {
    case EXTERNAL_DECL_FUNCTION:
        typecheck_fn_decl(d);
        if (semantic_debug) {
            printf("--- result:\n");
            print_external_decl(stdout, d, 4);
        }
        break;
    case EXTERNAL_DECL_DECLARATION:
        switch (d->u.declaration->kind) {
        case DECL_VAR:
            typecheck_file_scope_var_decl(d->u.declaration);
            break;
        case DECL_EMPTY:
            typecheck_tag_decl(d->u.declaration);
            break;
        case DECL_STATIC_ASSERT: {
            const Expr *e = typecheck_and_decay(d->u.declaration->u.static_assrt.condition);
            if (!is_scalar(e->type)) {
                fatal_error("_Static_assert condition must have scalar type");
            }
            break;
        }
        }
        if (semantic_debug) {
            printf("--- result:\n");
            print_declaration(stdout, d->u.declaration, 4);
        }
        break;
    }
}
