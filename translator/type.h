//
// Helpers for Type.
//
#include "ast.h"

size_t get_size(const Type *t);
size_t get_alignment(const Type *t);
bool is_complete(const Type *t);
bool is_scalar(const Type *t);
bool is_arithmetic(const Type *t);
bool is_integer(const Type *t);
bool is_character(const Type *t);
bool is_pointer(const Type *t);
bool is_complete_pointer(const Type *t);
bool is_signed(const Type *t);
