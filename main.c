#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "chibicc_error.h"
#include "chibicc_types.h"
#include "chibicc_utils.h"
#include "codegen.h"
#include "parse.h"
#include "tokenize.h"
#include "type.h"

char *filename;

static char *read_file(char *path) {
  errno = 0;

  int fd = open(path, O_RDONLY);
  if (fd == -1)
    error("Cannot open %s: %s", path, strerror(errno));

  // Fetch the size of file
  struct stat stbuf;
  if (fstat(fd, &stbuf) == -1) {
    close(fd);
    error("Access denied %s: %s", path, strerror(errno));
  }
  off_t fsize = stbuf.st_size;
  CHECK(fsize >= 0);

  FILE *fp = fdopen(fd, "r");
  if (fp == NULL) {
    close(fd);
    error("Failed to open %s: %s", path, strerror(errno));
  }

  // Read the contents of file
  char *buf = calloc((size_t)fsize + 2, sizeof(*buf));
  CHECK(buf != nullptr);

  size_t read_bytes = fread(buf, 1, (size_t)fsize, fp);
  if (read_bytes < (size_t)fsize && ferror(fp)) {
    free(buf);
    fclose(fp);
    error("Read error %s: %s", path, strerror(errno));
  }

  // File must be terminated by "\n\0".
  if (fsize == 0 || buf[fsize - 1] != '\n')
    buf[fsize++] = '\n';
  buf[fsize] = '\0';
  fclose(fp);

  return buf;
}

int main(int argc, char **argv) {
  if (argc != 2)
    error("Invalid number of arguments.");

  filename = argv[1];
  char *user_input = read_file(filename);
  struct Token *token = tokenize(user_input);
  struct Program *prog = program(&token);
  CHECK(prog != nullptr);
  add_type(prog);

  // Assign offsets to local variables.
  for (struct Function *func = prog->functions; func != nullptr;
       func = func->next) {
    long offset = 0;
    for (struct VarList *vl = func->locals; vl != nullptr; vl = vl->next) {
      struct Var *var = vl->var;
      offset = align_to(offset, var->ty->align);
      offset += __size_of(var->ty);
      var->offset = offset;
    }
    func->stack_size = align_to(offset, 16);
  }

  codegen(prog);

  return 0;
}
