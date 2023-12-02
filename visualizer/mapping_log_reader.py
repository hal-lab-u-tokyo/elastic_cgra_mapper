from mapping import *
import parse
import sys
import os
import csv


class MappingLogInfo:
    def __init__(self):
        self.row: int = 0
        self.column: int = 0
        self.context_size: int = 0
        self.memory_io: MemoryIOType
        self.cgra_type: CGRAType
        self.network_type: NetworkType
        self.mapping_succeed: bool = False
        self.mapping_time: float
        self.num_threads: int


def mapping_log_reader(file_path) -> MappingLogInfo:
    log_info = MappingLogInfo()
    with open(file_path) as f:
        is_setting = -1
        for line in f:
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

            if line.find("Optimal solution found(tolerance") != -1:
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
