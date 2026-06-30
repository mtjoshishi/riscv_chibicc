#include "chibicc_error.h"
#include "chibicc_types.h"
#include "codegen.h"
#include "parse.h"
#include "tokenize.h"

int main(int argc, char **argv) {
  if (argc != 2)
    error("Invalid number of arguments.");

  struct Token *token = tokenize(argv[1]);
  struct Node *node = program(&token);
  CHECK(node != nullptr);
  codegen(node);

  return 0;
}
