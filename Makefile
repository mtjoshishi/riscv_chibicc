CFLAGS=-std=gnu23 -g -static -Wall -Wextra -Wformat=2 -Wconversion \
       -Wtrampolines -Wimplicit-fallthrough

riscv_chibicc: riscv_chibicc.c

test: riscv_chibicc
	./test.sh

clean:
	rm -f riscv_chibicc *.o *~ tmp*

.PHONY: test clean
