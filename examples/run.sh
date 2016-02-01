#!/bin/bash
set -eu -o pipefail

ADD_CFLAGS=""
LINKLIBS="-llltaprt"
if [[ "$#" -lt 2 ]]; then
    echo "usage: $0 source.c hooks.c [cflags] [-llinkstuff]"
    exit 1
fi
if [[ "$#" -gt 3 ]]; then
    ADD_CFLAGS="$3"
fi
if [[ "$#" -eq 4 ]]; then
    LINKLIBS="$LINKLIBS $4"
fi

# adapt to use llvm built somewhere else, e.g. debug builds
#export PATH=/path/to/src/llvm/build/bin/:$PATH

CFLAGS="-Wall -pedantic"
OPTS="$CFLAGS -emit-llvm $ADD_CFLAGS"
LLTAPSO="../build/llvmpass/libLLTap.so"
LLTAPRTSO="../build/lib/liblltaprt.so"

SRC=$(basename -s ".c" $1)
HOOKSRC=$(basename -s ".c" $2)

BCFILES="$SRC.inst.bc"
BINOUT="$SRC.exec.bin"

echo "Using"
which opt
opt --version
which clang
clang --version

ulimit -c unlimited
set -x
#clang $OPTS -I../include -c
clang $OPTS -S $SRC.c
clang $OPTS -c $SRC.c
opt -verify -verify-each \
    -load $LLTAPSO \
    -LLTapInst \
    "$SRC.bc" > "$SRC.inst.bc"
llvm-dis "$SRC.inst.bc"
clang $CFLAGS $ADD_CFLAGS -I../include/ -L ../build/lib/ "$HOOKSRC.c" $BCFILES -o "$BINOUT" $LINKLIBS

#clang -Xclang -load -Xclang "$LLTAPSO" -LLTapInst \
#    -L "../build/lib/" \
#    -I "../include/" \
#    "$1" "$2" \
#    -o "$BINOUT" \
#    $LINKLIBS

echo "Executing instrumented binary $BINOUT"
env LLTAP_LOGLEVEL=DEBUG LD_LIBRARY_PATH=../build/lib "./$BINOUT"
