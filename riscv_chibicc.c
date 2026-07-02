#include "chibicc_error.h"
#include "chibicc_types.h"
#include "codegen.h"
#include "parse.h"
#include "tokenize.h"

int main(int argc, char **argv) {
  if (argc != 2)
    error("Invalid number of arguments.");

  struct Token *token = tokenize(argv[1]);
  struct Program *prog = program(&token);
  CHECK(prog != nullptr);

  // Assign offsets to local variables.
  int offset = 0;
  for (struct Var *var = prog->locals; var; var = var->next) {
    offset += 8;
    var->offset = offset;
  }
  prog->stack_size = offset;

  codegen(prog);

  return 0;
}
