#!/bin/bash

listdir="$(pwd)/"

pushd ../tracergen/
for list in cstd_headers.txt posix_headers.txt openssl_headers.txt; do
    out="$(basename -s '_headers.txt' "$list").c"
    echo "Generating $out from $list"
    ./lltaptracergen -o "$listdir/$out" --from-lists "$listdir/$list"
done
