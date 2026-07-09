#!/bin/sh

RISCV64_CC=riscv64-linux-gnu-gcc

cat <<EOF | $RISCV64_CC -xc -c -o tmp2.o -
int ret3() { return 3; }
int ret5() { return 5; }
int add(int x, int y) { return x + y; }
int mul(int x, int y) { return x * y; }

int add6(int a, int b, int c, int d, int e, int f) {
  return a + b + c + d + e + f;
}
EOF

assert() {
  expected="$1"
  input="$2"

  ./riscv_chibicc "$input" > tmp.s || exit 1
  $RISCV64_CC -static tmp.s tmp2.o -o tmp.elf || exit 1

  # Run on QEMU.
  qemu-riscv64 ./tmp.elf
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected, but got $actual"
    exit 1
  fi
}

assert 0 "main() { return 0; }"
assert 42 "main() { return 42; }"
assert 21 "main() { return 5+20-4; }"
assert 41 "main() { return 12 + 34 - 5; }"
assert 47 "main() { return 5+6*7; }"
assert 15 "main() { return 5*(9-6); }"
assert 4 "main() { return (3+5)/2; }"
assert 10 "main() { return -10+20; }"
assert 10 "main() { return - -10; }"
assert 10 "main() { return - - +10; }"

assert 0 "main() { return 0 == 1; }"
assert 1 "main() { return 42 == 42; }"
assert 1 "main() { return 0 != 1; }"
assert 0 "main() { return 42 != 42; }"

assert 1 "main() { return 0 < 1; }"
assert 0 "main() { return 1 < 1; }"
assert 0 "main() { return 2 < 1; }"
assert 1 "main() { return 0 <= 1; }"
assert 1 "main() { return 1 <= 1; }"
assert 0 "main() { return 2 <= 1; }"

assert 1 "main() { return 1 > 0; }"
assert 0 "main() { return 1 > 1; }"
assert 0 "main() { return 1 > 2; }"
assert 1 "main() { return 1 >= 0; }"
assert 1 "main() { return 1 >= 1; }"
assert 0 "main() { return 1 >= 2; }"

assert 1 "main() { return 1; 2; 3; }"
assert 2 "main() { 1; return 2; 3; }"
assert 3 "main() { 1; 2; return 3; }"

assert 3 "main() { a=3; return a; }"
assert 3 "main() { foo=3; return foo; }"
assert 8 "main() { foo123=3; bar=5; return foo123+bar; }"

assert 3 "main() { if (0) return 2; return 3; }"
assert 3 "main() { if (1-1) return 2; return 3; }"
assert 2 "main() { if (0 <= 1) return 2; return 3; }"
assert 2 "main() { if (1 > 0) return 2; else return 3; }"

assert 3 "main() { 1; {2;} return 3; }"

assert 10 "main() { i=0; while(i<10) i=i+1; return i; }"
assert 55 "main() { i=0; j=0; while(i<=10) {j=i+j; i=i+1;} return j; }"

assert 10 "main() { a=0; for (a = 0; a < 10; a = a + 1) a = a + 1; return a; }"
assert 3 "main() { for (;;) return 3; return 5; }"
assert 55 "main() { i=0; j=0; for (i=0; i<=10; i=i+1) j=i+j; return j; }"

assert 3 "main() { return ret3(); }"
assert 5 "main() { return ret5(); }"
assert 8 "main() { return add(3, 5); }"
assert 15 "main() { return mul(3, 5); }"
assert 21 "main() { return add6(1, 2, 3, 4, 5, 6); }"

assert 32 "ret32() { return 32; } main() { return ret32(); }"
assert 7 "add2(x, y) { return x + y; } main() { return add2(3, 4); }"
assert 12 "mul2(x, y) { return x * y; } main() { return mul2(3, 4); }"
assert 55 "fib(n) { if (n < 2) return n; else return fib(n-1) + fib(n-2); } main() { return fib(10); }"

assert 3 "main() { x = 3; return *(&x); }"
assert 3 "main() { x = 3; y = &x; z = &y; return **z; }"
assert 5 "main() { x=3; y=&x; *y=5; return x; }"
echo OK
