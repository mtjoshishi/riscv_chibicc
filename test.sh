#!/bin/sh

RISCV64_CC=riscv64-linux-gnu-gcc

assert() {
  expected="$1"
  input="$2"

  ./riscv_chibicc "$input" > tmp.s || exit 1
  $RISCV64_CC -static tmp.s -o tmp.elf || exit 1

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

assert 0 "return 0;"
assert 42 "return 42;"
assert 21 "return 5+20-4;"
assert 41 "return 12 + 34 - 5;"
assert 47 "return 5+6*7;"
assert 15 "return 5*(9-6);"
assert 4 "return (3+5)/2;"
assert 10 "return -10+20;"
assert 10 "return - -10;"
assert 10 "return - - +10;"

assert 0 "return 0 == 1;"
assert 1 "return 42 == 42;"
assert 1 "return 0 != 1;"
assert 0 "return 42 != 42;"

assert 1 "return 0 < 1;"
assert 0 "return 1 < 1;"
assert 0 "return 2 < 1;"
assert 1 "return 0 <= 1;"
assert 1 "return 1 <= 1;"
assert 0 "return 2 <= 1;"

assert 1 "return 1 > 0;"
assert 0 "return 1 > 1;"
assert 0 "return 1 > 2;"
assert 1 "return 1 >= 0;"
assert 1 "return 1 >= 1;"
assert 0 "return 1 >= 2;"

assert 1 "return 1; 2; 3;"
assert 2 "1; return 2; 3;"
assert 3 "1; 2; return 3;"

assert 3 "a=3; return a;"
assert 3 "foo=3; return foo;"
assert 8 "foo123=3; bar=5; return foo123+bar;"

assert 3 "if (0) return 2; return 3;"
assert 3 "if (1-1) return 2; return 3;"
assert 2 "if (0 <= 1) return 2; return 3;"
assert 2 "if (1 > 0) return 2; else return 3;"

assert 10 "i=0; while(i<10) i=i+1; return i;"

assert 10 "a=0; for (a = 0; a < 10; a = a + 1) a = a + 1; return a;"
assert 3 "for (;;) return 3; return 5;"

echo OK
