# !/bin/bash

DEBUG_ENABLE=1

if [ ! -d "build" ]; then
    mkdir build
fi
cd build
if [ $DEBUG_ENABLE -eq 1 ]; then
    cmake -GNinja -DCMAKE_BUILD_TYPE=Debug ..
else
    cmake -GNinja ..
fi

ninja

cd ..
