#!/bin/bash
set -e

# to find llvm-config
#export PATH=/path/to/src/llvm/build/bin/:$PATH
# added to python path
#export PY_LIBCLANG=/path/to/src/llvm/tools/clang/bindings/python

listdir="$(pwd)/"

pushd ../tracergen/
for list in cstd_headers.txt posix_headers.txt openssl_headers.txt; do
    out="$(basename -s '_headers.txt' "$list").c"
    echo "Generating $out from $list"
    ./lltaptracergen -o "$listdir/$out" --from-lists "$listdir/$list"
done
