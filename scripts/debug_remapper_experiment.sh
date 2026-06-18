#!/bin/bash
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <experiment_dir_path>"
    exit 1
fi

experiment_dir_path=$(realpath "$1")

echo "Re-executing failed experiments..."
cd /home/ubuntu/elastic_cgra_mapper/python_tools/experiment_runner
python3 remapper_debugger.py "$experiment_dir_path"
