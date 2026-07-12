#ifndef CHIBICC_ERROR_H_
#define CHIBICC_ERROR_H_

#include "chibicc_types.h"

void error(char *fmt, ...);
void error_tok(struct Token *token, char *fmt, ...);

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      error("Internal error: Assertion '%s' is failed at %s:%d", #cond,        \
            __FILE__, __LINE__);                                               \
    }                                                                          \
  } while (0)

#endif // CHIBICC_ERROR_H_
