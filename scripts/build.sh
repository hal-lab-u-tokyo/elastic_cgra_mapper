#!/bin/sh

set -e

DEBUG_ENABLE=0 # 1: enable debug, 0: disable debug

cd "$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
if [ ! -d "build" ]; then
    mkdir build
fi
cd build
if [ $DEBUG_ENABLE -eq 1 ]; then
    cmake -GNinja -DCMAKE_BUILD_TYPE=Debug ..
else
    cmake -GNinja -DCMAKE_BUILD_TYPE=Release ..
fi

cmake --build .

cd ..
