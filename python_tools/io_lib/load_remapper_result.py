import json
import os
import sys
sys.path.append(os.pardir)
from entity.remapping_log import *
from io_lib.cgra_json_reader import *
from io_lib.mapping_json_reader import *


def load_remapper_result(remapper_dir_path):
    remapper_dir_name = os.path.basename(remapper_dir_path)
    cgra_file_name = "cgra_" + remapper_dir_name + ".json"
    input_summary_file_name = "input_summary_" + remapper_dir_name + ".json"
    output_summary_file_name = "output_summary_" + remapper_dir_name + ".json"
    remapping_file_name = "remapping_" + remapper_dir_name + ".json"
    transform_op_name = "transform_op_" + remapper_dir_name + ".json"
    success = True

    with open(os.path.join(remapper_dir_path, input_summary_file_name), 'r') as f:
        print(f"Loading remapper result from {os.path.join(remapper_dir_path, input_summary_file_name)}...")
        input_summary_json = json.load(f)
    if not os.path.exists(os.path.join(remapper_dir_path, output_summary_file_name)):
        success = False
    else:
        success = True
        with open(os.path.join(remapper_dir_path, output_summary_file_name), 'r') as f:
            output_summary_json = json.load(f)
        with open(os.path.join(remapper_dir_path, remapping_file_name), 'r') as f:
            remapping_json = json.load(f)
        with open(os.path.join(remapper_dir_path, transform_op_name), 'r') as f:
            transform_op_json = json.load(f)

    remapper_result = RemapperLogInfo()

    remapper_result.server_name = input_summary_json["host_name"]
    remapper_result.git_commit_id = input_summary_json["git_commit_id"]
    remapper_result.result_path = remapper_dir_path
    dfg_file_path = input_summary_json["dfg_file"]
    remapper_result.benchmark_name = dfg_file_path.split("/")[-1].split(".")[0]
    remapper_result.timeout_s = input_summary_json["timeout_s"]
    remapper_result.num_available_mappings = input_summary_json["num_available_mappings"]
    remapper_result.database_mapping_files_num = len(input_summary_json["mapping_files"])


    cgra_file_path = os.path.join(remapper_dir_path, cgra_file_name)
    print(f"Loading CGRA info from {cgra_file_path}...")
    remapper_result.cgra = cgra_json_reader(cgra_file_path)

    remapper_result.mapping_succeed = success
    if not success:
        remapper_result.remapper_time = None
        remapper_result.parallel_num = None
        remapper_result.mapping_type_num = None
        return remapper_result

    remapper_result.remapper_time = output_summary_json["remapping_time_s"]
    remapper_result.parallel_num = output_summary_json["parallel_num"]
    remapper_result.mapping_type_num = output_summary_json["mapping_type_num"]

    mapping_result = read_mapping_from_json(output_summary_json["mapping_file"])
    remapper_result.utilization = mapping_result.get_utilization()

    return remapper_result
