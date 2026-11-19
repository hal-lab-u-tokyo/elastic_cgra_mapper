#!/bin/sh

set -eu

workspace_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
python_test_dir="$workspace_dir/python_tools/tests"
build_dir="$workspace_dir/build"

echo "Configuring the project..."
cmake -S "$workspace_dir" -B "$build_dir" -GNinja

echo "Building the project..."
cmake --build "$build_dir"

echo "Running ctest..."
ctest --test-dir "$build_dir" -V --no-tests=error

echo "Running python tests..."
python3 -m unittest discover -s "$python_test_dir" -v
