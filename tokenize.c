#include "tokenize.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chibicc_error.h"
#include "chibicc_types.h"

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
 * @param op Expected string.
 * @return boolean
 */
bool consume(struct Token **token_ptr, char *op) {
  CHECK(token_ptr != nullptr && *token_ptr != nullptr);
  if ((*token_ptr)->kind != TK_RESERVED || strlen(op) != (*token_ptr)->len ||
      memcmp((*token_ptr)->str, op, (*token_ptr)->len))
    return false;
  *token_ptr = (*token_ptr)->next;
  return true;
}

/**
 * @brief Consumes the current token if it is an identifier.
 * @param token_ptr The pointer of a token to be consumed.
 * @return The current token.
 * @note The given pointer of token will be sought.
 */
struct Token *consume_ident(struct Token **token_ptr) {
  CHECK(token_ptr != nullptr && *token_ptr != nullptr);
  if ((*token_ptr)->kind != TK_IDENT)
    return nullptr;
  struct Token *current = *token_ptr;
  *token_ptr = (*token_ptr)->next;
  return current;
}

/**
 * @brief Seek the token if the expected character.
 * @param token_ptr The pointer of a token to seek.
 * @param op Expected string.
 */
void seek_if_expect(struct Token **token_ptr, char *op) {
  CHECK(token_ptr != nullptr && *token_ptr != nullptr);
  if ((*token_ptr)->kind != TK_RESERVED || strlen(op) != (*token_ptr)->len ||
      memcmp((*token_ptr)->str, op, (*token_ptr)->len))
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
 * @param len The length of string for specified token.
 * @return The pointer of new token.
 */
struct Token *new_token(enum TokenKind kind, struct Token **cur_ptr, char *str,
                        size_t len) {
  CHECK(cur_ptr != nullptr && *cur_ptr != nullptr && str != nullptr);
  struct Token *tok = calloc(1, sizeof(*tok));
  CHECK(tok != nullptr);
  tok->kind = kind;
  tok->str = str;
  tok->len = len;
  (*cur_ptr)->next = tok;
  return tok;
}

/**
 * @brief Checks whether the input string (*p) begins with the specified string
 * (*q).
 * @param *p The input string.
 * @param *q The string that serves as the prefix for `*p`.
 * @return bool If `*p` begins with `*p`, returns true.
 */
static bool startswith(const char *p, const char *q) {
  return memcmp(p, q, strlen(q)) == 0;
}

static bool is_alpha(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

static bool is_alnum(char c) { return is_alpha(c) || ('0' <= c && c <= '9'); }

/**
 */
static char *starts_with_reserved_keyword(const char *p) {
  // Reserved keyword in C.
  static char *kw[] = {"return", "if", "else"};

  for (size_t i = 0; i < sizeof(kw) / sizeof(*kw); i++) {
    size_t reserved_kw_len = strlen(kw[i]);
    if (startswith(p, kw[i]) && !is_alnum(p[reserved_kw_len]))
      return kw[i];
  }

  // Multi-letter puctuator
  static char *operators[] = {"==", "!=", "<=", ">="};

  for (size_t i = 0; i < sizeof(operators) / sizeof(*operators); i++) {
    if (startswith(p, operators[i]))
      return operators[i];
  }

  return nullptr;
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

    // Reserved keywords or multi-letter puctuator
    char *kw = starts_with_reserved_keyword(p);
    if (kw != nullptr) {
      size_t len = strlen(kw);
      cur = new_token(TK_RESERVED, &cur, p, len);
      cur->source_input = input;
      p += len;
      continue;
    }

    if (strchr("+-*/()<>;=", *p)) {
      cur = new_token(TK_RESERVED, &cur, p, 1);
      p++;
      cur->source_input = input;
      continue;
    }

    // Identifier
    if (is_alpha(*p)) {
      char *q = p;
      p++;
      while (is_alnum(*p))
        p++;
      cur = new_token(TK_IDENT, &cur, q, (size_t)(p - q));
      cur->source_input = input;
      continue;
    }

    if (isdigit(*p)) {
      cur = new_token(TK_NUM, &cur, p, 0);
      char *q = p;
      cur->val = (int)strtol(p, &p, 10);
      cur->len = (size_t)(p - q);
      cur->source_input = input;
      continue;
    }

    error_tok(cur, "Invalid token.");
  }

  cur = new_token(TK_EOF, &cur, p, 0);
  cur->source_input = input;
  return head.next;
}
