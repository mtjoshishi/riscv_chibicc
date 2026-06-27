#include "parse.h"

#include <stdlib.h>

#include "chibicc_error.h"
#include "chibicc_types.h"
#include "tokenize.h"

/**
 * @brief Create new node instance.
 * @param node_kind Kind of node.
 * @return Created new node.
 */
struct Node *new_node(enum NodeKind node_kind) {
  struct Node *node = calloc(1, sizeof(*node));
  CHECK(node != nullptr);
  node->kind = node_kind;
  return node;
}

/**
 * @brief Create binary node.
 * @param node_kind Kind of node.
 * @param lhs Left hand side node of binary.
 * @param rhs Right hand side node of binary.
 * @return New binary node.
 */
struct Node *new_binary(enum NodeKind node_kind, struct Node *lhs,
                        struct Node *rhs) {
  CHECK(lhs != nullptr && rhs != nullptr);
  struct Node *node = new_node(node_kind);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

/**
 * @brief Create numerical value node.
 * @param value
 * @return New numerical value node.
 */
struct Node *new_num(int value) {
  struct Node *node = new_node(NODE_NUM);
  node->val = value;
  return node;
}

static struct Node *mul(struct Token **token);
static struct Node *primary(struct Token **token);

/**
 * @brief expr = mul ("+" mul | "-" mul)*
 * @param **token Tokenized source code.
 * @return Node for `expr`.
 */
struct Node *expr(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  struct Node *node = mul(token);

  for (;;) {
    if (consume(token, '+'))
      node = new_binary(NODE_ADD, node, mul(token));
    else if (consume(token, '-'))
      node = new_binary(NODE_SUB, node, mul(token));
    else
      return node;
  }
}

/**
 * @brief mul = primary ("*" primary | "/" primary)
 * @param **token Tokenized source code.
 * @return Node for `mul`.
 */
static struct Node *mul(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  struct Node *node = primary(token);

  for (;;) {
    if (consume(token, '*'))
      node = new_binary(NODE_MUL, node, primary(token));
    else if (consume(token, '/'))
      node = new_binary(NODE_DIV, node, primary(token));
    else
      return node;
  }
}

/**
 * @brief primary = num | "(" expr ")"
 * @param **token Tokenized source code.
 * @return Node for `primary`.
 */
static struct Node *primary(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);

  if (consume(token, '(')) {
    struct Node *node = expr(token);
    seek_if_expect(token, ')');
    return node;
  }

  return new_num(seek_if_expect_number(token));
}
