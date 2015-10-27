#!/bin/bash
set -eu -o pipefail

OPTS="-Wall -pedantic -emit-llvm"
LLTAPSO="../build/llvmpass/libLLTap.so"
LLTAPRTSO="../build/lib/liblltaprt.so"

SRC=$(basename -s ".c" $1)
HOOKSRC=$(basename -s ".c" $2)

BCFILES="$SRC.inst.bc $HOOKSRC.bc"
BCOUT="$SRC.exec.bc"
OBJOUT="$SRC.exec.o"
BINOUT="$SRC.exec.bin"

ulimit -c unlimited

echo "Using"
opt --version
clang --version

set -x
clang $OPTS -I../include -c $2
clang $OPTS -S $SRC.c
clang $OPTS -c $SRC.c
opt -verify -verify-each \
    -load $LLTAPSO \
    -LLTapInst \
    "$SRC.bc" > "$SRC.inst.bc"
llvm-dis "$SRC.inst.bc"
llvm-link $BCFILES -o "$BCOUT"
llc -filetype=obj "$BCOUT" -o "$OBJOUT"
g++ -L ../build/lib/ "$OBJOUT" -o "$BINOUT" -llltaprt
#clang -Xclang -load -Xclang "$LLTAPSO" -LLTapInst \
#    -L "../build/lib/" \
#    -I "../include/" \
#    "$1" "$2" \
#    -o "$BINOUT" \
#    -llltaprt
env LD_LIBRARY_PATH=../build/lib "./$BINOUT"
