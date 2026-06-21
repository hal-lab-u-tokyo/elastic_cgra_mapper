#!/bin/bash
set -e

if [ "$#" -ne 1 ] && [ "$#" -ne 2 ]; then
    echo "Usage: $0 <experiment_dir_path> [baseline_experiment_dir_path]"
    exit 1
fi

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(dirname "$script_dir")
experiment_dir_path=$(realpath "$1")
baseline_experiment_dir_path=""
if [ "$#" -eq 2 ]; then
    baseline_experiment_dir_path=$(realpath "$2")
fi

plotter_config_file_path=$repo_dir/data/analyzer/plotter_config.json

cd "$repo_dir/python_tools/analyzer"
python3 analyze.py "$experiment_dir_path" "$plotter_config_file_path"

if [ -n "$baseline_experiment_dir_path" ]; then
    python3 analyze.py "$baseline_experiment_dir_path" "$plotter_config_file_path"
    cd "$repo_dir"
    comparison_output_path="$experiment_dir_path/remapper/analysis/remapper_quality_compare.txt"
    set +e
    python_tools/analyzer/compare_remapper_result_quality.py \
        "$baseline_experiment_dir_path/remapper/analysis/remapper_result.csv" \
        "$experiment_dir_path/remapper/analysis/remapper_result.csv" \
        > "$comparison_output_path" 2>&1
    compare_status=$?
    set -e
    cat "$comparison_output_path"
    echo "Saved remapper quality comparison to $comparison_output_path"
    exit $compare_status
fi
