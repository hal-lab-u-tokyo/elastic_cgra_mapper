import os
import sys
sys.path.append("/home/ubuntu/elastic_cgra_mapper/python_tools")
from io_lib.load_remapper_result import *
from io_lib.load_plotter_config import *
from visualizer.plot_remapping_result import RemappingResultPlotter

def remapper_result_to_csv(remapper_result):
    csv_str = ""
    csv_str += remapper_result.benchmark_name + ","
    csv_str += str(remapper_result.cgra.row) + ","
    csv_str += str(remapper_result.cgra.column) + ","
    csv_str += str(remapper_result.cgra.memory_io_type) + ","
    csv_str += str(remapper_result.cgra.cgra_type) + ","
    csv_str += str(remapper_result.cgra.network_type) + ","
    csv_str += str(remapper_result.cgra.local_reg_size) + ","
    csv_str += str(remapper_result.cgra.context_size) + ","
    csv_str += str(remapper_result.cgra.loop_controller_list) + ","
    csv_str += str(remapper_result.mapping_succeed) + ","
    csv_str += str(remapper_result.remapper_time) + ","
    csv_str += str(remapper_result.parallel_num) + ","
    csv_str += str(remapper_result.mapping_type_num) + ","
    csv_str += str(remapper_result.num_available_mappings) + ","
    csv_str += str(remapper_result.database_mapping_files_num)

    return csv_str

if __name__ == "__main__":
    args = sys.argv
    if len(args) == 2:
        print("ERROR: invalid arguments")
        raise ValueError
    experiment_dir: str = args[1]
    plotter_config_path = args[2]

    remapper_dir = os.path.join(experiment_dir, "remapper", "remapping")
    if not os.path.exists(remapper_dir):
        print("ERROR: remapper directory does not exist")
        raise ValueError

    analysis_dir = os.path.join(experiment_dir, "remapper", "analysis")
    if not os.path.exists(analysis_dir):
        os.makedirs(analysis_dir)

    remapper_results = []
    for dir_name in os.listdir(remapper_dir):
        dir_path = os.path.join(remapper_dir, dir_name)
        remapper_results.append(load_remapper_result(dir_path))

    # output csv
    output_file_path = os.path.join(analysis_dir, "remapper_result.csv")
    with open(output_file_path, "w") as f:
        f.write("benchmark_name,cgra_row,cgra_column,cgra_memory_io,cgra_type,cgra_network_type,cgra_local_reg_size,cgra_context_size,cgra_loop_controllers,mapping_succeed,remapper_time_s,parallel_num,mapping_type_num,num_available_mappings,database_mapping_files_num\n")
        for remapper_result in remapper_results:
            csv_str = remapper_result_to_csv(remapper_result)
            f.write(csv_str + "\n")
    output_file_path = os.path.join(analysis_dir, "remapper_failed_results.csv")
    with open(output_file_path, "w") as f:
        f.write("benchmark_name,cgra_row,cgra_column,cgra_memory_io,cgra_type,cgra_network_type,cgra_local_reg_size,cgra_context_size,cgra_loop_controllers,mapping_succeed,remapper_time_s,parallel_num,mapping_type_num,num_available_mappings,database_mapping_files_num\n")
        for remapper_result in remapper_results:
            if not remapper_result.mapping_succeed:
                csv_str = remapper_result_to_csv(remapper_result)
                f.write(csv_str + "\n")

    plotter_config = load_plotter_config(plotter_config_path)

    remapping_result_plotter = RemappingResultPlotter(remapper_results, plotter_config, analysis_dir)

    remapping_result_plotter.plot_benchmark_comparison()
    remapping_result_plotter.plot_cgra_size_comparison()
    remapping_result_plotter.plot_cgra_type_comparison()
    remapping_result_plotter.plot_available_mapping_num_comparison()
