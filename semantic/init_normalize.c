//
// Initializer normalization: brace elision and braced scalars (C11 §6.7.9p11, p17–p21).
//
// normalize_init turns a raw parser initializer for a known type into canonical form,
// which build_static_init and typecheck_init then consume positionally:
//  - scalar: an INITIALIZER_SINGLE (a braced `{ e }` is unwrapped);
//  - char array from a string literal, bare or braced: that INITIALIZER_SINGLE;
//  - array of N: a COMPOUND with exactly N items, in index order;
//  - struct: one item per member, in declaration order;
//  - union: exactly one item (the first member);
//  - an item whose init is NULL is not explicitly initialized, i.e. zero.
// An unsized top-level array gets its length here.
//
// In INIT_AUTOMATIC mode this is the only place leaf expressions are typechecked.  A
// typechecked leaf is marked by a non-NULL Initializer.type (the parser leaves it NULL),
// so an item visited again under brace elision is not typechecked twice.
//
#include <stdio.h>

#include "semantic.h"
#include "structtab.h"
#include "typecheck.h"
#include "xalloc.h"

static Initializer *normalize_compound(Type *t, Initializer *init, InitMode mode);

static bool is_aggregate(const Type *t)
{
    return t->kind == TYPE_ARRAY || t->kind == TYPE_STRUCT || t->kind == TYPE_UNION;
}

// A string literal takes a whole array of integers (the consumer rejects a non-char
// element), as GCC does; for any other array it starts brace elision.
static bool takes_string(const Type *t)
{
    return t->kind == TYPE_ARRAY && is_integer(t->u.array.element);
}

static bool is_string_init(const Initializer *init)
{
    return init->kind == INITIALIZER_SINGLE && init->u.expr->kind == EXPR_LITERAL &&
           init->u.expr->u.literal->kind == LITERAL_STRING;
}

static const char *aggregate_name(const Type *t)
{
    return t->kind == TYPE_ARRAY ? "array" : t->kind == TYPE_STRUCT ? "struct" : "union";
}

// Remove the item at *cur, keeping its init (now owned by the caller).
static Initializer *take_item(InitItem **cur)
{
    InitItem *item    = *cur;
    Initializer *init = item->init;
    if (item->designators)
        fatal_error("Designated initializers are not supported yet");
    *cur = item->next;
    xfree(item);
    return init;
}

// Typecheck a leaf expression once (automatic mode only).
static Initializer *check_leaf(Initializer *init, InitMode mode)
{
    if (mode == INIT_AUTOMATIC && !init->type) {
        init->u.expr = typecheck_and_decay(init->u.expr);
        init->type   = clone_type(init->u.expr->type, __func__, __FILE__, __LINE__);
    }
    return init;
}

// Empty canonical node for aggregate t: one NULL item per element or member, a single
// item for a union, none for an unsized array (it grows as it is filled).
static Initializer *new_canonical(const Type *t)
{
    Initializer *node = new_initializer(INITIALIZER_COMPOUND);
    size_t n          = 1;
    if (t->kind == TYPE_ARRAY) {
        n = t->u.array.size ? get_array_size(t) : 0;
    } else if (t->kind == TYPE_STRUCT) {
        n = 0;
        for (const FieldDef *f = structtab_find(t->u.struct_t.name)->members; f; f = f->next)
            n++;
    }
    InitItem **tail = &node->u.items;
    for (size_t i = 0; i < n; i++) {
        *tail = new_init_item(NULL, NULL);
        tail  = &(*tail)->next;
    }
    return node;
}

static void place(Type *t, Initializer **slot, InitItem **cur, InitMode mode);

// Fill the canonical node of aggregate t from the items at *cur.  When braced, the items
// are t's own brace list and a leftover is an excess element; otherwise (brace elision)
// filling stops once t is full and the enclosing level takes the rest.
static void fill(const Type *t, Initializer *node, InitItem **cur, bool braced, InitMode mode)
{
    InitItem **slot       = &node->u.items;
    const FieldDef *field = t->kind == TYPE_ARRAY ? NULL : structtab_find(t->u.struct_t.name)->members;
    bool unsized          = t->kind == TYPE_ARRAY && !t->u.array.size;

    while (*cur) {
        if (!*slot) {
            if (!unsized) {
                if (braced)
                    fatal_error("Too many elements in %s initializer", aggregate_name(t));
                return;
            }
            *slot = new_init_item(NULL, NULL);
        }
        Type *sub = t->kind == TYPE_ARRAY ? t->u.array.element : field->type;
        place(sub, &(*slot)->init, cur, mode);
        slot = &(*slot)->next;
        if (field)
            field = field->next;
    }
}

// Initialize the subobject of type t, whose canonical slot is *slot, from the item at *cur.
static void place(Type *t, Initializer **slot, InitItem **cur, InitMode mode)
{
    const Type *ut    = unalias(t);
    Initializer *init = (*cur)->init;

    if (init->kind == INITIALIZER_COMPOUND) {
        *slot = normalize_compound(t, take_item(cur), mode);
        return;
    }
    if (!is_aggregate(ut)) {
        *slot = check_leaf(take_item(cur), mode);
        return;
    }
    if (is_string_init(init)) {
        if (takes_string(ut)) {
            *slot = take_item(cur);
            return;
        }
    } else if (mode == INIT_AUTOMATIC && ut->kind != TYPE_ARRAY) {
        // A struct/union-valued expression initializes the whole subobject.
        check_leaf(init, mode);
        if (compatible_type(ut, init->u.expr->type)) {
            *slot = take_item(cur);
            return;
        }
    }
    // Brace elision: the item starts the subobject's own initializer list.
    if (!*slot)
        *slot = new_canonical(ut);
    fill(ut, *slot, cur, false, mode);
}

// Normalize a brace-enclosed initializer for type t; consumes init.
static Initializer *normalize_compound(Type *t, Initializer *init, InitMode mode)
{
    const Type *ut = unalias(t);
    InitItem *items = init->u.items;
    init->u.items   = NULL;
    free_initializer(init);

    if (!is_aggregate(ut)) {
        // A braced scalar (§6.7.9p11).
        if (!items)
            fatal_error("Empty scalar initializer");
        if (items->next)
            fatal_error("Excess elements in scalar initializer");
        Initializer *inner = take_item(&items);
        return inner->kind == INITIALIZER_COMPOUND ? normalize_compound(t, inner, mode)
                                                   : check_leaf(inner, mode);
    }
    if (takes_string(ut) && items && !items->next && !items->designators &&
        is_string_init(items->init)) {
        // A braced string for a char array (§6.7.9p14).
        return take_item(&items);
    }
    Initializer *node = new_canonical(ut);
    fill(ut, node, &items, true, mode);
    return node;
}

Initializer *normalize_init(Type *type, Initializer *init, InitMode mode)
{
    if (semantic_debug) {
        printf("--- %s()\n", __func__);
    }
    Type *t = (Type *)unalias(type);

    if (init->kind == INITIALIZER_SINGLE) {
        // A string for an array is checked by the consumer; it is never decayed here.
        if (t->kind == TYPE_ARRAY && is_string_init(init))
            return init;
        return check_leaf(init, mode);
    }
    bool unsized = t->kind == TYPE_ARRAY && !t->u.array.size;
    init         = normalize_compound(t, init, mode);
    if (unsized && init->kind == INITIALIZER_COMPOUND) {
        size_t n = 0;
        for (const InitItem *item = init->u.items; item; item = item->next)
            n++;
        set_array_size(t, n);
    }
    return init;
}
