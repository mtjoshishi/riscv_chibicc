#include "parse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chibicc_error.h"
#include "chibicc_types.h"
#include "chibicc_utils.h"
#include "tokenize.h"
#include "type.h"

// Scopr for local variables, global variables or typedefs.
struct VarScope {
  struct VarScope *next;
  char *name;
  struct Var *var;
  struct Type *type_def;
};

// Scope for struct tags
struct TagScope {
  struct TagScope *next;
  char *name;
  struct Type *ty;
};

struct VarList *locals;
struct VarList *globals;

struct VarScope *var_scope;
struct TagScope *tag_scope;

/// @brief Find a variable or a typedef by name.
static struct VarScope *find_var(const struct Token *token) {
  CHECK(token != nullptr);
  for (struct VarScope *vsc = var_scope; vsc != nullptr; vsc = vsc->next) {
    if (strlen(vsc->name) == token->len &&
        !memcmp(token->str, vsc->name, token->len))
      return vsc;
  }
  return nullptr;
}

/**
 * @brief Find the struct tag.
 * @param token The tokenized token.
 * @return Found tag scope. If not found, returns nullptr.
 */
static struct TagScope *find_tag(struct Token *token) {
  CHECK(token != nullptr);
  for (struct TagScope *sc = tag_scope; sc != nullptr; sc = sc->next) {
    if (strlen(sc->name) == token->len &&
        !memcmp(token->str, sc->name, token->len))
      return sc;
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
struct Node *new_num(long value, struct Token *tok) {
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
 * @brief Push the new scoped variable or typedef onto list.
 * @param name Name of scoped variable or typedef.
 * @return struct VarScope
 */
static struct VarScope *push_scope(char *name) {
  CHECK(name != nullptr);
  struct VarScope *vsc = calloc(1, sizeof(*vsc));
  CHECK(vsc != nullptr);
  vsc->name = name;
  vsc->next = var_scope;
  var_scope = vsc;
  return vsc;
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
  var->ty = ty;
  var->is_local = is_local;

  struct VarList *vl = calloc(1, sizeof(*vl));
  CHECK(vl != nullptr);
  vl->var = var;

  if (is_local) {
    vl->next = locals;
    locals = vl;
  } else if (ty->kind != TYPE_FUNC) {
    vl->next = globals;
    globals = vl;
  }

  push_scope(name)->var = var;
  return var;
}

/**
 * @brief Search the defined 'typedef' from the scope.
 * @param token The tokenized source code. This will not be sought.
 * @return struct Type
 */
static struct Type *find_typedef(const struct Token *token) {
  CHECK(token != nullptr);
  if (token->kind == TK_IDENT) {
    struct VarScope *vsc = find_var(token);
    if (vsc != nullptr)
      return vsc->type_def;
  }
  return nullptr;
}

static char *new_label() {
  static int cnt = 0;
  enum { kBufLen = 256 };
  char buf[kBufLen] = {};

  sprintf(buf, ".LC%d", cnt++);
  return strndup(buf, 20);
}

static struct Function *function(struct Token **token);
static struct Type *type_specifier(struct Token **token);
static struct Type *declarator(struct Token **token, struct Type *ty,
                               char **name);
static struct Type *abstract_declarator(struct Token **token, struct Type *ty);
static struct Type *type_suffix(struct Token **token, struct Type *ty);
static struct Type *type_name(struct Token **token);
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
static struct Node *cast(struct Token **token);
static struct Node *unary(struct Token **token);
static struct Node *postfix(struct Token **token);
static struct Node *primary(struct Token **token);

static bool is_function(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  struct Token *snapshot = *token;

  struct Type *ty = type_specifier(token);
  char *name = nullptr;
  declarator(token, ty, &name);
  bool is_func = name && consume(token, "(");

  *token = snapshot;
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

/**
 * @brief type-specifier = builtin-type | struct-decl | typedef-name
 *        builtin-type = "void"
 *                     | "_Bool"
 *                     | "char"
 *                     | "short" | "short" "int" | "int" "short"
 *                     | "int"
 *                     | "long" | "long" "int" | "int" "long"
 */
static struct Type *type_specifier(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  if (!is_typename(token))
    error_tok(*token, "Typename expected.");

  struct Type *ty = nullptr;

  enum {
    kVoid = 1 << 1,
    kBool = 1 << 3,
    kChar = 1 << 5,
    kShort = 1 << 7,
    kInt = 1 << 9,
    kLong = 1 << 11,
  };

  int base_type = 0;
  struct Type *user_type = nullptr;
  bool is_typedef = false;

  for (;;) {
    // Read one token at a time.
    struct Token *tok = *token;
    if (consume(token, "typedef")) {
      is_typedef = true;
    } else if (consume(token, "void")) {
      base_type += kVoid;
    } else if (consume(token, "_Bool")) {
      base_type += kBool;
    } else if (consume(token, "char")) {
      base_type += kChar;
    } else if (consume(token, "short")) {
      base_type += kShort;
    } else if (consume(token, "int")) {
      base_type += kInt;
    } else if (consume(token, "long")) {
      base_type += kLong;
    } else if (peek(token, "struct")) {
      if (base_type || user_type)
        break;
      user_type = struct_decl(token);
    } else {
      if (base_type || user_type)
        break;
      struct Type *ty = find_typedef(*token);
      if (ty == nullptr)
        break;
      *token = (*token)->next;
      user_type = ty;
    }

    switch (base_type) {
    case kVoid:
      ty = void_type();
      break;
    case kBool:
      ty = bool_type();
      break;
    case kChar:
      ty = char_type();
      break;
    case kShort:
      [[fallthrough]];
    case kShort + kInt:
      ty = short_type();
      break;
    case kInt:
      ty = int_type();
      break;
    case kLong:
      [[fallthrough]];
    case kLong + kInt:
      ty = long_type();
      break;
    case 0:
      /*
       * The original chibicc allows implicit int conversion if the source of
       * 'typedef' is not defined. However, ISO C99 and later do not support it.
       * Therefore, if 'user_type' is not set, raise the error.
       */
      ty = user_type ? user_type : nullptr;
      break;
    default:
      error_tok(tok, "Invalid type is specified.");
    }
  }

  if (ty == nullptr)
    error_tok(*token, "Type specifier missing, default to 'int'; ISO C99 and "
                      "later do not support implicit int.");

  ty->is_typedef = is_typedef;
  return ty;
}

/**
 * @brief declarator = "*" ("(" declarator ")" | ident) type-suffix
 * @param token The tokenized token.
 * @param ty The representative 'type' object.
 * @param name The pointer to name of declarator.
 */
static struct Type *declarator(struct Token **token, struct Type *ty,
                               char **name) {
  CHECK(token != nullptr && *token != nullptr);
  CHECK(ty != nullptr);
  CHECK(name != nullptr);

  while (consume(token, "*"))
    ty = pointer_to(ty);

  if (consume(token, "(")) {
    struct Type *placeholder = calloc(1, sizeof(*placeholder));
    struct Type *new_ty = declarator(token, placeholder, name);
    seek_if_expect(token, ")");
    *placeholder = *type_suffix(token, ty);
    return new_ty;
  }

  *name = seek_if_expect_ident(token);
  return type_suffix(token, ty);
}

/**
 * @brief abstract-declarator = "*"* ("(" abstract-declarator ")")? type-suffix
 * @param token The tokenized source code.
 * @param ty The based type.
 * @return The new type.
 */
static struct Type *abstract_declarator(struct Token **token, struct Type *ty) {
  CHECK(token != nullptr && *token != nullptr);
  CHECK(ty != nullptr);

  while (consume(token, "*"))
    ty = pointer_to(ty);

  if (consume(token, "(")) {
    struct Type *placeholder = calloc(1, sizeof(*placeholder));
    struct Type *new_ty = abstract_declarator(token, placeholder);
    seek_if_expect(token, ")");
    *placeholder = *type_suffix(token, ty);
    return new_ty;
  }
  return type_suffix(token, ty);
}

/**
 * @brief type-suffix = ("[" num "]" type-suffix)?
 * @param token The tokenized token.
 * @param ty The representative 'type' object.
 * @return 'struct Type': type-suffix.
 */
static struct Type *type_suffix(struct Token **token, struct Type *ty) {
  CHECK(token != nullptr && *token != nullptr);
  CHECK(ty != nullptr);
  if (!consume(token, "["))
    return ty;
  long sz = seek_if_expect_number(token);
  seek_if_expect(token, "]");
  ty = type_suffix(token, ty);
  return array_of(ty, sz);
}

/**
 * @brief type-name = type-specifier abstract-declarator type-suffix
 * @param token The tokenized source code.
 */
static struct Type *type_name(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  struct Type *ty = type_specifier(token);
  ty = abstract_declarator(token, ty);
  return type_suffix(token, ty);
}

/**
 * @brief Push the struct's tag name into scope of tag.
 * @param token The tokenized of source code. NOTE: this will not be sought.
 * @param ty The type of tag.
 */
static void push_tag_scope(const struct Token *token, struct Type *ty) {
  CHECK(token != nullptr);
  struct TagScope *sc = calloc(1, sizeof(*sc));
  CHECK(sc != nullptr);
  sc->next = tag_scope;
  sc->name = strndup(token->str, token->len);
  sc->ty = ty;
  tag_scope = sc;
}

/**
 * @brief struct-decl = "struct" ident
 *                    | "struct" ident? "{ member-declaration-list }"
 * @param token The tokenized source code. This will be sought.
 * @return The type of struct.
 */
static struct Type *struct_decl(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  // Read a struct tag.
  seek_if_expect(token, "struct");
  struct Token *tag = consume_ident(token);
  if (tag != nullptr && peek(token, "{") == nullptr) {
    struct TagScope *sc = find_tag(tag);
    if (sc == nullptr)
      error_tok(tag, "Unknown struct type.");
    return sc->ty;
  }
  seek_if_expect(token, "{");

  // Read struct members.
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
  long offset = 0;
  for (struct Member *mem = ty->members; mem != nullptr; mem = mem->next) {
    offset = align_to(offset, mem->ty->align);
    mem->offset = offset;
    offset += __size_of(mem->ty);

    if (ty->align < mem->ty->align)
      ty->align = mem->ty->align;
  }

  // Register the struct type if a name was given.
  if (tag != nullptr)
    push_tag_scope(tag, ty);
  return ty;
}

/**
 * @brief member-declaration-list = type-specifier declarator type_suffix ";"
 * @param token The tokenized source codes.
 */
static struct Member *member_declaration_list(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);

  struct Type *ty = type_specifier(token);
  char *name = nullptr;
  ty = declarator(token, ty, &name);
  ty = type_suffix(token, ty);
  seek_if_expect(token, ";");

  struct Member *mem = calloc(1, sizeof(*mem));
  mem->name = name;
  mem->ty = ty;
  return mem;
}

static struct VarList *read_func_param(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  struct Type *ty = type_specifier(token);
  char *name = nullptr;
  ty = declarator(token, ty, &name);
  ty = type_suffix(token, ty);

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

/// @brief global-var = type-specifier declarator type-suffix ";"
static void global_var(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);

  struct Type *ty = type_specifier(token);
  char *name = nullptr;
  ty = declarator(token, ty, &name);
  ty = type_suffix(token, ty);
  seek_if_expect(token, ";");
  push_var(name, ty, false);
}

/**
 * @brief function = type-specifier declarator "(" params? ")" "{" stmt* "}"
 *        params   = param ("," param)*
 *        param    = type-specifier declarator type_suffix
 * @param[in] token Tokenized source code.
 * @return Node of 'function'.
 */
static struct Function *function(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  locals = nullptr;

  struct Type *ty = type_specifier(token);
  char *name = nullptr;
  ty = declarator(token, ty, &name);

  // Add a function type to the scope
  push_var(name, func_type(ty), false);

  // Construct a function object.
  struct Function *func = calloc(1, sizeof(*func));
  CHECK(func != nullptr);
  // Seek the type of function. Now, ignore it.
  func->name = name;
  seek_if_expect(token, "(");
  func->params = read_func_params(token);
  seek_if_expect(token, "{");

  // Read function body.
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

/*
 * @brief declaration = type-specifier declarator type-suffix ("=" expr)? ";"
 *                    | type-specifier ";"
 */
static struct Node *declaration(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);

  struct Token *tok = *token;
  struct Type *ty = type_specifier(token);
  if (consume(token, ";"))
    return new_node(NODE_NULL, tok);

  char *name = nullptr;
  ty = declarator(token, ty, &name);
  ty = type_suffix(token, ty);

  if (ty->is_typedef) {
    seek_if_expect(token, ";");
    ty->is_typedef = false;
    push_scope(name)->type_def = ty;
    return new_node(NODE_NULL, tok);
  }

  if (ty->kind == TYPE_VOID)
    error_tok(tok, "Variable declared as void.");

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
  return peek(token, "void") || peek(token, "_Bool") || peek(token, "char") ||
         peek(token, "short") || peek(token, "int") || peek(token, "long") ||
         peek(token, "struct") || peek(token, "typedef") ||
         find_typedef(*token);
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

  struct VarScope *vsc = var_scope;
  struct TagScope *tag_sc = tag_scope;
  if (consume(token, "{")) {
    struct Node head = {};
    head.next = nullptr;
    struct Node *cur = &head;

    while (!consume(token, "}")) {
      cur->next = stmt(token);
      cur = cur->next;
    }
    var_scope = vsc;
    tag_scope = tag_sc;

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
 * @brief mul = cast ("*" cast | "/" cast)
 * @param **token Tokenized source code.
 * @return Node for `mul`.
 */
static struct Node *mul(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  struct Node *node = cast(token);

  for (;;) {
    if (consume(token, "*"))
      node = new_binary(NODE_MUL, node, cast(token), *token);
    else if (consume(token, "/"))
      node = new_binary(NODE_DIV, node, cast(token), *token);
    else
      return node;
  }
}

/**
 * @brief cast = "(" type-name ")" cast | unary
 * @param token The tokenized source code.
 */
static struct Node *cast(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  struct Token *tok = *token;

  if (consume(token, "(")) {
    if (is_typename(token)) {
      struct Type *ty = type_name(token);
      seek_if_expect(token, ")");
      struct Node *node = new_unary(NODE_CAST, cast(token), tok);
      node->ty = ty;
      return node;
    }
    *token = tok;
  }

  return unary(token);
}

/**
 * @brief unary = ("+" | "-" | "&" | "*")? cast
 *              | postfix
 * @param token Tokenized source code.
 * @return Finally returns node for `primary`.
 */
static struct Node *unary(struct Token **token) {
  CHECK(token != nullptr && *token != nullptr);
  if (consume(token, "+"))
    return cast(token);
  if (consume(token, "-"))
    return new_binary(NODE_SUB, new_num(0, *token), cast(token), *token);
  if (consume(token, "&"))
    return new_unary(NODE_ADDR, cast(token), *token);
  if (consume(token, "*"))
    return new_unary(NODE_DEREF, cast(token), *token);
  return postfix(token);
}

// @brief postfix = primary ("[ expr ]" | "." ident | "->" ident)*
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

    if (consume(token, "->")) {
      // x->y is short for (*x).y.
      node = new_unary(NODE_DEREF, node, *token);
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
  struct VarScope *var_sc = var_scope;
  struct TagScope *tag_sc = tag_scope;

  struct Node *node = new_node(NODE_STMT_EXPR, *token);
  node->body = stmt(token);
  struct Node *cur = node->body;

  while (!consume(token, "}")) {
    cur->next = stmt(token);
    cur = cur->next;
  }
  seek_if_expect(token, ")");

  var_scope = var_sc;
  tag_scope = tag_sc;

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
 *                | "sizeof" "(" type-name ")"
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

  struct Token *tok = *token;
  if (consume(token, "sizeof")) {
    if (consume(token, "(")) {
      if (is_typename(token)) {
        struct Type *ty = type_name(token);
        seek_if_expect(token, ")");
        return new_num(__size_of(ty), tok);
      }
      *token = tok->next;
    }
    return new_unary(NODE_SIZEOF, unary(token), *token);
  }

  tok = consume_ident(token);
  if (tok != nullptr) {
    if (consume(token, "(")) {
      struct Node *node = new_node(NODE_FUNC_CALL, *token);
      node->funcname = strndup(tok->str, tok->len);
      node->args = func_args(token);

      struct VarScope *vsc = find_var(tok);
      if (vsc != nullptr) {
        if (vsc->var == nullptr || vsc->var->ty->kind != TYPE_FUNC)
          error_tok(tok, "Not a function.");
        node->ty = vsc->var->ty->return_ty;
      } else {
        node->ty = int_type();
      }
      return node;
    }

    struct VarScope *vsc = find_var(tok);
    if (vsc != nullptr && vsc->var != nullptr)
      return new_var(vsc->var, tok);
    error_tok(tok, "Undefined variable");
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
