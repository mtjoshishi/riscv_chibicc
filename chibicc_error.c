#include "chibicc_error.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/// @brief Reports the error.
void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

/**
 * @brief Reports an error message in the following format and exit.
 * @example
 * foo.c:10: x = y + 1;
 *               ^ Undefined variable.
 * @param user_input The overall input.
 * @param loc The error location.
 * @param fmt The format string of error message.
 * @param ap The list of variable arguments.
 */
static void verror_at(const char *user_input, const char *loc, const char *fmt,
                      va_list ap) {
  CHECK(filename != nullptr && user_input != nullptr && loc != nullptr &&
        fmt != nullptr);
  const char *line = loc;

  while (user_input < line && line[-1] != '\n')
    line--;

  const char *end = loc;
  while (*end != '\0' && *end != '\n')
    end++;

  // Get a line number.
  int line_num = 1;
  for (const char *p = user_input; p < line; p++) {
    if (*p == '\n')
      line_num++;
  }

  // Print out the line.
  int indent = fprintf(stderr, "%s:%d: ", filename, line_num);
  fprintf(stderr, "%.*s\n", (int)(end - line), line);

  // Show the error message.
  int pos = (int)(loc - line) + indent;
  fprintf(stderr, "%*s", pos, "");
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

/**
 * @brief Reports an error message in format style and exit.
 * @example
 * foo.c:10: x = y + 1;
 *               ^ Undefined variable.
 * @param user_input The overall input.
 * @param loc The error location.
 * @param fmt The format string of error message.
 * @param ap The list of variable arguments.
 */
void error_at(const char *user_input, const char *loc, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  verror_at(user_input, loc, fmt, ap);
}

/**
 * @brief Reports the error location using token.
 * @note Do not use before tokenization of source code.
 * @param loc Location where the error occurs.
 * @param fmt Format string of error message.
 */
void error_tok(struct Token *token, char *fmt, ...) {
  CHECK(token != nullptr);
  va_list ap;
  va_start(ap, fmt);
  verror_at(token->source_input, token->str, fmt, ap);
}
