#include "parse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chibicc_error.h"
#include "chibicc_types.h"
#include "chibicc_utils.h"
#include "tokenize.h"
#include "type.h"

struct VarList *locals;
struct VarList *globals;
struct VarList *scope;

/// @brief Find a variable by name
struct Var *find_var(struct Token *token) {
  CHECK(token != nullptr);
  for (struct VarList *vl = scope; vl != nullptr; vl = vl->next) {
    struct Var *var = vl->var;
    CHECK(var != nullptr);
    if (var->len == token->len && !memcmp(token->str, var->name, var->len))
      return var;
  }

  for (struct VarList *vl = globals; vl != nullptr; vl = vl->next) {
    struct Var *gvar = vl->var;
    if (gvar->len == token->len && !memcmp(token->str, gvar->name, gvar->len))
      return gvar;
  }
  return nullptr;
}

/**
 * @brief Create new node instance.
 * @param node_kind Kind of node.
 * @param[in] tok Representative token.
 * @return Created new node.
 */
struct Node *new_node(enum NodeKind node_kind, struct Token *tok) {
  CHECK(tok != nullptr);
  struct Node *node = calloc(1, sizeof(*node));
  CHECK(node != nullptr);
  *node = (struct Node){};
  node->kind = node_kind;
  node->tok = tok;
  return node;
}

/**
 * @brief Create binary node.
 * @param node_kind Kind of node.
 * @param lhs Left hand side node of binary.
 * @param rhs Right hand side node of binary.
 * @param[in] tok Representative token.
 * @return New binary node.
 */
struct Node *new_binary(enum NodeKind node_kind, struct Node *lhs,
                        struct Node *rhs, struct Token *tok) {
  CHECK(lhs != nullptr && rhs != nullptr && tok != nullptr);
  struct Node *node = new_node(node_kind, tok);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

/**
 * @brief Create unary node.
 * @param node_kind Kind of node.
 * @param[in] expr Expression
 * @param[in] tok Representative token.
 * @return New unary node.
 */
struct Node *new_unary(enum NodeKind node_kind, struct Node *expr,
                       struct Token *tok) {
  CHECK(expr != nullptr && tok != nullptr);
  struct Node *node = new_node(node_kind, tok);
  node->lhs = expr;
  return node;
}

/**
 * @brief Create numerical value node.
 * @param value
 * @param[in] tok Representative token.
 * @return New numerical value node.
 */
struct Node *new_num(int value, struct Token *tok) {
  CHECK(tok != nullptr);
  struct Node *node = new_node(NODE_NUM, tok);
  node->val = value;
  return node;
}

/**
 * @brief Create new variable node.
 * @param name The name of local variable.
 * @param[in] tok Representative token.
 * @return New 'NODE_LVAR' node.
 */
struct Node *new_var(struct Var *var, struct Token *tok) {
  CHECK(var != nullptr && tok != nullptr);
  struct Node *node = new_node(NODE_VAR, tok);
  node->var = var;
  return node;
}

/**
 * @brief Push the local variable to the 'VarList'.
 * @param name The name of local variable.
 * @param ty The type of local variable.
 * @param is_local Whether the scope of variable is in local or global.
 * @return Local variable object.
 */
struct Var *push_var(char *name, struct Type *ty, bool is_local) {
  struct Var *var = calloc(1, sizeof(*var));
  CHECK(var != nullptr);
  var->name = name;
  var->len = strlen(name);
  var->ty = ty;
  var->is_local = is_local;

  struct VarList *vl = calloc(1, sizeof(*vl));
  CHECK(vl != nullptr);
  vl->var = var;

  if (is_local) {
    vl->next = locals;
    locals = vl;
  } else {
    vl->next = globals;
    globals = vl;
  }

  struct VarList *sc = calloc(1, sizeof(*sc));
  CHECK(sc != nullptr);
  sc->var = var;
  sc->next = scope;
  scope = sc;

  return var;
}

static char *new_label() {
  static int cnt = 0;
  enum { kBufLen = 256 };
  char buf[kBufLen] = {};

  sprintf(buf, ".LC%d", cnt++);
  return strndup(buf, 20);
}

static struct Function *function(struct Token **token);
static struct Type *basetype(struct Token **token);
static struct Type *struct_decl(struct Token **token);
static struct Member *member_declaration_list(struct Token **token);
static void global_var(struct Token **token);
static struct Node *declaration(struct Token **token);
static bool is_typename(struct Token **token);
static struct Node *stmt(struct Token **token);
static struct Node *expr(struct Token **token);
static struct Node *assign(struct Token **token);
static struct Node *equality(struct Token **token);
static struct Node *relational(struct Token **token);
static struct Node *add(struct Token **token);
static struct Node *mul(struct Token **token);
static struct Node *unary(struct Token **token);
static struct Node *postfix(struct Token **token);
static struct Node *primary(struct Token **token);

static bool is_function(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  struct Token *snapshot = *token;
  basetype(&snapshot);
  bool is_func =
      (consume_ident(&snapshot) != nullptr) && consume(&snapshot, "(");
  return is_func;
}

/**
 * @brief program = (global-var | function)*
 * @param token Tokenized source codes.
 * @return Node of `program`.
 */
struct Program *program(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  struct Function head = {};
  head.next = nullptr;
  struct Function *cur = &head;
  globals = nullptr;

  while (!at_eof(token)) {
    if (is_function(token)) {
      cur->next = function(token);
      cur = cur->next;
    } else {
      global_var(token);
    }
  }

  struct Program *prog = calloc(1, sizeof(*prog));
  CHECK(prog != nullptr);
  prog->globals = globals;
  prog->functions = head.next;
  return prog;
}

// @brief basetype = ("char" | "int" | struct-decl ) "*"*
static struct Type *basetype(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  struct Type *ty = nullptr;
  if (consume(token, "char"))
    ty = char_type();
  else if (consume(token, "int"))
    ty = int_type();
  else
    ty = struct_decl(token);

  while (consume(token, "*"))
    ty = pointer_to(ty);
  return ty;
}

static struct Type *read_type_suffix(struct Token **token, struct Type *base) {
  CHECK(token != nullptr && *token != nullptr);
  CHECK(base != nullptr);
  if (!consume(token, "["))
    return base;
  int sz = seek_if_expect_number(token);
  seek_if_expect(token, "]");
  base = read_type_suffix(token, base);
  return array_of(base, sz);
}

/**
 * @brief struct-decl = "struct" "{ member-declaration-list }"
 * @param token The tokenized source code.
 */
static struct Type *struct_decl(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  seek_if_expect(token, "struct");
  seek_if_expect(token, "{");

  struct Member head = {};
  head.next = nullptr;
  struct Member *cur = &head;

  while (!consume(token, "}")) {
    cur->next = member_declaration_list(token);
    cur = cur->next;
  }

  struct Type *ty = calloc(1, sizeof(*ty));
  ty->kind = TYPE_STRUCT;
  ty->members = head.next;

  /*
   * Assign offsets within the struct to members.
   * The alignment of all struct member is going to be aligned to
   * 'mem->ty->align'.
   */
  int offset = 0;
  for (struct Member *mem = ty->members; mem != nullptr; mem = mem->next) {
    offset = align_to(offset, mem->ty->align);
    mem->offset = offset;
    offset += __size_of(mem->ty);

    if (ty->align < mem->ty->align)
      ty->align = mem->ty->align;
  }
  return ty;
}

/**
 * @brief member-declaration-list = basetype ident ("[ num ]")* ";"
 * @param token The tokenized source codes.
 */
static struct Member *member_declaration_list(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  struct Member *mem = calloc(1, sizeof(*mem));
  mem->ty = basetype(token);
  mem->name = seek_if_expect_ident(token);
  mem->ty = read_type_suffix(token, mem->ty);
  seek_if_expect(token, ";");
  return mem;
}

static struct VarList *read_func_param(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  struct Type *ty = basetype(token);
  char *name = seek_if_expect_ident(token);
  ty = read_type_suffix(token, ty);

  struct VarList *vl = calloc(1, sizeof(*vl));
  CHECK(vl != nullptr);
  vl->var = push_var(name, ty, true);
  return vl;
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

  struct VarList *head = read_func_param(token);
  struct VarList *cur = head;

  while (!consume(token, ")")) {
    seek_if_expect(token, ",");
    cur->next = read_func_param(token);
    cur = cur->next;
  }

  return head;
}

/// @brief global-var = basetype ident ("[ num ]")* ";"
static void global_var(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  struct Type *ty = basetype(token);
  char *name = seek_if_expect_ident(token);
  ty = read_type_suffix(token, ty);
  seek_if_expect(token, ";");
  push_var(name, ty, false);
}

/**
 * @brief function = basetype ident "(" params? ")" "{" stmt* "}"
 *          params = param ("," param)*
 *           param = basetype ident
 * @param[in] token Tokenized source code.
 * @return Node of 'function'.
 */
static struct Function *function(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  locals = nullptr;

  struct Function *func = calloc(1, sizeof(*func));
  CHECK(func != nullptr);
  // Seek the type of function. Now, ignore it.
  basetype(token);
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

// @brief declaration = basetype ident ("[" num "]")* ("=" expr) ";"
static struct Node *declaration(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  struct Token *tok = *token;
  struct Type *ty = basetype(token);
  char *name = seek_if_expect_ident(token);
  ty = read_type_suffix(token, ty);
  struct Var *var = push_var(name, ty, true);

  if (consume(token, ";"))
    return new_node(NODE_NULL, tok);

  seek_if_expect(token, "=");
  struct Node *lhs = new_var(var, tok);
  struct Node *rhs = expr(token);
  seek_if_expect(token, ";");
  struct Node *node = new_binary(NODE_ASSIGN, lhs, rhs, tok);
  return new_unary(NODE_EXPR_STMT, node, tok);
}

static struct Node *read_expr_stmt(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  return new_unary(NODE_EXPR_STMT, expr(token), *token);
}

static bool is_typename(struct Token **token) {
  return peek(token, "char") || peek(token, "int") || peek(token, "struct");
}

/**
 * @brief stmt = expr ";"
 *             | "{" stmt "}"
 *             | "if" "(" expr ")" stmt ("else" stmt)?
 *             | "while" "(" expr ")" stmt
 *             | "for" "(" expr? ";" expr? ";" expr ";" )" stmt
 *             | declaration
 *             | "return" expr ";"
 * @param token Tokenized source codes.
 * @return Node of `stmt`.
 */
static struct Node *stmt(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);

  if (consume(token, "if")) {
    struct Node *node = new_node(NODE_IF, *token);
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
    struct Node *node = new_node(NODE_WHILE, *token);
    CHECK(node != nullptr);

    seek_if_expect(token, "(");
    node->cond = expr(token);
    seek_if_expect(token, ")");
    node->then = stmt(token);

    return node;
  }

  if (consume(token, "for")) {
    struct Node *node = new_node(NODE_FOR, *token);
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
    struct Node *node = new_unary(NODE_RETURN, expr(token), *token);
    seek_if_expect(token, ";");
    return node;
  }

  struct VarList *sc = scope;
  if (consume(token, "{")) {
    struct Node head = {};
    head.next = nullptr;
    struct Node *cur = &head;

    while (!consume(token, "}")) {
      cur->next = stmt(token);
      cur = cur->next;
    }
    scope = sc;

    struct Node *node = new_node(NODE_BLOCK, *token);
    node->body = head.next;
    return node;
  }

  if (is_typename(token))
    return declaration(token);

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
    node = new_binary(NODE_ASSIGN, node, assign(token), *token);
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
      node = new_binary(NODE_EQ, node, relational(token), *token);
    else if (consume(token, "!="))
      node = new_binary(NODE_NE, node, relational(token), *token);
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
      node = new_binary(NODE_LT, node, add(token), *token);
    else if (consume(token, "<="))
      node = new_binary(NODE_LE, node, add(token), *token);
    else if (consume(token, ">"))
      node = new_binary(NODE_LT, add(token), node, *token);
    else if (consume(token, ">="))
      node = new_binary(NODE_LE, add(token), node, *token);
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
      node = new_binary(NODE_ADD, node, mul(token), *token);
    else if (consume(token, "-"))
      node = new_binary(NODE_SUB, node, mul(token), *token);
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
      node = new_binary(NODE_MUL, node, unary(token), *token);
    else if (consume(token, "/"))
      node = new_binary(NODE_DIV, node, unary(token), *token);
    else
      return node;
  }
}

/**
 * @brief unary = ("+" | "-" | "&" | "*")? unary
 *              | postfix
 * @param token Tokenized source code.
 * @return Finally returns node for `primary`.
 */
static struct Node *unary(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  if (consume(token, "+"))
    return unary(token);
  if (consume(token, "-"))
    return new_binary(NODE_SUB, new_num(0, *token), unary(token), *token);
  if (consume(token, "&"))
    return new_unary(NODE_ADDR, unary(token), *token);
  if (consume(token, "*"))
    return new_unary(NODE_DEREF, unary(token), *token);
  return postfix(token);
}

// @brief postfix = primary ("[ expr ]" | "." ident)*
static struct Node *postfix(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  struct Node *node = primary(token);

  for (;;) {
    if (consume(token, "[")) {
      // x[y] is short for *(x+y)
      struct Node *exp = new_binary(NODE_ADD, node, expr(token), *token);
      seek_if_expect(token, "]");
      node = new_unary(NODE_DEREF, exp, *token);
      continue;
    }

    if (consume(token, ".")) {
      node = new_unary(NODE_MEMBER, node, *token);
      node->member_name = seek_if_expect_ident(token);
      continue;
    }
    return node;
  }
}

/**
 * @brief stmt-expr-tail = stmt stmt* "}" ")"
 *        This is the statement expression in a GNU C extension.
 * @param token Tokenized source code.
 * @return `stmt-expr-tail`
 */
static struct Node *stmt_expr(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  struct VarList *sc = scope;

  struct Node *node = new_node(NODE_STMT_EXPR, *token);
  node->body = stmt(token);
  struct Node *cur = node->body;

  while (!consume(token, "}")) {
    cur->next = stmt(token);
    cur = cur->next;
  }
  seek_if_expect(token, ")");

  scope = sc;

  if (cur->kind != NODE_EXPR_STMT)
    error_tok(cur->tok,
              "Returning void from a statement expression is not supported.");
  *cur = *(cur->lhs);
  return node;
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
 *                | "sizeof" unary
 *                | ident func-args?
 *                | str
 *                | "(" expr ")"
 *                | "(" "{" stmt-expr-tail
 *        args = "(" ident (",", ident)* ")"
 * @param **token Tokenized source code.
 * @return Either value node or node for `expr`.
 */
static struct Node *primary(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);

  if (consume(token, "(")) {
    if (consume(token, "{"))
      return stmt_expr(token);
    struct Node *node = expr(token);
    seek_if_expect(token, ")");
    return node;
  }

  if (consume(token, "sizeof"))
    return new_unary(NODE_SIZEOF, unary(token), *token);

  struct Token *tok = consume_ident(token);
  if (tok != nullptr) {
    if (consume(token, "(")) {
      struct Node *node = new_node(NODE_FUNC_CALL, *token);
      node->funcname = strndup(tok->str, tok->len);
      node->args = func_args(token);
      return node;
    }

    struct Var *var = find_var(tok);
    if (var == nullptr)
      error_tok(tok, "Undefined variable");
    return new_var(var, *token);
  }

  tok = *token;
  if (tok->kind == TK_STR) {
    (*token) = (*token)->next;

    struct Type *ty = array_of(char_type(), tok->content_len);
    struct Var *var = push_var(new_label(), ty, false);
    var->contents = tok->contents;
    var->content_len = tok->content_len;
    return new_var(var, tok);
  }

  if (tok->kind != TK_NUM)
    error_tok(tok, "Expected expression");
  return new_num(seek_if_expect_number(token), tok);
}
