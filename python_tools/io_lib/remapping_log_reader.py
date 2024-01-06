from entity import *
import parse
from enum import Enum
from .mapping_json_reader import *
from .cgra_json_reader import *
import os
import re
from typing import Tuple

def get_parallel_num(log_file):
    exec_log_file = log_file.replace("/log/", "/exec_log/").replace("remapping_", "exec_log_")
    parallel_num = 0
    with open(exec_log_file) as f:
        for line in f:
            if line == "----- mapping -----\n":
                parallel_num = parallel_num + 1

    return parallel_num

def remapping_log_reader(log_file_path, benchmark_list=[]) -> Tuple[bool, RemapperLogInfo]:
    remapper_log_info = RemapperLogInfo()
    remapper_log_info.log_file_path = log_file_path
    remapper_log_info.remapper_mode = None
    remapper_log_info.parallel_num = None

    dir_list = log_file_path.split("/")
    for dir_name in dir_list:
        if dir_name in benchmark_list:
            remapper_log_info.benchmark = dir_name
    
    with open(log_file_path) as f:
        read_mode = False
        line_num = 1
        mapping_file_num = 100000000
        for line in f:
            if line_num >= 3:
                parsed = parse.parse(">> mapping file: {}\n", line)
                if parsed == None:
                    mapping_file_num = len(remapper_log_info.mapping_json_list)
                else:
                    remapper_log_info.mapping_json_list.append(parsed[0])
            if line_num == mapping_file_num + 3:
                cgra_file = parse.parse("cgra file: {}\n", line)[0]
                cgra = cgra_json_reader(cgra_file)
                remapper_log_info.row = cgra.row
                remapper_log_info.column = cgra.column
                remapper_log_info.context_size = cgra.context_size
                remapper_log_info.memory_io = cgra.memory_io_type
                remapper_log_info.cgra_type = cgra.cgra_type
                remapper_log_info.network_type = cgra.network_type
                remapper_log_info.local_reg_size = cgra.local_reg_size
            if line_num == mapping_file_num + 5:
                mode = parse.parse("remapper mode: {:w}\n", line)[0]
                remapper_log_info.remapper_mode = RemapperType.from_string(mode)            
            if line_num == mapping_file_num + 7:
                remapping_time = float(parse.parse("remapping time (s): {}\n", line)[0])
            if line_num == mapping_file_num + 8:
                remapper_log_info.parallel_num = get_parallel_num(log_file_path)

            line_num = line_num + 1
            
    if line_num <= mapping_file_num + 8:
        return (False, remapper_log_info)

    return (True, remapper_log_info)
    
    