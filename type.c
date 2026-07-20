#include "type.h"

#include <stdlib.h>
#include <string.h>

#include "chibicc_error.h"
#include "chibicc_types.h"
#include "chibicc_utils.h"

static struct Type *new_type(enum TypeKind kind, int align) {
  struct Type *ty = calloc(1, sizeof(*ty));
  CHECK(ty != nullptr);
  ty->kind = kind;
  ty->align = align;
  ty->base = nullptr;
  return ty;
}

struct Type *char_type() { return new_type(TYPE_CHAR, 1); }

struct Type *int_type() { return new_type(TYPE_INT, 4); }

struct Type *pointer_to(struct Type *base) {
  CHECK(base != nullptr);
  struct Type *ty = new_type(TYPE_PTR, 8);
  ty->base = base;
  return ty;
}

struct Type *array_of(struct Type *base, int size) {
  CHECK(base != nullptr);
  struct Type *ty = new_type(TYPE_ARRAY, base->align);
  ty->base = base;
  ty->array_size = size;
  return ty;
}

int __size_of(struct Type *ty) {
  CHECK(ty != nullptr);
  switch (ty->kind) {
  case TYPE_CHAR:
    return 1;
  case TYPE_INT:
    return 4;
  case TYPE_PTR:
    return 8;
  case TYPE_ARRAY:
    return __size_of(ty->base) * ty->array_size;
  default:
    CHECK(ty->kind == TYPE_STRUCT);
    struct Member *mem = ty->members;
    while (mem->next != nullptr)
      mem = mem->next;
    int end = mem->offset + __size_of(mem->ty);
    return align_to(end, ty->align);
  }
}

static struct Member *find_member(struct Type *ty, const char *name) {
  CHECK(ty != nullptr && name != nullptr);
  CHECK(ty->kind == TYPE_STRUCT);
  for (struct Member *mem = ty->members; mem != nullptr; mem = mem->next) {
    if (!strcmp(mem->name, name))
      return mem;
  }
  return nullptr;
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
  case NODE_FUNC_CALL:
    [[fallthrough]];
  case NODE_NUM:
    node->ty = int_type();
    return;
  case NODE_VAR:
    node->ty = node->var->ty;
    return;
  case NODE_ADD:
    if (node->rhs->ty->base != nullptr) {
      struct Node *tmp = node->lhs;
      node->lhs = node->rhs;
      node->rhs = tmp;
    }
    if (node->rhs->ty->base != nullptr)
      error_tok(node->tok, "Invalid pointer arithmetic operands.");
    node->ty = node->lhs->ty;
    return;
  case NODE_SUB:
    if (node->rhs->ty->base != nullptr)
      error_tok(node->tok, "Invalid pointer arithmetic operands.");
    node->ty = node->lhs->ty;
    return;
  case NODE_ASSIGN:
    node->ty = node->lhs->ty;
    return;
  case NODE_MEMBER: {
    if (node->lhs->ty->kind != TYPE_STRUCT)
      error_tok(node->tok, "Not a struct.");
    node->member = find_member(node->lhs->ty, node->member_name);
    if (node->member == nullptr)
      error_tok(node->tok, "Specified member does not exist.");
    node->ty = node->member->ty;
    return;
  }
  case NODE_ADDR:
    if (node->lhs->ty->kind == TYPE_ARRAY)
      node->ty = pointer_to(node->lhs->ty->base);
    else
      node->ty = pointer_to(node->lhs->ty);
    return;
  case NODE_DEREF:
    if (node->lhs->ty->base == nullptr)
      error_tok(node->tok, "Invalid pointer dereference.");
    node->ty = node->lhs->ty->base;
    return;
  case NODE_SIZEOF:
    node->kind = NODE_NUM;
    node->ty = int_type();
    node->val = __size_of(node->lhs->ty);
    node->lhs = nullptr;
    return;
  case NODE_STMT_EXPR: {
    struct Node *last = node->body;
    while (last->next != nullptr)
      last = last->next;
    node->ty = last->ty;
    return;
  }
  default:
    return;
  }
}

void add_type(struct Program *prog) {
  CHECK(prog != nullptr);
  for (struct Function *func = prog->functions; func != nullptr;
       func = func->next) {
    for (struct Node *n = func->node; n != nullptr; n = n->next)
      visit(n);
  }
}
