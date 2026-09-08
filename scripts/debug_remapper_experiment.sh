#!/bin/bash
set -euo pipefail

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "Usage: $0 <failed_results_csv> [plotter_config_path]"
    exit 1
fi

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(dirname "$script_dir")
failed_csv=$(realpath "$1")
plotter_config_path=${2:-"$repo_dir/data/analyzer/plotter_config.json"}

if [ ! -f "$failed_csv" ]; then
    echo "Failed-results CSV does not exist: $failed_csv" >&2
    exit 1
fi

experiment_info_output=$(
    python3 "$repo_dir/python_tools/experiment_runner/remapper_debugger.py" \
        --resolve-experiment "$failed_csv"
)
readarray -t experiment_info <<< "$experiment_info_output"
experiment_dir_path=${experiment_info[0]}
experiment_date=${experiment_info[1]}

failed_result_count=$(($(wc -l < "$failed_csv") - 1))
if [ "$failed_result_count" -le 0 ]; then
    echo "No failed remapper results were found. Nothing to re-execute."
    exit 0
fi

debug_date=$(date +%Y-%m-%d-%H-%M-%S)
debug_experiment="$repo_dir/debug/$experiment_date/$debug_date"
launch_json="$debug_experiment/remapper/analysis/launch.json"

mkdir -p "$debug_experiment"
cp "$failed_csv" "$debug_experiment/input_failed_results.csv"
python3 - "$debug_experiment/debug_metadata.json" "$experiment_dir_path" "$experiment_date" "$failed_csv" <<'PY'
import json
import os
import sys

metadata_path, experiment_dir, experiment_date, input_csv = sys.argv[1:]
with open(metadata_path, "w") as output:
    json.dump(
        {
            "experiment_date": experiment_date,
            "experiment_dir": os.path.realpath(experiment_dir),
            "input_csv": os.path.realpath(input_csv),
        },
        output,
        indent=2,
    )
    output.write("\n")
PY

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
cp "$debug_experiment/remapper/analysis/remapper_failed_results.csv" \
    "$debug_experiment/failed_results.csv"
echo "Remaining failures: $debug_experiment/failed_results.csv"
echo "Copy VS Code configurations from $launch_json"
