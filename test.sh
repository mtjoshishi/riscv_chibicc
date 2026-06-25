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

assert 0 0
assert 42 42
assert 21 "5+20-4"
assert 41 " 12 + 34 - 5"

echo OK
