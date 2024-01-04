from entity import *
import parse
import sys
import os
import csv
import re
from .cgra_json_reader import *
from entity import *
from typing import Tuple

def mapping_log_reader(file_path, benchmark_list=[]) -> Tuple[bool, Tuple]:
    log_info = MappingLogInfo()
    log_info.log_file_path = file_path

    dir_list = file_path.split("/")
    for dir_name in dir_list:
        if dir_name in benchmark_list:
            log_info.benchmark = dir_name

    with open(file_path) as f:
        line_num = 1
        for line in f:
            if line_num == 3:
                json_file_path = parse.parse("cgra file: {}\n", line)[0]
                cgra = cgra_json_reader(json_file_path)
                log_info.row = cgra.row
                log_info.column = cgra.column
                log_info.context_size = cgra.context_size
                log_info.memory_io = cgra.memory_io_type
                log_info.cgra_type = cgra.cgra_type
                log_info.network_type = cgra.network_type
            elif line_num == 5:
                log_info.timeout = float(parse.parse("timeout (s): {}\n", line)[0])
            elif line_num == 6:
                log_info.parallel_num = parse.parse("parallel num: {:d}\n", line)[0]
            elif line_num == 8:
                log_info.mapping_time = float(parse.parse("mapping time (s): {}\n", line)[0])
            elif line_num == 9:
                log_info.mapping_succeed = parse.parse("is success: {:w}\n", line)[0]
            elif line_num == 10:
                log_info.mapping_file_path = parse.parse("mapping file: {}\n", line)[0]                     

            line_num = line_num + 1

    if line_num < 11:
        print("Mapping Log Reader Error: " + file_path)
        return (False, log_info)

    return (True, log_info)


if __name__ == "__main__":
    args = sys.argv

    dir_path = args[1]
    output_csv_path = args[2]

    log_list = []
    file_path_list = os.listdir(dir_path)
    for file_path in file_path_list:
        file_path = dir_path + file_path
        if os.path.isfile(file_path):
            log_list.append(mapping_log_reader(file_path))

    with open(output_csv_path, "w") as f:
        writer = csv.writer(f)
        for memory_io in MemoryIOType:
            for cgra_type in CGRAType:
                for network_type in NetworkType:
                    writer.writerow([memory_io, cgra_type, network_type])
                    tmp_log_list = []
                    for log in log_list:
                        if (log.memory_io is memory_io) and (log.cgra_type is cgra_type) and (log.network_type is network_type):
                            tmp_log_list.append(log)

                    cgra_size_list = []
                    mapping_time_list = []
                    for log in tmp_log_list:
                        cgra_size_list.append(
                            "(" + str(log.row) + "," + str(log.column) + "," + str(log.context_size) + ")")
                        if log.mapping_succeed:
                            mapping_time_list.append(log.mapping_time)
                        else:
                            mapping_time_list.append(-1)

                    writer.writerow(cgra_size_list)
                    writer.writerow(mapping_time_list)
        writer.writerow("")
