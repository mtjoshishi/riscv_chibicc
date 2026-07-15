#include "chibicc_error.h"
#include "chibicc_types.h"
#include "codegen.h"
#include "parse.h"
#include "tokenize.h"
#include "type.h"

#define ALIGN_TO(offset, align) (((offset) + (align) - 1) & ~((align) - 1))
#define ALIGN_TO_16(offset) ALIGN_TO((offset), 16)

int main(int argc, char **argv) {
  if (argc != 2)
    error("Invalid number of arguments.");

  struct Token *token = tokenize(argv[1]);
  struct Function *prog = program(&token);
  CHECK(prog != nullptr);
  add_type(prog);

  // Assign offsets to local variables.
  for (struct Function *func = prog; func != nullptr; func = func->next) {
    int offset = 0;
    for (struct VarList *vl = func->locals; vl != nullptr; vl = vl->next) {
      struct Var *var = vl->var;
      offset += __size_of(var->ty);
      var->offset = offset;
    }
    func->stack_size = ALIGN_TO_16(offset);
  }

  codegen(prog);

  return 0;
}
