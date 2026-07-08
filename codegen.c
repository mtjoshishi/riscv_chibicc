#include "codegen.h"

#include <stdio.h>

#include "chibicc_error.h"
#include "chibicc_types.h"

// Registers for the function arguments.
char *argreg[] = {"a0", "a1", "a2", "a3", "a4", "a5"};

// The number of label to go to the end of selection statement.
long labelseq = 0;
char *func_name = "";

/// @brief Generate local variable into the stack.
static void gen_addr(struct Node *node) {
  CHECK(node != nullptr);
  if (node->kind != NODE_VAR)
    error("Not an lvalue.");
  int offset = node->var->offset;
  printf("    addi t0, fp, -%d\n", 16 + offset);
  printf("    addi sp, sp, -8\n");
  printf("    sd t0, 0(sp)\n");
}

/// @brief Load the value from the stack.
static void load() {
  // Pop the address of the top, read the value, and assign to 't0'.
  printf("    ld t0, 0(sp)\n");
  printf("    addi sp, sp, 8\n");
  // Pop the address of the top, read the value, and assign to 't1'.
  printf("    ld t1, 0(t0)\n");
  // Push the read value at 't1' to stack.
  printf("    addi sp, sp, -8\n");
  printf("    sd t1, 0(sp)\n");
}

/// @brief Store the data onto the stack.
static void store() {
  // Pop the rvalue from the top of stack.
  printf("    ld t1, 0(sp)\n");
  printf("    addi sp, sp, 8\n");
  // Pop the lvalue from the top of stack.
  printf("    ld t0, 0(sp)\n");
  printf("    addi sp, sp, 8\n");
  // Assign the value of 't1' into the address of 't0'
  printf("    sd t1, 0(t0)\n");
  // Push the value of 't1'
  printf("    addi sp, sp, -8\n");
  printf("    sd t1, 0(sp)\n");
}

/**
 * @brief Export prologue.
 * @param stack_size The size of stack to push down for the local variables.
 */
static void prologue(int stack_size) {
  // Comply IALIGN=16 boundary.
  printf("    addi sp, sp, -16\n");
  printf("    sd ra, 8(sp)\n");
  printf("    sd fp, 0(sp)\n");
  printf("    addi fp, sp, 16\n");
  printf("    addi sp, sp, -%d\n", stack_size);
}

/**
 * @brief Export epilogue.
 */
static void epilogue() {
  printf(".Lreturn.%s:\n", func_name);
  printf("    addi sp, fp, -16\n");
  printf("    ld ra, 8(sp)\n");
  printf("    ld fp, 0(sp)\n");
  printf("    addi sp, sp, 16\n");
  printf("    ret\n");
}

/**
 * @brief Generate assembly codes by traversing the AST.
 * @param node Parsed nodes of AST.
 */
static void gen(struct Node *node) {
  CHECK(node != nullptr);

  switch (node->kind) {
  case NODE_NUM:
    printf("    li t0, %d\n", node->val);
    printf("    addi sp, sp, -8\n");
    printf("    sd t0, 0(sp)\n");
    return;
  case NODE_EXPR_STMT:
    gen(node->lhs);
    // Removed the evaluated result in progress from the top of stack.
    printf("    ld t0, 0(sp)\n");
    printf("    addi sp, sp, 8\n");
    return;
  case NODE_VAR:
    gen_addr(node);
    load();
    return;
  case NODE_ASSIGN:
    gen_addr(node->lhs);
    gen(node->rhs);
    store();
    return;
  case NODE_IF: {
    long seq = labelseq++;
    /*
     * KEEP IN MIND!
     * The evaluation result of the conditional statement is stored in 't0',
     * and is set to zero if the conditional statement is true.
     */
    if (node->els != nullptr) {
      gen(node->cond);
      printf("    ld t0, 0(sp)\n");
      printf("    addi sp, sp, 8\n");
      printf("    beqz t0, .Lelse%ld\n", seq);
      gen(node->then);
      printf("    j .Lend%ld\n", seq);
      printf(".Lelse%ld:\n", seq);
      gen(node->els);
      printf(".Lend%ld:\n", seq);
    } else {
      gen(node->cond);
      printf("    ld t0, 0(sp)\n");
      printf("    addi sp, sp, 8\n");
      printf("    beqz t0, .Lend%ld\n", seq);
      gen(node->then);
      printf(".Lend%ld:\n", seq);
    }
    return;
  }
  case NODE_WHILE: {
    long seq = labelseq++;
    printf(".Lbegin%ld:\n", seq);
    gen(node->cond);
    printf("    ld t0, 0(sp)\n");
    printf("    addi sp, sp, 8\n");
    printf("    beqz t0, .Lend%ld\n", seq);
    gen(node->then);
    printf("    j .Lbegin%ld\n", seq);
    printf(".Lend%ld:\n", seq);
    return;
  }
  case NODE_FOR: {
    long seq = labelseq++;

    if ((node->init) != nullptr)
      gen(node->init);

    printf(".Lbegin%ld:\n", seq);

    if ((node->cond) != nullptr) {
      gen(node->cond);
      printf("    ld t0, 0(sp)\n");
      printf("    addi sp, sp, 8\n");
      printf("    beqz t0, .Lend%ld\n", seq);
    }

    gen(node->then);

    if ((node->increment) != nullptr)
      gen(node->increment);

    printf("    j .Lbegin%ld\n", seq);
    printf(".Lend%ld:\n", seq);
    return;
  }
  case NODE_BLOCK:
    for (struct Node *n = node->body; n; n = n->next)
      gen(n);
    return;
  case NODE_FUNC_CALL: {
    int args_cnt = 0;
    for (struct Node *arg = node->args; arg != nullptr; arg = arg->next) {
      gen(arg);
      args_cnt += 1;
    }

    // Check whether the alignment is a multiple of 16 bytes.
    bool xalign_chk = ((args_cnt * 8) % 16) == 0;
    if (!xalign_chk)
      printf("    addi sp, sp, -8\n");

    for (int i = args_cnt - 1; i >= 0; i -= 1) {
      printf("    ld %s, 0(sp)\n", argreg[i]);
      printf("    addi sp, sp, 8\n");
    }

    printf("    call %s\n", node->funcname);

    if (!xalign_chk)
      printf("    addi sp, sp, 8\n");

    printf("    addi sp, sp, -8\n");
    printf("    sd a0, 0(sp)\n");
    return;
  }
  case NODE_RETURN:
    gen(node->lhs);
    printf("    ld a0, 0(sp)\n");
    printf("    addi sp, sp, 8\n");
    printf("    j .Lreturn.%s\n", func_name);
    return;
  default:
    break;
  }

  gen(node->lhs);
  gen(node->rhs);

  printf("    ld t1, 0(sp)\n");
  printf("    addi sp, sp, 8\n");
  printf("    ld t0, 0(sp)\n");
  printf("    addi sp, sp, 8\n");

  switch (node->kind) {
  case NODE_ADD:
    printf("    add t0, t0, t1\n");
    break;
  case NODE_SUB:
    printf("    sub t0, t0, t1\n");
    break;
  case NODE_MUL:
    printf("    mul t0, t0, t1\n");
    break;
  case NODE_DIV:
    printf("    div t0, t0, t1\n");
    break;
  case NODE_EQ:
    printf("    sub t0, t0, t1\n");
    // Equals 'sltiu t0, t0, 1'. 'SLT' = Set less than
    printf("    seqz t0, t0\n");
    break;
  case NODE_NE:
    printf("    sub t0, t0, t1\n");
    // Equals 'sltu t0, x0, t0'.
    printf("    snez t0, t0\n");
    break;
  case NODE_LT:
    printf("    slt t0, t0, t1\n");
    break;
  case NODE_LE:
    /*
     * 'lhs <= rhs' means 'not lhs > rhs'. So firstly, check 'lhs > rhs is
     * true?' using 'sgt'.
     */
    printf("    sgt t0, t0, t1\n");
    // And, 'lhs > rhs' is false, zero will be set in 't0'.
    printf("    seqz t0, t0\n");
    break;
  default:
    break;
  }

  // Push the temporary calculated results into t0 register.
  printf("    addi sp, sp, -8\n");
  printf("    sd t0, 0(sp)\n");
}

void codegen(struct Function *prog) {
  CHECK(prog != nullptr);
  for (struct Function *func = prog; func != nullptr; func = func->next) {
    printf(".global %s\n", func->name);
    printf("%s:\n", func->name);
    func_name = func->name;

    prologue(func->stack_size);

    // Emit the code
    for (struct Node *n = prog->node; n != nullptr; n = n->next)
      gen(n);

    epilogue();
  }
}
