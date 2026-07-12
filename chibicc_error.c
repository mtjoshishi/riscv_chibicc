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
 * @brief Reports the error location.
 * @param loc Location where the error occurs.
 * @param fmt Format string of error message.
 */
void error_tok(struct Token *token, char *fmt, ...) {
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
