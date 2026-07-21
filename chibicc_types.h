#ifndef CHIBICC_TYPES_H_
#define CHIBICC_TYPES_H_

#include <stddef.h>

// Kind of token
enum TokenKind {
  TK_RESERVED, // Reserved word
  TK_IDENT,    // Identifier
  TK_NUM,      // Numeric token
  TK_STR,      // String literal
  TK_EOF,      // End of file
};

// Token
struct Token {
  enum TokenKind kind; // Kind of token
  struct Token *next;  // Next input token
  long val;            // Numerical value if kind is 'TK_NUM'
  char *str;           // String of token
  size_t len;          // Length of token.

  char *contents;  // String literal contents including NULL termination
  int content_len; // Length of string literal

  char *source_input; // Input source
};

extern char *filename;

// Kind of node for AST
struct Member;

enum NodeKind {
  NODE_ADD,       // '+'
  NODE_SUB,       // '-'
  NODE_MUL,       // '*'
  NODE_DIV,       // '/'
  NODE_EQ,        // ==
  NODE_NE,        // !=
  NODE_LT,        // <
  NODE_LE,        // <=
  NODE_ASSIGN,    // =
  NODE_MEMBER,    // . (struct member access)
  NODE_ADDR,      // unary '&'
  NODE_DEREF,     // unary '*'
  NODE_IF,        // "if"
  NODE_WHILE,     // "while"
  NODE_FOR,       // "for"
  NODE_SIZEOF,    // "sizeof"
  NODE_BLOCK,     // { ... }
  NODE_FUNC_CALL, // Function call (zero-arity)
  NODE_RETURN,    // "return"
  NODE_EXPR_STMT, // Expression statement to handle void-expression.
  NODE_STMT_EXPR, // GNU statement expression
  NODE_VAR,       // Variable
  NODE_NUM,       // number
  NODE_NULL,      // Empty statement
};

struct Type;

// Variable
struct Var {
  char *name;      // Name of variable
  struct Type *ty; // Type of variable
  bool is_local;   // Whether the scope is global or not.

  // For local variables
  long offset; // Offset of the stack from the frame pointer.

  // For global string literal
  char *contents;  // String literal contents including NULL termination
  int content_len; // Length of string literal
};

struct VarList {
  struct VarList *next;
  struct Var *var;
};

// Node for AST
struct Node {
  enum NodeKind kind; // Type of node.
  struct Node *next;  // Next node.
  struct Type *ty;    // Type, e.g. int or pointer int
  struct Token *tok;  // Representative token

  struct Node *lhs; // Left hand side statement
  struct Node *rhs; // Right hand side statement

  // "if", "while", or "for" statement
  struct Node *cond;      // Condition expression
  struct Node *then;      // Statement then "if", "while", and "for"
  struct Node *els;       // Else statement
  struct Node *init;      // Initial condition for "for".
  struct Node *increment; // Increment expression for "for".

  // Block or GNU statement expression
  struct Node *body;

  // Struct member access
  char *member_name;
  struct Member *member;

  // Name of calling function
  char *funcname;
  struct Node *args;

  struct Var *var; // Object of variable. Use if kind is NODE_VAR.
  long val;        // Value if kind is NODE_NUM
};

enum TypeKind {
  TYPE_VOID,
  TYPE_BOOL,
  TYPE_CHAR,
  TYPE_SHORT,
  TYPE_INT,
  TYPE_LONG,
  TYPE_PTR,
  TYPE_ARRAY,
  TYPE_STRUCT,
  TYPE_FUNC,
};

struct Type {
  enum TypeKind kind;
  bool is_typedef;        // typedef
  int align;              // Alignment
  struct Type *base;      // Use pointer or array
  long array_size;        // size of array
  struct Member *members; // struct
  struct Type *return_ty; // return type of function.
};

// Struct member
struct Member {
  struct Member *next;
  struct Type *ty;
  char *name;
  long offset;
};

// Program
struct Function {
  struct Function *next;
  char *name;
  struct VarList *params;

  struct Node *node;
  struct VarList *locals;
  long stack_size;
};

struct Program {
  struct VarList *globals;
  struct Function *functions;
};

#endif // CHIBICC_TYPES_H_
