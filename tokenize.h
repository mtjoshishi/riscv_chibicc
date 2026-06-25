#ifndef TOKENIZE_H_
#define TOKENIZE_H_

#include <stdbool.h>

#include "chibicc_types.h"

void error(char *fmt, ...);
bool consume(struct Token **token_ptr, char op);
void seek_if_expect(struct Token **token_ptr, char op);
int seek_if_expect_number(struct Token **token_ptr);
bool at_eof(struct Token **token_ptr);
struct Token *new_token(enum TokenKind kind, struct Token **cur_ptr, char *str);
struct Token *tokenize(char *p);

#endif // TOKENIZE_H_
