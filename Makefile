CFLAGS=-std=gnu23 -g -static -Wall -Wextra -Wformat=2 -Wconversion \
       -Wtrampolines -Wimplicit-fallthrough
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

riscv_chibicc: $(OBJS)
	$(CC) -o riscv_chibicc $(OBJS) $(LDFLAGS)

$(OBJS): chibicc_error.h chibicc_types.h codegen.h parse.h tokenize.h type.h

test: riscv_chibicc
	./test.sh

clean:
	rm -f riscv_chibicc *.o *~ tmp*

.PHONY: test clean
