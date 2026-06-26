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
