#ifndef TOKENIZE_H_
#define TOKENIZE_H_

#include <stdbool.h>

#include "chibicc_types.h"

struct Token *peek(struct Token **token_ptr, const char *s);
bool consume(struct Token **token_ptr, char *op);
struct Token *consume_ident(struct Token **token_ptr);
void seek_if_expect(struct Token **token_ptr, char *op);
char *seek_if_expect_ident(struct Token **token_ptr);
int seek_if_expect_number(struct Token **token_ptr);
bool at_eof(struct Token **token_ptr);
struct Token *new_token(enum TokenKind kind, struct Token **cur_ptr, char *str,
                        size_t len);
struct Token *tokenize(char *p);

#endif // TOKENIZE_H_
