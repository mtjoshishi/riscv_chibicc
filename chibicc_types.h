#ifndef CHIBICC_TYPES_H_
#define CHIBICC_TYPES_H_

#include <stddef.h>

// Kind of token
enum TokenKind {
  TK_RESERVED, // Reserved word
  TK_IDENT,    // Identifier
  TK_NUM,      // Numeric token
  TK_EOF,      // End of file
};

// Token
struct Token {
  enum TokenKind kind; // Kind of token
  struct Token *next;  // Next input token
  int val;             // Numerical value if kind is 'TK_NUM'
  char *str;           // String of token
  size_t len;          // Length of token.

  char *source_input; // Input source
};

// Kind of node for AST
enum NodeKind {
  NODE_ADD,    // '+'
  NODE_SUB,    // '-'
  NODE_MUL,    // '*'
  NODE_DIV,    // '/'
  NODE_EQ,     // ==
  NODE_NE,     // !=
  NODE_LT,     // <
  NODE_LE,     // <=
  NODE_ASSIGN, // =
  NODE_VAR,    // Variable
  NODE_NUM,    // number
};

// Variable
struct Var {
  struct Var *next; // Next variable
  char *name;       // Name of variable
  size_t len;       // Length of variable name
  int offset;       // Offset of the stack from the frame pointer.
};

// Node for AST
struct Node {
  enum NodeKind kind; // Type of node.
  struct Node *next;  // Next node.
  struct Node *lhs;   // Left hand side statement
  struct Node *rhs;   // Right hand side statement
  struct Var *var;    // Object of variable. Use if kind is NODE_VAR.
  int val;            // Value if kind is NODE_NUM
};

// Program
struct Program {
  struct Node *node;
  struct Var *locals;
  int stack_size;
};

#endif // CHIBICC_TYPES_H_
