# !/bin/bash

set -e

DEBUG_ENABLE=0 # 1: enable debug, 0: disable debug

cd /home/ubuntu/elastic_cgra_mapper
if [ ! -d "build" ]; then
    mkdir build
fi
cd build
if [ $DEBUG_ENABLE -eq 1 ]; then
    cmake -U "GUROBI_*" -GNinja -DCMAKE_BUILD_TYPE=Debug ..
else
    cmake -U "GUROBI_*" -GNinja ..
fi

ninja

cd ..
