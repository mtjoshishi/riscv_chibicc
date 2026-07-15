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

assert 0 "int main() { return 0; }"
assert 42 "int main() { return 42; }"
assert 21 "int main() { return 5+20-4; }"
assert 41 "int main() { return 12 + 34 - 5; }"
assert 47 "int main() { return 5+6*7; }"
assert 15 "int main() { return 5*(9-6); }"
assert 4 "int main() { return (3+5)/2; }"
assert 10 "int main() { return -10+20; }"
assert 10 "int main() { return - -10; }"
assert 10 "int main() { return - - +10; }"

assert 0 "int main() { return 0 == 1; }"
assert 1 "int main() { return 42 == 42; }"
assert 1 "int main() { return 0 != 1; }"
assert 0 "int main() { return 42 != 42; }"

assert 1 "int main() { return 0 < 1; }"
assert 0 "int main() { return 1 < 1; }"
assert 0 "int main() { return 2 < 1; }"
assert 1 "int main() { return 0 <= 1; }"
assert 1 "int main() { return 1 <= 1; }"
assert 0 "int main() { return 2 <= 1; }"

assert 1 "int main() { return 1 > 0; }"
assert 0 "int main() { return 1 > 1; }"
assert 0 "int main() { return 1 > 2; }"
assert 1 "int main() { return 1 >= 0; }"
assert 1 "int main() { return 1 >= 1; }"
assert 0 "int main() { return 1 >= 2; }"

assert 1 "int main() { return 1; 2; 3; }"
assert 2 "int main() { 1; return 2; 3; }"
assert 3 "int main() { 1; 2; return 3; }"

assert 3 "int main() { int a=3; return a; }"
assert 3 "int main() { int foo=3; return foo; }"
assert 8 "int main() { int foo123=3; int bar=5; return foo123+bar; }"

assert 3 "int main() { if (0) return 2; return 3; }"
assert 3 "int main() { if (1-1) return 2; return 3; }"
assert 2 "int main() { if (0 <= 1) return 2; return 3; }"
assert 2 "int main() { if (1 > 0) return 2; else return 3; }"

assert 3 "int main() { 1; {2;} return 3; }"

assert 10 "int main() { int i=0; while(i<10) i=i+1; return i; }"
assert 55 "int main() { int i=0; int j=0; while(i<=10) {j=i+j; i=i+1;} return j; }"

assert 10 "int main() { int a=0; for (a = 0; a < 10; a = a + 1) a = a + 1; return a; }"
assert 3 "int main() { for (;;) return 3; return 5; }"
assert 55 "int main() { int i=0; int j=0; for (i=0; i<=10; i=i+1) j=i+j; return j; }"

assert 3 "int main() { return ret3(); }"
assert 5 "int main() { return ret5(); }"
assert 8 "int main() { return add(3, 5); }"
assert 15 "int main() { return mul(3, 5); }"
assert 21 "int main() { return add6(1, 2, 3, 4, 5, 6); }"

assert 32 "int ret32() { return 32; } int main() { return ret32(); }"
assert 7 "int add2(int x, int y) { return x + y; } int main() { return add2(3, 4); }"
assert 12 "int mul2(int x, int y) { return x * y; } int main() { return mul2(3, 4); }"
assert 55 "int fib(int n) { if (n < 2) return n; else return fib(n-1) + fib(n-2); } int main() { return fib(10); }"

assert 3 "int main() { int x = 3; return *(&x); }"
assert 3 "int main() { int x = 3; int *y = &x; int **z = &y; return **z; }"
assert 5 "int main() { int x=3; int *y=&x; *y=5; return x; }"
assert 5 "int main() { int x=3; int y=5; return *(&x + 1); }"
assert 3 "int main() { int x=3; int y=5; return *(&y - 1); }"
assert 7 "int main() { int x=3; int y=5; *(&x+1)=7; return y; }"
assert 7 "int main() { int x=3; int y=5; *(&y-1)=7; return x; }"
assert 8 "int foo(int *x, int y) { return *x + y; } int main() { int x=3; int y=5; return foo(&x, y); }"

assert 3 "int main() { int x[2]; int *y = &x; *y = 3; return *x; }"

assert 3 "int main() { int x[3]; *x=3; *(x+1)=4; *(x+2)=5; return *x; }"
assert 4 "int main() { int x[3]; *x=3; *(x+1)=4; *(x+2)=5; return *(x+1); }"
assert 5 "int main() { int x[3]; *x=3; *(x+1)=4; *(x+2)=5; return *(x+2); }"

assert 0 "int main() { int x[2][3]; int *y=x; *y=0; return **x; }"
assert 1 "int main() { int x[2][3]; int *y=x; *(y+1)=1; return *(*x+1); }"
assert 2 "int main() { int x[2][3]; int *y=x; *(y+2)=2; return *(*x+2); }"
assert 3 "int main() { int x[2][3]; int *y=x; *(y+3)=3; return **(x+1); }"
assert 4 "int main() { int x[2][3]; int *y=x; *(y+4)=4; return *(*(x+1)+1); }"
assert 5 "int main() { int x[2][3]; int *y=x; *(y+5)=5; return *(*(x+1)+2); }"
assert 6 "int main() { int x[2][3]; int *y=x; *(y+6)=6; return **(x+2); }"

assert 3 "int main() { int x[3]; *x=3; x[1]=4; x[2]=5; return *x; }"
assert 4 "int main() { int x[3]; *x=3; x[1]=4; x[2]=5; return *(x+1); }"
assert 5 "int main() { int x[3]; *x=3; x[1]=4; x[2]=5; return *(x+2); }"
assert 5 "int main() { int x[3]; *x=3; x[1]=4; 2[x]=5; return *(x+2); }"

assert 0 "int main() { int x[2][3]; int *y=x; y[0]=0; return x[0][0]; }"
assert 1 "int main() { int x[2][3]; int *y=x; y[1]=1; return x[0][1]; }"
assert 2 "int main() { int x[2][3]; int *y=x; y[2]=2; return x[0][2]; }"
assert 3 "int main() { int x[2][3]; int *y=x; y[3]=3; return x[1][0]; }"
assert 4 "int main() { int x[2][3]; int *y=x; y[4]=4; return x[1][1]; }"
assert 5 "int main() { int x[2][3]; int *y=x; y[5]=5; return x[1][2]; }"
assert 6 "int main() { int x[2][3]; int *y=x; y[6]=6; return x[1][3]; }"

assert 8 "int main() { int x = 0; return sizeof(x); }"
assert 8 "int main() { return sizeof(1); }"
assert 8 "int main() { int *x; return sizeof(x); }"

assert 1 "int main() { char x = 1; return x; }"
assert 1 "int main() { char x = 1; char y = 2; return x; }"
assert 2 "int main() { char x = 1; char y = 2; return y; }"

assert 1 "int main() { char x; return sizeof(x); }"
assert 10 "int main() { char x[10]; return sizeof(x); }"
assert 1 "int main() { return sub_char(7, 3, 3); } char sub_char(char a, char b, char c) { return a-b-c; }"

echo OK
