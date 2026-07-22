CFLAGS=-std=gnu23 -g -static -Wall -Wextra -Wformat=2 -Wconversion \
       -Wtrampolines -Wimplicit-fallthrough
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)
RISCV64_CC=riscv64-linux-gnu-gcc

riscv_chibicc: $(OBJS)
	$(CC) -o riscv_chibicc $(OBJS) $(LDFLAGS)

$(OBJS): chibicc_error.h chibicc_types.h chibicc_utils.h codegen.h parse.h \
	tokenize.h type.h

test: riscv_chibicc
	./riscv_chibicc tests.tc > tmp.s
	echo 'int char_fn() { return 257; }' | $(RISCV64_CC) -xc -c -o tmp2.o -
	$(RISCV64_CC) -static -o tmp tmp.s tmp2.o
	qemu-riscv64 ./tmp

clean:
	rm -f riscv_chibicc *.o *~ tmp*

.PHONY: test clean
