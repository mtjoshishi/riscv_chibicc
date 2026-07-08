#include "parse.h"

#include <stdlib.h>
#include <string.h>

#include "chibicc_error.h"
#include "chibicc_types.h"
#include "tokenize.h"

struct VarList *locals;

struct Var *find_var(struct Token *token) {
  CHECK(token != nullptr);
  for (struct VarList *vl = locals; vl != nullptr; vl = vl->next) {
    struct Var *var = vl->var;
    CHECK(var != nullptr);
    if (var->len == token->len && !memcmp(token->str, var->name, var->len))
      return var;
  }
  return nullptr;
}

/**
 * @brief Create new node instance.
 * @param node_kind Kind of node.
 * @return Created new node.
 */
struct Node *new_node(enum NodeKind node_kind) {
  struct Node *node = calloc(1, sizeof(*node));
  CHECK(node != nullptr);
  *node = (struct Node){};
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
 * @brief Create unary node.
 *  @param node_kind Kind of node.
 *  @param[in] expr Expression
 *  @return New unary node.
 */
struct Node *new_unary(enum NodeKind node_kind, struct Node *expr) {
  CHECK(expr != nullptr);
  struct Node *node = new_node(node_kind);
  node->lhs = expr;
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

/**
 * @brief Create new variable node.
 * @param name The name of local variable.
 * @return New 'NODE_LVAR' node.
 */
struct Node *new_var(struct Var *var) {
  CHECK(var != nullptr);
  struct Node *node = new_node(NODE_VAR);
  node->var = var;
  return node;
}

struct Var *push_var(char *name) {
  struct Var *var = calloc(1, sizeof(*var));
  CHECK(var != nullptr);
  var->name = name;
  var->len = strlen(name);

  struct VarList *vl = calloc(1, sizeof(*vl));
  CHECK(vl != nullptr);
  vl->var = var;
  vl->next = locals;
  locals = vl;
  return var;
}

static struct Function *function(struct Token **token);
static struct Node *stmt(struct Token **token);
static struct Node *expr(struct Token **token);
static struct Node *assign(struct Token **token);
static struct Node *equality(struct Token **token);
static struct Node *relational(struct Token **token);
static struct Node *add(struct Token **token);
static struct Node *mul(struct Token **token);
static struct Node *unary(struct Token **token);
static struct Node *primary(struct Token **token);

/**
 * @brief program = function*
 * @param token Tokenized source codes.
 * @return Node of `program`.
 */
struct Function *program(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  struct Function head = {};
  head.next = nullptr;
  struct Function *cur = &head;

  while (!at_eof(token)) {
    cur->next = function(token);
    cur = cur->next;
  }
  return head.next;
}

/**
 * @brief Returns function parameters as 'params'.
 * @param[in] token Tokenized source code.
 * @return Parameter list.
 */
static struct VarList *read_func_params(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  if (consume(token, ")"))
    return nullptr;

  struct VarList *head = calloc(1, sizeof(*head));
  head->var = push_var(seek_if_expect_ident(token));
  struct VarList *cur = head;

  while (!consume(token, ")")) {
    seek_if_expect(token, ",");
    cur->next = calloc(1, sizeof(*(cur->next)));
    CHECK(cur->next != nullptr);
    cur->next->var = push_var(seek_if_expect_ident(token));
    cur = cur->next;
  }

  return head;
}

/**
 * @brief function = ident "(" params? ")" "{" stmt* "}"
 *          params = ident ("," ident)*
 * @param[in] token Tokenized source code.
 * @return Node of 'function'.
 */
static struct Function *function(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  locals = nullptr;

  struct Function *func = calloc(1, sizeof(*func));
  CHECK(func != nullptr);
  func->name = seek_if_expect_ident(token);
  seek_if_expect(token, "(");
  func->params = read_func_params(token);
  seek_if_expect(token, "{");

  struct Node head = {};
  head.next = nullptr;
  struct Node *cur = &head;
  while (!consume(token, "}")) {
    cur->next = stmt(token);
    CHECK(cur->next != nullptr);
    cur = cur->next;
  }

  func->node = head.next;
  func->locals = locals;
  return func;
}

static struct Node *read_expr_stmt(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  return new_unary(NODE_EXPR_STMT, expr(token));
}

/**
 * @brief stmt = expr ";"
 *             | "{" stmt "}"
 *             | "if" "(" expr ")" stmt ("else" stmt)?
 *             | "while" "(" expr ")" stmt
 *             | "for" "(" expr? ";" expr? ";" expr ";" )" stmt
 *             | "return" expr ";"
 * @param token Tokenized source codes.
 * @return Node of `stmt`.
 */
static struct Node *stmt(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);

  if (consume(token, "if")) {
    struct Node *node = new_node(NODE_IF);
    CHECK(node != nullptr);

    seek_if_expect(token, "(");
    node->cond = expr(token);
    seek_if_expect(token, ")");
    node->then = stmt(token);

    if (consume(token, "else"))
      node->els = stmt(token);

    return node;
  }

  if (consume(token, "while")) {
    struct Node *node = new_node(NODE_WHILE);
    CHECK(node != nullptr);

    seek_if_expect(token, "(");
    node->cond = expr(token);
    seek_if_expect(token, ")");
    node->then = stmt(token);

    return node;
  }

  if (consume(token, "for")) {
    struct Node *node = new_node(NODE_FOR);
    CHECK(node != nullptr);

    seek_if_expect(token, "(");
    if (!consume(token, ";")) {
      node->init = read_expr_stmt(token);
      seek_if_expect(token, ";");
    }

    if (!consume(token, ";")) {
      node->cond = expr(token);
      seek_if_expect(token, ";");
    }

    if (!consume(token, ")")) {
      node->increment = read_expr_stmt(token);
      seek_if_expect(token, ")");
    }
    node->then = stmt(token);

    return node;
  }

  if (consume(token, "return")) {
    struct Node *node = new_unary(NODE_RETURN, expr(token));
    seek_if_expect(token, ";");
    return node;
  }

  if (consume(token, "{")) {
    struct Node head = {};
    head.next = nullptr;
    struct Node *cur = &head;

    while (!consume(token, "}")) {
      cur->next = stmt(token);
      cur = cur->next;
    }

    struct Node *node = new_node(NODE_BLOCK);
    node->body = head.next;
    return node;
  }

  struct Node *node = read_expr_stmt(token);
  seek_if_expect(token, ";");
  return node;
}

/**
 * @brief expr = assign
 * @param **token Tokenized source code.
 * @return Node for `expr`.
 */
static struct Node *expr(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  return assign(token);
}

/**
 * @brief assign = equality ("=" assign)?
 * @param[in] token Tokenized source code.
 * @return Node for `assign`
 */
static struct Node *assign(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  struct Node *node = equality(token);
  if (consume(token, "="))
    node = new_binary(NODE_ASSIGN, node, assign(token));
  return node;
}

/**
 * @brief equality = relational ("==" relational | "!=" relational)*
 * @param **token Tokenized source code.
 * @return Node for `equality`.
 */
static struct Node *equality(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  struct Node *node = relational(token);

  for (;;) {
    if (consume(token, "=="))
      node = new_binary(NODE_EQ, node, relational(token));
    else if (consume(token, "!="))
      node = new_binary(NODE_NE, node, relational(token));
    else
      return node;
  }
}

/**
 * @brief relational = add ("<" add | "<=" add | ">" add | ">=" add)*
 * @param **token Tokenized source code.
 * @return Node for `add`.
 */
static struct Node *relational(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  struct Node *node = add(token);

  for (;;) {
    if (consume(token, "<"))
      node = new_binary(NODE_LT, node, add(token));
    else if (consume(token, "<="))
      node = new_binary(NODE_LE, node, add(token));
    else if (consume(token, ">"))
      node = new_binary(NODE_LT, add(token), node);
    else if (consume(token, ">="))
      node = new_binary(NODE_LE, add(token), node);
    else
      return node;
  }
}

/**
 * @brief add = mul ("+" mul | "-" mul)*
 * @param **token Tokenized source code.
 * @return Node for `mul`.
 */
static struct Node *add(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  struct Node *node = mul(token);

  for (;;) {
    if (consume(token, "+"))
      node = new_binary(NODE_ADD, node, mul(token));
    else if (consume(token, "-"))
      node = new_binary(NODE_SUB, node, mul(token));
    else
      return node;
  }
}

/**
 * @brief mul = unary ("*" unary | "/" unary)
 * @param **token Tokenized source code.
 * @return Node for `mul`.
 */
static struct Node *mul(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  struct Node *node = unary(token);

  for (;;) {
    if (consume(token, "*"))
      node = new_binary(NODE_MUL, node, unary(token));
    else if (consume(token, "/"))
      node = new_binary(NODE_DIV, node, unary(token));
    else
      return node;
  }
}

/**
 * @brief unary = ("+" | "-")? primary
 * @param **token Tokenized source code.
 * @return Finally returns node for `primary`.
 */
static struct Node *unary(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  if (consume(token, "+"))
    return unary(token);
  if (consume(token, "-"))
    return new_binary(NODE_SUB, new_num(0), unary(token));
  return primary(token);
}

static struct Node *func_args(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  if (consume(token, ")"))
    return nullptr;

  struct Node *head = assign(token);
  struct Node *cur = head;
  while (consume(token, ",")) {
    cur->next = assign(token);
    cur = cur->next;
  }
  seek_if_expect(token, ")");
  return head;
}

/**
 * @brief primary = num
 *                | ident func-args?
 *                | "(" expr ")"
 *        args = "(" ")"
 * @param **token Tokenized source code.
 * @return Either value node or node for `expr`.
 */
static struct Node *primary(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);

  if (consume(token, "(")) {
    struct Node *node = expr(token);
    seek_if_expect(token, ")");
    return node;
  }

  struct Token *tok = consume_ident(token);
  if (tok != nullptr) {
    if (consume(token, "(")) {
      struct Node *node = new_node(NODE_FUNC_CALL);
      node->funcname = strndup(tok->str, tok->len);
      node->args = func_args(token);
      return node;
    }

    struct Var *var = find_var(tok);
    if (var == nullptr)
      var = push_var(strndup(tok->str, tok->len));
    return new_var(var);
  }

  return new_num(seek_if_expect_number(token));
}
