#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Invalid number of arguments.\n");
    return 1;
  }

  char *p = argv[1];

  printf(".globl main\n");
  printf("main:\n");
  printf("    li a0,%ld\n", strtol(p, &p, 10));

  while (*p) {
    if (*p == '+') {
      p++;
      printf("    addi a0,a0,%ld\n", strtol(p, &p, 10));
      continue;
    }

    if (*p == '-') {
      p++;
      printf("    addi a0,a0,-%ld\n", strtol(p, &p, 10));
      continue;
    }

    fprintf(stderr, "Unexpected character: '%c'\n", *p);
    return 1;
  }

  printf("    ret\n");
  return 0;
}
