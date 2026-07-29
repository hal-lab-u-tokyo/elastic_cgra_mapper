# !/bin/bash

cd /home/ubuntu/elastic_cgra_mapper
if [ ! -d "build" ]; then
    mkdir build
fi

cd build
cmake -GNinja .. && ninja && ctest -V
