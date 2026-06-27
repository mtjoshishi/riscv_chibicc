#include "codegen.h"

#include <stdio.h>

#include "chibicc_error.h"
#include "chibicc_types.h"

/**
 * @brief Generate assembly codes by traversing the AST.
 * @param node Parsed nodes of AST.
 */
void gen(struct Node *node) {
  CHECK(node != nullptr);

  if (node->kind == NODE_NUM) {
    printf("    addi sp, sp, -8\n");
    printf("    li t0, %d\n", node->val);
    printf("    sd t0, 0(sp)\n");
    return;
  }

  gen(node->lhs);
  gen(node->rhs);

  printf("    ld t1, 0(sp)\n");
  printf("    addi sp, sp, 8\n");
  printf("    ld t0, 0(sp)\n");
  printf("    addi sp, sp, 8\n");

  switch (node->kind) {
  case NODE_ADD:
    printf("    addw t0, t0, t1\n");
    break;
  case NODE_SUB:
    printf("    subw t0, t0, t1\n");
    break;
  case NODE_MUL:
    printf("    mulw t0, t0, t1\n");
    break;
  case NODE_DIV:
    printf("    divw t0, t0, t1\n");
    break;
  default:
    break;
  }

  // Push the temporary calculated results into t0 register.
  printf("    addi sp, sp, -8\n");
  printf("    sd t0, 0(sp)\n");
}
