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
 * @brief Returns the current token if it matches a given string.
 * @param token_ptr The pointer of a token.
 * @param p Given string
 * @return struct Token If matched, returns the corresponding token.
 */
struct Token *peek(struct Token **token_ptr, const char *p) {
  CHECK(token_ptr != nullptr && *token_ptr != nullptr);
  if ((*token_ptr)->kind != TK_RESERVED || strlen(p) != (*token_ptr)->len ||
      memcmp((*token_ptr)->str, p, (*token_ptr)->len))
    return nullptr;
  return *token_ptr;
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
  struct Token *token = peek(token_ptr, op);
  if (token == nullptr)
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
 * @param s Expected string.
 */
void seek_if_expect(struct Token **token_ptr, char *s) {
  CHECK(token_ptr != nullptr && *token_ptr != nullptr);
  struct Token *token = peek(token_ptr, s);
  if (token == nullptr)
    error_tok(*token_ptr, "Expected '%s'", s);
  *token_ptr = (*token_ptr)->next;
}

/**
 * @brief Seek the token if it's identifier.
 * @param token_ptr The pointer of a token to seek.
 * @return The name of identifier.
 */
char *seek_if_expect_ident(struct Token **token_ptr) {
  CHECK(token_ptr != nullptr && *token_ptr != nullptr);
  if ((*token_ptr)->kind != TK_IDENT)
    error_tok(*token_ptr, "Expected an identifier.");
  char *s = strndup((*token_ptr)->str, (*token_ptr)->len);
  *token_ptr = (*token_ptr)->next;
  return s;
}

/**
 * @brief Seek the token if the numeric value, and returns that numeric value.
 * Otherwise, raise the error.
 * @param token_ptr A token to seek. Expected as numeric value.
 * @return Value of a give token.
 */
long seek_if_expect_number(struct Token **token_ptr) {
  CHECK(token_ptr != nullptr && *token_ptr != nullptr);
  if ((*token_ptr)->kind != TK_NUM)
    error_tok(*token_ptr, "The given token is not a numeric value.");
  long val = (*token_ptr)->val;
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
 * @param source_input The overall source code.
 * @param str The input string to be tokenize.
 * @param len The length of string for specified token.
 * @return The pointer of new token.
 */
struct Token *new_token(enum TokenKind kind, struct Token **cur_ptr,
                        char *source_input, char *str, size_t len) {
  CHECK(cur_ptr != nullptr && *cur_ptr != nullptr && str != nullptr);
  struct Token *tok = calloc(1, sizeof(*tok));
  CHECK(tok != nullptr);
  tok->kind = kind;
  tok->source_input = source_input;
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
 * @brief Check whether given string is reserved keyword or not.
 * @param p Input source code.
 * @return char If matched with the reserved keyword, returns that keyword.
 */
static char *starts_with_reserved_keyword(const char *p) {
  // Reserved keyword in C.
  static char *kw[] = {"return", "if",     "else",    "while", "for",
                       "sizeof", "struct", "typedef", "char",  "short",
                       "int",    "long",   "void",    "_Bool"};

  for (size_t i = 0; i < sizeof(kw) / sizeof(*kw); i++) {
    size_t reserved_kw_len = strlen(kw[i]);
    if (startswith(p, kw[i]) && !is_alnum(p[reserved_kw_len]))
      return kw[i];
  }

  // Multi-letter puctuator
  static char *operators[] = {"==", "!=", "<=", ">=", "->"};

  for (size_t i = 0; i < sizeof(operators) / sizeof(*operators); i++) {
    if (startswith(p, operators[i]))
      return operators[i];
  }

  return nullptr;
}

/// @brief Returns escape character.
static char get_escape_char(char c) {
  switch (c) {
  case 'a':
    return '\a';
  case 'b':
    return '\b';
  case 't':
    return '\t';
  case 'n':
    return '\n';
  case 'v':
    return '\v';
  case 'f':
    return '\f';
  case 'r':
    return '\r';
  /*
   * Escape sequence '\e' is not guaranteed to work in C-language.
   * Therefore, use decimal of 27.
   * See: https://gist.github.com/fnky/458719343aabd01cfb17a3a4f7296797
   */
  case 'e':
    return 27;
  case '0':
    return 0;
  default:
    return c;
  }
}

/**
 * @brief Read a given string literal
 */
struct Token *read_string_literal(struct Token **cur, char *start) {
  CHECK(cur != nullptr && *cur != nullptr && start != nullptr);
  CHECK((*cur)->source_input != nullptr);
  char *p = start + 1;
  enum { kLiteralBufLen = 1024 };
  char buf[kLiteralBufLen] = {};
  int len = 0;

  for (;;) {
    if (len == kLiteralBufLen)
      error_at((*cur)->source_input, start, "String literal too large.");
    if (*p == '\0')
      error((*cur)->source_input, start, "Unclosed string literal.");
    if (*p == '"')
      break;

    if (*p == '\\') {
      p++;
      buf[len] = get_escape_char(*p);
    } else {
      buf[len] = *p;
    }
    len += 1;
    p++;
  }

  struct Token *tok = new_token(TK_STR, cur, (*cur)->source_input, start,
                                (size_t)(p - start + 1));
  CHECK(len > 0);
  tok->contents = malloc((size_t)(len + 1));
  CHECK(tok->contents != nullptr);
  memcpy(tok->contents, buf, (size_t)len);
  tok->contents[len] = '\0';
  tok->content_len = len + 1;
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

    // Skip the line comment
    if (startswith(p, "//")) {
      p += 2;
      while (*p != '\n')
        p++;
      continue;
    }

    // Skip block comment
    if (startswith(p, "/*")) {
      char *q = strstr(p + 2, "*/");
      if (q == nullptr)
        error_at(input, p, "Unclosed block comment");
      p = q + 2;
      continue;
    }

    // Reserved keywords or multi-letter puctuator
    char *kw = starts_with_reserved_keyword(p);
    if (kw != nullptr) {
      size_t len = strlen(kw);
      cur = new_token(TK_RESERVED, &cur, input, p, len);
      p += len;
      continue;
    }

    if (strchr("+-*/()<>;={},&[].", *p)) {
      cur = new_token(TK_RESERVED, &cur, input, p, 1);
      p++;
      continue;
    }

    // Identifier
    if (is_alpha(*p)) {
      char *q = p;
      p++;
      while (is_alnum(*p))
        p++;
      cur = new_token(TK_IDENT, &cur, input, q, (size_t)(p - q));
      continue;
    }

    // String literal
    if (*p == '"') {
      cur = read_string_literal(&cur, p);
      p += cur->len;
      continue;
    }

    if (isdigit(*p)) {
      cur = new_token(TK_NUM, &cur, input, p, 0);
      char *q = p;
      cur->val = strtol(p, &p, 10);
      cur->len = (size_t)(p - q);
      continue;
    }

    error_at(input, p, "Invalid token.");
  }

  cur = new_token(TK_EOF, &cur, input, p, 0);
  cur->source_input = input;
  return head.next;
}
