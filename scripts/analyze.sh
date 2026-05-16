#!/bin/bash
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <experiment_dir_path>"
    exit 1
fi

experiment_dir_path=$(realpath "$1")

plotter_config_file_path=/home/ubuntu/elastic_cgra_mapper/data/analyzer/plotter_config.json

cd /home/ubuntu/elastic_cgra_mapper/python_tools/analyzer
python3 analyze.py "$experiment_dir_path" "$plotter_config_file_path"
