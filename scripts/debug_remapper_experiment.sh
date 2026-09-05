#!/bin/bash
set -euo pipefail

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "Usage: $0 <experiment_dir_path> [plotter_config_path]"
    exit 1
fi

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(dirname "$script_dir")
experiment_dir_path=$(realpath "$1")
plotter_config_path=${2:-"$repo_dir/data/analyzer/plotter_config.json"}

echo "Analyzing remapper results..."
python3 "$repo_dir/python_tools/analyzer/analyze.py" \
    "$experiment_dir_path" "$plotter_config_path"

failed_csv="$experiment_dir_path/remapper/analysis/remapper_failed_results.csv"
failed_result_count=$(($(wc -l < "$failed_csv") - 1))
if [ "$failed_result_count" -le 0 ]; then
    echo "No failed remapper results were found. Nothing to re-execute."
    exit 0
fi

debug_experiment="$experiment_dir_path/debug-reruns/$(date +%Y-%m-%d-%H-%M-%S)"
launch_json="$debug_experiment/remapper/analysis/launch.json"

echo "Re-executing commands from $failed_csv ..."
cd "$repo_dir/python_tools/experiment_runner"
python3 remapper_debugger.py \
    "$experiment_dir_path" \
    --csv "$failed_csv" \
    --output-dir "$debug_experiment" \
    --launch-json "$launch_json" \
    --program "$repo_dir/build/remapping"

echo "Analyzing re-executed results..."
python3 "$repo_dir/python_tools/analyzer/analyze.py" \
    "$debug_experiment" "$plotter_config_path"
echo "Copy VS Code configurations from $launch_json"
