#include "type.h"

#include <stdlib.h>

#include "chibicc_error.h"
#include "chibicc_types.h"

static struct Type *int_type() {
  struct Type *ty = calloc(1, sizeof(*ty));
  CHECK(ty != nullptr);
  ty->kind = TYPE_INT;
  ty->base = nullptr;
  return ty;
}

static struct Type *pointer_to(struct Type *base) {
  CHECK(base != nullptr);
  struct Type *ty = calloc(1, sizeof(*ty));
  CHECK(ty != nullptr);
  ty->kind = TYPE_PTR;
  ty->base = base;
  return ty;
}

static void visit(struct Node *node) {
  if (node == nullptr)
    return;

  visit(node->lhs);
  visit(node->rhs);
  visit(node->cond);
  visit(node->then);
  visit(node->els);
  visit(node->init);
  visit(node->increment);

  for (struct Node *n = node->body; n != nullptr; n = n->next)
    visit(n);
  for (struct Node *n = node->args; n != nullptr; n = n->next)
    visit(n);

  switch (node->kind) {
  case NODE_MUL:
    [[fallthrough]];
  case NODE_DIV:
    [[fallthrough]];
  case NODE_EQ:
    [[fallthrough]];
  case NODE_NE:
    [[fallthrough]];
  case NODE_LT:
    [[fallthrough]];
  case NODE_LE:
    [[fallthrough]];
  case NODE_VAR:
    [[fallthrough]];
  case NODE_FUNC_CALL:
    [[fallthrough]];
  case NODE_NUM:
    node->ty = int_type();
    return;
  case NODE_ADD:
    if (node->rhs->ty->kind == TYPE_PTR) {
      struct Node *tmp = node->lhs;
      node->lhs = node->rhs;
      node->rhs = tmp;
    }
    if (node->rhs->ty->kind == TYPE_PTR)
      error("Invalid pointer arithmetic operands.");
    node->ty = node->lhs->ty;
    return;
  case NODE_SUB:
    if (node->rhs->ty->kind == TYPE_PTR)
      error("Invalid pointer arithmetic operands.");
    node->ty = node->lhs->ty;
    return;
  case NODE_ASSIGN:
    node->ty = node->lhs->ty;
    return;
  case NODE_ADDR:
    node->ty = pointer_to(node->lhs->ty);
    return;
  case NODE_DEREF:
    if (node->lhs->ty->kind == TYPE_PTR)
      node->ty = node->lhs->ty->base;
    else
      node->ty = int_type();
    return;
  default:
    return;
  }
}

void add_type(struct Function *prog) {
  CHECK(prog != nullptr);
  for (struct Function *func = prog; func != nullptr; func = func->next) {
    for (struct Node *n = func->node; n != nullptr; n = n->next)
      visit(n);
  }
}
