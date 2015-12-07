#!/bin/bash
if [[ $1 == "-f" ]]; then
    rm -rf ./build/
fi
mkdir -p build/
pushd build
cmake -G Ninja ..
ninja || ninja-build
popd
