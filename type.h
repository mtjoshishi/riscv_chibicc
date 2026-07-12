// Defines type
#ifndef TYPE_H_
#define TYPE_H_

#include "chibicc_types.h"

struct Type *int_type();
struct Type *pointer_to(struct Type *base);

void add_type(struct Function *prog);

#endif // TYPE_H_
