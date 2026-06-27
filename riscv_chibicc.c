#include <stdio.h>

#include "chibicc_error.h"
#include "chibicc_types.h"
#include "codegen.h"
#include "parse.h"
#include "tokenize.h"

int main(int argc, char **argv) {
  if (argc != 2)
    error("Invalid number of arguments.");

  struct Token *token = tokenize(argv[1]);
  struct Node *node = expr(&token);

  printf(".globl main\n");
  printf("main:\n");

  // Traverse the AST to emit assembly.
  gen(node);

  /*
   * A result must be at the 't0' register, so pop it to 'a0' register to make
   * it a program exit code.
   */
  printf("    lw a0, 0(sp)\n");
  printf("    addi sp, sp, 8\n");
  printf("    ret\n");
  return 0;
}
