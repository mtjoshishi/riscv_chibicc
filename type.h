// Defines type
#ifndef TYPE_H_
#define TYPE_H_

#include "chibicc_types.h"

// Define minimum and maximum values of int type.
#define kIntMin (int)(1U << (sizeof(int) * 8 - 1))
#define kIntMax ~kIntMin

struct Type *void_type();
struct Type *bool_type();
struct Type *char_type();
struct Type *short_type();
struct Type *int_type();
struct Type *long_type();
struct Type *enum_type(struct Type *basetype);
struct Type *func_type(struct Type *return_ty);
struct Type *pointer_to(struct Type *base);
struct Type *array_of(struct Type *base, long size);
long __size_of(struct Type *ty);

void add_type(struct Program *prog);

#endif // TYPE_H_
