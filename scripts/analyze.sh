#!/bin/bash
set -e

if [ "$#" -ne 1 ] && [ "$#" -ne 2 ]; then
    echo "Usage: $0 <experiment_dir_path> [baseline_experiment_dir_path]"
    exit 1
fi

repo_dir=/home/ubuntu/elastic_cgra_mapper
experiment_dir_path=$(realpath "$1")
baseline_experiment_dir_path=""
if [ "$#" -eq 2 ]; then
    baseline_experiment_dir_path=$(realpath "$2")
fi

plotter_config_file_path=$repo_dir/data/analyzer/plotter_config.json

cd $repo_dir/python_tools/analyzer
python3 analyze.py "$experiment_dir_path" "$plotter_config_file_path"

if [ -n "$baseline_experiment_dir_path" ]; then
    python3 analyze.py "$baseline_experiment_dir_path" "$plotter_config_file_path"
    cd $repo_dir
    scripts/compare_remapper_result_quality.py \
        "$baseline_experiment_dir_path/remapper/analysis/remapper_result.csv" \
        "$experiment_dir_path/remapper/analysis/remapper_result.csv"
fi
