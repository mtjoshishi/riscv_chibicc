#include "tokenize.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "chibicc_types.h"

/// @brief Reports the error.
void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      error("Internal error: Assertion '%s' is failed at %s:%d", #cond,        \
            __FILE__, __LINE__);                                               \
    }                                                                          \
  } while (0)

/**
 * @brief Reports the error location.
 * @param loc Location where the error occurs.
 * @param fmt Format string of error message.
 */
static void error_tok(struct Token *token, char *fmt, ...) {
  CHECK(token != nullptr);
  va_list ap;
  va_start(ap, fmt);

  char *source = token->source_input;
  ptrdiff_t pos = token->str - source;
  fprintf(stderr, "%s\n", source);
  fprintf(stderr, "%*s", (int)pos, "");
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

/**
 * @brief If the next token is expected character, consume the token and
 * return true. Otherwise, returns false.
 * @param token_ptr The pointer of a token to be consumed.
 * @param op Expected character.
 * @return boolean
 */
bool consume(struct Token **token_ptr, char op) {
  CHECK(token_ptr != nullptr && *token_ptr != nullptr);
  if ((*token_ptr)->kind != TK_RESERVED || (*token_ptr)->str[0] != op)
    return false;
  *token_ptr = (*token_ptr)->next;
  return true;
}

/**
 * @brief Seek the token if the expected character.
 * @param token_ptr The pointer of a token to seek.
 * @param op Expected character.
 */
void seek_if_expect(struct Token **token_ptr, char op) {
  CHECK(token_ptr != nullptr && *token_ptr != nullptr);
  if ((*token_ptr)->kind != TK_RESERVED || (*token_ptr)->str[0] != op)
    error_tok(*token_ptr, "Expected '%c'", op);
  *token_ptr = (*token_ptr)->next;
}

/**
 * @brief Seek the token if the numeric value, and returns that numeric value.
 * Otherwise, raise the error.
 * @param token_ptr A token to seek. Expected as numeric value.
 * @return Value of a give token.
 */
int seek_if_expect_number(struct Token **token_ptr) {
  CHECK(token_ptr != nullptr && *token_ptr != nullptr);
  if ((*token_ptr)->kind != TK_NUM)
    error_tok(*token_ptr, "The given token is not a numeric value.");
  int val = (*token_ptr)->val;
  *token_ptr = (*token_ptr)->next;
  return val;
}

/**
 * @brief Judge a give token is at EOF.
 * @param token_ptr A token to be expected as EOF.
 * @return boolean If a give token is at EOF, returns true.
 */
bool at_eof(struct Token **token_ptr) {
  CHECK(token_ptr != nullptr && *token_ptr != nullptr);
  return (*token_ptr)->kind == TK_EOF;
}

/**
 * @brief Create a new token and concatenate to the current token.
 * @param kind The kind of token.
 * @param cur_ptr The pointer of the current token.
 * @param str The input string to be tokenize.
 * @return The pointer of new token.
 */
struct Token *new_token(enum TokenKind kind, struct Token **cur_ptr,
                        char *str) {
  CHECK(cur_ptr != nullptr && *cur_ptr != nullptr && str != nullptr);
  struct Token *tok = calloc(1, sizeof(*tok));
  CHECK(tok != nullptr);
  tok->kind = kind;
  tok->str = str;
  (*cur_ptr)->next = tok;
  return tok;
}

struct Token *tokenize(char *input) {
  CHECK(input != nullptr);
  char *p = input;
  struct Token head;
  head.next = nullptr;
  struct Token *cur = &head;

  while (*p) {
    // Skip whitespace
    if (isspace(*p)) {
      p++;
      continue;
    }

    if (*p == '+' || *p == '-') {
      cur = new_token(TK_RESERVED, &cur, p++);
      cur->source_input = input;
      continue;
    }

    if (isdigit(*p)) {
      cur = new_token(TK_NUM, &cur, p);
      cur->val = (int)strtol(p, &p, 10);
      cur->source_input = input;
      continue;
    }

    error_tok(cur, "Invalid token.");
  }

  cur = new_token(TK_EOF, &cur, p);
  cur->source_input = input;
  return head.next;
}
