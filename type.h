// Defines type
#ifndef TYPE_H_
#define TYPE_H_

#include "chibicc_types.h"

struct Type *char_type();
struct Type *short_type();
struct Type *int_type();
struct Type *long_type();
struct Type *func_type(struct Type *return_ty);
struct Type *pointer_to(struct Type *base);
struct Type *array_of(struct Type *base, int size);
int __size_of(struct Type *ty);

void add_type(struct Program *prog);

#endif // TYPE_H_
