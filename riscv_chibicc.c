#include <stdio.h>

#include "chibicc_error.h"
#include "chibicc_types.h"
#include "tokenize.h"

int main(int argc, char **argv) {
  if (argc != 2) {
    error("Invalid number of arguments.");
    return 1;
  }

  struct Token *token = tokenize(argv[1]);

  printf(".globl main\n");
  printf("main:\n");
  printf("    li a0,%d\n", seek_if_expect_number(&token));

  while (!at_eof(&token)) {
    if (consume(&token, '+')) {
      printf("    addi a0,a0,%d\n", seek_if_expect_number(&token));
      continue;
    }

    seek_if_expect(&token, '-');
    printf("    addi a0,a0,-%d\n", seek_if_expect_number(&token));
  }

  printf("    ret\n");
  return 0;
}
