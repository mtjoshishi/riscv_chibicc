#ifndef CHIBICC_TYPES_H_
#define CHIBICC_TYPES_H_

// Kind of token
enum TokenKind {
  TK_RESERVED, // Reserved word
  TK_NUM,      // Numeric token
  TK_EOF,      // End of file
};

// Token
struct Token {
  enum TokenKind kind; // Kind of token
  struct Token *next;  // Next input token
  int val;             // Numerical value if kind is 'TK_NUM'
  char *str;           // String of token

  char *source_input; // Input source
};

#endif // CHIBICC_TYPES_H_
