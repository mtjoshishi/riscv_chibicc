#include "codegen.h"

#include <stdio.h>

#include "chibicc_error.h"
#include "chibicc_types.h"
#include "type.h"

// Registers for the function arguments.
char *argreg[] = {"a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7"};

// The number of label to go to the end of selection statement.
long labelseq = 0;
char *func_name = "";

static void gen(struct Node *node);

/// @brief Generate local variable into the stack.
static void gen_addr(struct Node *node) {
  CHECK(node != nullptr);
  switch (node->kind) {
  case NODE_VAR: {
    struct Var *var = node->var;
    int offset = -(16 + var->offset);
    if (var->is_local)
      /*
       * RISC-V's integer immediate instructions accept 12-bit signed integers.
       * Since overflow is ignored, numbers less than -2048 or greater than 2047
       * are not accepted. If the stack size exceeds the range of a 12-bit
       * signed integer, it must first be stored in a register.
       */
      // TODO: Add helper function of immediate instructions.
      if (offset >= -2028 && offset <= 2047) {
        printf("    addi t0, fp, %d\n", offset);
      } else {
        printf("    li t1, %d\n", offset);
        printf("    add t0, fp, t1\n");
      }
    else
      printf("    lla t0, %s\n", var->name);
    printf("    addi sp, sp, -8\n");
    printf("    sd t0, 0(sp)\n");
    return;
  }
  case NODE_DEREF:
    gen(node->lhs);
    return;
  case NODE_MEMBER:
    gen_addr(node->lhs);
    printf("    ld t0, 0(sp)\n");
    printf("    addi t0, t0, %d\n", node->member->offset);
    printf("    sd t0, 0(sp)\n");
    return;
  default:
    error_tok(node->tok,
              "Expected 'NODE_VAR' and 'NODE_DEREF' which are lvalue.");
  }
}

/**
 * @brief Generate lvalue.
 * @param[in] node struct Node*
 */
static void gen_lval(struct Node *node) {
  if (node->ty->kind == TYPE_ARRAY)
    error_tok(node->tok, "Not an lvalue.");
  gen_addr(node);
}

/// @brief Load the value from the stack.
static void load(struct Type *ty) {
  // Pop the address of the top and assign to 't0'.
  printf("    ld t0, 0(sp)\n");

  // Read the value and assign to 't0'.
  int sz = __size_of(ty);
  if (sz == 1) {
    printf("    lb t0, 0(t0)\n");
  } else if (sz == 4) {
    printf("    lw t0, 0(t0)\n");
  } else {
    CHECK(sz == 8);
    printf("    ld t0, 0(t0)\n");
  }

  // Push the read value at 't0' to stack.
  printf("    sd t0, 0(sp)\n");
}

/// @brief Store the data onto the stack.
static void store(struct Type *ty) {
  // Pop the rvalue from the top of stack.
  printf("    ld t1, 0(sp)\n");
  printf("    addi sp, sp, 8\n");

  // Pop the lvalue from the top of stack.
  printf("    ld t0, 0(sp)\n");
  printf("    addi sp, sp, 8\n");

  // Assign the value of 't1' into the address of 't0'
  int sz = __size_of(ty);
  if (sz == 1) {
    printf("    sb t1, 0(t0)\n");
  } else if (sz == 4) {
    printf("    sw t1, 0(t0)\n");
  } else {
    CHECK(sz == 8);
    printf("    sd t1, 0(t0)\n");
  }

  // Push the value of 't1'
  printf("    addi sp, sp, -8\n");
  if (sz == 1) {
    printf("    sb t1, 0(sp)\n");
  } else if (sz == 4) {
    printf("    sw t1, 0(sp)\n");
  } else {
    CHECK(sz == 8);
    printf("    sd t1, 0(sp)\n");
  }
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
  /*
   * RISC-V's integer immediate instructions accept 12-bit signed integers.
   * Since overflow is ignored, numbers less than -2048 or greater than 2047
   * are not accepted. If the stack size exceeds the range of a 12-bit signed
   * integer, it must first be stored in a register.
   */
  if (-stack_size >= -2048) {
    printf("    addi sp, sp, %d\n", -stack_size);
  } else {
    printf("    li t0, %d\n", -stack_size);
    printf("    add sp, sp, t0\n");
  }
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
  case NODE_NULL:
    return;
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
    [[fallthrough]];
  case NODE_MEMBER:
    gen_addr(node);
    if (node->ty->kind != TYPE_ARRAY)
      load(node->ty);
    return;
  case NODE_ASSIGN:
    gen_lval(node->lhs);
    gen(node->rhs);
    store(node->ty);
    return;
  case NODE_ADDR:
    gen_addr(node->lhs);
    return;
  case NODE_DEREF:
    gen(node->lhs);
    if (node->ty->kind != TYPE_ARRAY)
      load(node->ty);
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
    [[fallthrough]];
  case NODE_STMT_EXPR:
    for (struct Node *n = node->body; n; n = n->next)
      gen(n);
    return;
  case NODE_FUNC_CALL: {
    int args_cnt = 0;
    for (struct Node *arg = node->args; arg != nullptr; arg = arg->next) {
      gen(arg);
      args_cnt += 1;
    }

    for (int i = args_cnt - 1; i >= 0; i -= 1) {
      printf("    ld %s, 0(sp)\n", argreg[i]);
      printf("    addi sp, sp, 8\n");
    }

    printf("    call %s\n", node->funcname);
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
    if (node->ty->base != nullptr) {
      printf("    li t2, %d\n", __size_of(node->ty->base));
      printf("    mul t1, t1, t2\n");
    }
    printf("    add t0, t0, t1\n");
    break;
  case NODE_SUB:
    if (node->ty->base != nullptr) {
      printf("    li t2, %d\n", __size_of(node->ty->base));
      printf("    mul t1, t1, t2\n");
    }
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

static void load_arg(struct Var *var, int idx) {
  int sz = __size_of(var->ty);
  if (sz == 1) {
    printf("    sb %s, %d(fp)\n", argreg[idx], -(16 + var->offset));
  } else if (sz == 4) {
    printf("    sw %s, %d(fp)\n", argreg[idx], -(16 + var->offset));
  } else {
    CHECK(sz == 8);
    printf("    sd %s, %d(fp)\n", argreg[idx], -(16 + var->offset));
  }
}

/// @brief Emit the '.data' entry
static void emit_data(struct Program *prog) {
  CHECK(prog != nullptr);
  printf(".data\n");

  for (struct VarList *vl = prog->globals; vl != nullptr; vl = vl->next) {
    struct Var *var = vl->var;
    printf("%s:\n", var->name);

    if (var->contents == nullptr) {
      printf("    .zero %d\n", __size_of(var->ty));
      continue;
    }

    for (int i = 0; i < var->content_len; i++) {
      printf("    .byte %d\n", var->contents[i]);
    }
  }
}

/// @brief Emit the '.text' entry
static void emit_text(struct Program *prog) {
  CHECK(prog != nullptr);
  printf(".text\n");

  for (struct Function *func = prog->functions; func != nullptr;
       func = func->next) {
    printf(".global %s\n", func->name);
    printf("%s:\n", func->name);
    func_name = func->name;

    prologue(func->stack_size);

    // Push arguments to the stack
    int i = 0;
    for (struct VarList *vl = func->params; vl != nullptr; vl = vl->next)
      load_arg(vl->var, i++);

    // Emit the code
    for (struct Node *n = func->node; n != nullptr; n = n->next)
      gen(n);

    epilogue();
  }
}

void codegen(struct Program *prog) {
  CHECK(prog != nullptr);
  emit_data(prog);
  emit_text(prog);
}
