from entity import *
import parse
import sys
import os
import csv
import re

class MappingLogInfo:
    def __init__(self):
        self.log_file_path = ""
        self.benchmark = ""
        self.row: int = 0
        self.column: int = 0
        self.context_size: int = 0
        self.memory_io: MemoryIOType
        self.cgra_type: CGRAType
        self.network_type: NetworkType
        self.mapping_succeed: bool = False
        self.mapping_time: float = -1
        self.num_threads: int = -1
        self.timeout: float = -1
        self.parallel_num: int = -1

    def get_input_as_str(self):
        return self.benchmark + str(self.row) + "_" + str(self.column) + "_" + str(self.context_size) + "_" + str(self.memory_io.value) + "_" + str(self.cgra_type.value) + "_" + str(self.network_type.value) + str(self.num_threads) + "_" + str(self.timeout) + "_" + str(self.parallel_num)

    def get_unix_time(self):
        file_name = os.path.basename(self.log_file_path)
        find_number = re.findall(r"\d+", file_name)
        if len(find_number) == 0:
            return -1
        return int(find_number[0])

def mapping_log_reader(file_path, benchmark_list=[]) -> MappingLogInfo:
    log_info = MappingLogInfo()
    log_info.log_file_path = file_path

    dir_list = file_path.split("/")
    for dir_name in dir_list:
        if dir_name in benchmark_list:
            log_info.benchmark = dir_name

    with open(file_path) as f:
        is_setting = -1
        for line in f:
            parsed = parse.parse("timeout (s): {:d}\n", line)
            if parsed != None:
                log_info.timeout = parsed[0]

            parsed = parse.parse("parallel num: {:d}\n", line)
            if parsed != None:
                log_info.parallel_num = parsed[0]                

            if line == "-- CGRA setting --\n":
                is_setting = 0
                continue

            if is_setting >= 0:
                if is_setting == 0:
                    parsed = parse.parse("row: {:d}\n", line)
                    log_info.row = int(parsed[0])
                elif is_setting == 1:
                    parsed = parse.parse("column: {:d}\n", line)
                    log_info.column = int(parsed[0])
                elif is_setting == 2:
                    parsed = parse.parse("context_size: {:d}\n", line)
                    log_info.context_size = int(parsed[0])
                elif is_setting == 3:
                    parsed = parse.parse("memory_io: {:w}\n", line)
                    log_info.memory_io = MemoryIOType.get_from_string(
                        parsed[0])
                elif is_setting == 4:
                    parsed = parse.parse("cgra_type: {:w}\n", line)
                    log_info.cgra_type = CGRAType.get_from_string(parsed[0])
                elif is_setting == 5:
                    parsed = parse.parse("network_type: {:w}\n", line)
                    log_info.network_type = NetworkType.get_from_string(
                        parsed[0])

                if is_setting != 5:
                    is_setting = is_setting + 1
                else:
                    is_setting = -1

                continue

            parsed = parse.parse(
                "Explored {:d} nodes ({:d} simplex iterations) in {:f} seconds\n", line)
            if parsed != None:
                log_info.mapping_time = parsed[2]

            parsed = parse.parse(
                "Thread count was {:d} (of {:d} available processors)\n", line)
            if parsed != None:
                log_info.num_threads = parsed[0]

            if line == "Model is infeasible\n":
                log_info.mapping_succeed = False

            if line.find("Optimal solution found (tolerance") != -1:
                log_info.mapping_succeed = True

    return log_info


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