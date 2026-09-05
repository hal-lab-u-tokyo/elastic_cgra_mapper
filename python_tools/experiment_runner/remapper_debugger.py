#!/usr/bin/env python3
import argparse
import ast
import csv
import json
import multiprocessing
import os
import shutil
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PYTHON_TOOLS_DIR = os.path.dirname(SCRIPT_DIR)
sys.path.insert(0, SCRIPT_DIR)
sys.path.insert(0, PYTHON_TOOLS_DIR)
from exec import *
from remapper_runner import RemappingRunnerConfig


REQUIRED_COLUMNS = {
    "benchmark_name", "cgra_row", "cgra_column", "cgra_memory_io",
    "cgra_type", "cgra_network_type", "cgra_local_reg_size",
    "cgra_context_size", "remapper_type", "num_available_mappings",
}


def generate_remapper_input(row, config, output_dir, program="/home/ubuntu/elastic_cgra_mapper/build/remapping"):
    loop_controllers = ast.literal_eval(row.get("cgra_loop_controllers", "[]"))
    cgra = CGRA(
        CGRAType.get_from_string(row["cgra_type"]),
        int(row["cgra_row"]),
        int(row["cgra_column"]),
        int(row["cgra_context_size"]),
        MemoryIOType.get_from_string(row["cgra_memory_io"]),
        NetworkType.get_from_string(row["cgra_network_type"]),
        int(row["cgra_local_reg_size"]),
        loop_controllers,
    )
    return RemapperInput(
        config.database_path,
        cgra,
        os.path.join(config.kernel_dir_path, row["benchmark_name"] + ".dot"),
        output_dir,
        RemapperType.from_string(row["remapper_type"]),
        config.remapper_timeout_s,
        int(row["num_available_mappings"]),
        program,
    )


def read_commands(csv_path, config, output_dir, program="/home/ubuntu/elastic_cgra_mapper/build/remapping"):
    with open(csv_path, newline="") as csv_file:
        reader = csv.DictReader(csv_file)
        missing = REQUIRED_COLUMNS - set(reader.fieldnames or [])
        if missing:
            raise ValueError("Missing CSV columns: " + ", ".join(sorted(missing)))
        return [generate_remapper_input(row, config, output_dir, program) for row in reader]


def launch_configuration(remapper_input, index, program, artifact_dir):
    cgra_path = os.path.join(artifact_dir, "launch_cgra_%04d.json" % index)
    remapper_input.cgra.dump_to_json(cgra_path)
    return {
        "name": "Remapper failed case %04d" % index,
        "type": "cppdbg",
        "request": "launch",
        "program": program,
        "cwd": os.path.dirname(program),
        "args": [
            "--database_dir", remapper_input.mapping_dir_path,
            "--dfg_file", remapper_input.dfg_file_path,
            "--cgra_file", cgra_path,
            "--output_dir", os.path.join(artifact_dir, "launch_case_%04d" % index),
            "--remapper_mode", str(remapping_type_to_int(remapper_input.remapper_mode)),
            "--timeout_s", str(remapper_input.timeout_s),
            "--num_available_mappings", str(remapper_input.num_available_mappings),
        ],
        "stopAtEntry": False,
        "externalConsole": False,
        "MIMode": "gdb",
    }


def write_launch_json(inputs, path, program):
    artifact_dir = os.path.dirname(path)
    os.makedirs(artifact_dir, exist_ok=True)
    document = {
        "version": "0.2.0",
        "configurations": [
            launch_configuration(item, index + 1, program, artifact_dir)
            for index, item in enumerate(inputs)
        ],
    }
    with open(path, "w") as output:
        json.dump(document, output, indent=2)
        output.write("\n")


def main():
    parser = argparse.ArgumentParser(description="Re-run remapper commands from analyzer CSV.")
    parser.add_argument("experiment_dir")
    parser.add_argument("--csv")
    parser.add_argument("--output-dir")
    parser.add_argument("--launch-json")
    parser.add_argument("--program", default="/home/ubuntu/elastic_cgra_mapper/build/remapping")
    parser.add_argument("--launch-only", action="store_true")
    args = parser.parse_args()

    experiment_dir = os.path.realpath(args.experiment_dir)
    csv_path = args.csv or os.path.join(
        experiment_dir, "remapper", "analysis", "remapper_failed_results.csv"
    )
    config = RemappingRunnerConfig()
    config.load(os.path.join(experiment_dir, "remapper_config.json"))
    debug_experiment = args.output_dir or os.path.join(experiment_dir, "debug-rerun")
    output_dir = os.path.join(debug_experiment, "remapper", "remapping")
    os.makedirs(output_dir, exist_ok=True)
    shutil.copy(
        os.path.join(experiment_dir, "remapper_config.json"),
        os.path.join(debug_experiment, "remapper_config.json"),
    )
    inputs = read_commands(csv_path, config, output_dir, args.program)

    analysis_dir = os.path.join(debug_experiment, "remapper", "analysis")
    os.makedirs(analysis_dir, exist_ok=True)
    launch_path = args.launch_json or os.path.join(analysis_dir, "launch.json")
    write_launch_json(inputs, launch_path, args.program)
    print("Parsed %d commands from %s" % (len(inputs), csv_path))
    print("VS Code launch configurations: %s" % launch_path)
    if args.launch_only or not inputs:
        return 0

    log_path = os.path.join(debug_experiment, "debug.log")
    lock = multiprocessing.Lock()
    pool = multiprocessing.Pool(config.process_num, initializer=init, initargs=(lock, log_path))
    pool.map(remapper_exec, inputs)
    pool.close()
    pool.join()
    return 0


if __name__ == "__main__":
    sys.exit(main())
