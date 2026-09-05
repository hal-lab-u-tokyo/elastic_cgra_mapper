# !/bin/bash

workspace_dir=/home/ubuntu/elastic_cgra_mapper
python_test_dir=$workspace_dir/python_tools/tests
if [ ! -d "build" ]; then
    mkdir build
fi

cd ${workspace_dir}/build

echo "Building the project..."
cmake -GNinja ..

echo "Running ctest..."
ninja && ctest -V

echo "Running python tests..."
python3 -m unittest discover -s ${python_test_dir} -v
