import sys
import os
import json
import csv
sys.path.append(os.pardir)
from exec import *
from remapper_runner import *

def parse_remapper_failed_result_csv_line(line, header_list):
    line = line.strip()
    items = line.split(",")
    remapper_failed_info = {}
    for i in range(len(items)):
        key = header_list[i]
        value = items[i]
        remapper_failed_info[key] = value
    return remapper_failed_info

def generate_remapper_input_from_failed_info(remapper_failed_info, database_dir_path, kernel_dir_path, output_dir_path, timeout_s):
    row = int(remapper_failed_info["cgra_row"])
    col = int(remapper_failed_info["cgra_column"])
    cgra_memory_io = MemoryIOType.get_from_string(remapper_failed_info["cgra_memory_io"])
    cgra_network_type = NetworkType.get_from_string(remapper_failed_info["cgra_network_type"])
    cgra_local_reg_size = int(remapper_failed_info["cgra_local_reg_size"])
    cgra_context_size = int(remapper_failed_info["cgra_context_size"])
    cgra_loop_controller = []
    cgra_type = CGRAType.get_from_string(remapper_failed_info["cgra_type"])
    cgra = CGRA(cgra_type, col, row, cgra_context_size, cgra_memory_io, cgra_network_type, cgra_local_reg_size, cgra_loop_controller)

    dfg_file_path = os.path.join(kernel_dir_path, remapper_failed_info["benchmark_name"] + ".dot")
    remapper_mode = RemapperType.from_string(remapper_failed_info["remapper_type"])
    timeout_s = timeout_s
    num_available_mappings = int(remapper_failed_info["num_available_mappings"])

    remapper_input = RemapperInput(database_dir_path, cgra, dfg_file_path, output_dir_path, remapper_mode, timeout_s, num_available_mappings)
    return remapper_input

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python remapper_debugger.py <remapper_output_dir_path>")
        sys.exit(1)
    args = sys.argv

    remapper_output_dir_path = args[1]
    remapper_failed_result_csv_path = os.path.join(remapper_output_dir_path, "remapper/analysis/remapper_failed_results.csv")
    remapper_config_path = os.path.join(remapper_output_dir_path, "remapper_config.json")

    remapper_config = RemappingRunnerConfig()
    remapper_config.load(remapper_config_path)
    database_dir_path = remapper_config.database_path
    kernel_dir_path = remapper_config.kernel_dir_path
    timeout_s = remapper_config.remapper_timeout_s
    experiment_output_dir_path = os.path.join(remapper_config.output_dir_path, "experiments/" + get_datetime_string() + "/remapper")
    os.makedirs(experiment_output_dir_path, exist_ok=True)

    # output log
    log_file_path = os.path.join(experiment_output_dir_path, "debug.log")
    log_file = open(log_file_path, "w")
    log_file.write("remapper_output_dir_path to debug: " + remapper_failed_result_csv_path + "\n")
    log_file.close()

    failed_remapper_input_list = []

    header = ""
    line_num = 0
    for line in open(remapper_failed_result_csv_path, "r").readlines():
        if line_num == 0:
            header = line.strip()
            header_list = header.split(",")
            line_num += 1
            continue

        remapper_failed_info = parse_remapper_failed_result_csv_line(line, header_list)

        failed_remapper_input_list.append(generate_remapper_input_from_failed_info(remapper_failed_info, database_dir_path, kernel_dir_path, experiment_output_dir_path, timeout_s))

        line_num += 1
    print(f"Total {line_num - 1} failed remapper results are parsed and will be re-executed.")

    lock = multiprocessing.Lock()
    log_file_path = os.path.join(experiment_output_dir_path, get_datetime_string() + ".log")
    check_dir_availability(os.path.dirname(log_file_path))

    pool = multiprocessing.Pool(remapper_config.process_num, initializer=init, initargs=(lock, log_file_path))
    pool.map(remapper_exec, failed_remapper_input_list)
