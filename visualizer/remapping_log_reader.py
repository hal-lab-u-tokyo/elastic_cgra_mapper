from mapping import *
import parse
from enum import Enum
from json_reader import read_mapping_from_json
import os
import re

class RemapperType(Enum):
    FullSearch = 0
    Greedy = 1
    DP = 2

    def to_string(self):
        if self == RemapperType.FullSearch:
            return "FullSearch"
        elif self == RemapperType.Greedy:
            return "Greedy"
        elif self == RemapperType.DP:
            return "DP"
        else:
            return "Unknown"
        
    @staticmethod
    def from_string(input):
        if input == "FullSearch":
            return RemapperType.FullSearch
        elif input == "Greedy" or input == "Naive":
            return RemapperType.Greedy
        elif input == "DP":
            return RemapperType.DP
        else:
            return None

class RemapperLogInfo:
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
        self.remapper_time: float = 0
        self.parallel_num: int
        self.remapper_mode: RemapperType

    def get_input_as_str(self):
        return self.benchmark + str(self.row) + "_" + str(self.column) + "_" + str(self.context_size) + "_" + str(self.memory_io.value) + "_" + str(self.cgra_type.value) + "_" + str(self.network_type.value) + "_" + str(self.parallel_num) + "_" + str(self.remapper_mode.value)
    
    def get_unix_time(self):
        file_name = os.path.basename(self.log_file_path)
        find_number = re.findall(r"\d+", file_name)
        if len(find_number) == 0:
            return -1
        return int(find_number[0])

def remapping_log_reader(log_file_path, mapping_file_path, benchmark_list=[]) -> RemapperLogInfo:
    remapper_log_info = RemapperLogInfo()
    remapper_log_info.log_file_path = log_file_path
    remapper_log_info.RemapperType = None
    dir_list = log_file_path.split("/")
    for dir_name in dir_list:
        if dir_name in benchmark_list:
            remapper_log_info.benchmark = dir_name
    with open(log_file_path) as f:
        read_mode = False
        for line in f:
            if read_mode == False:
                parsed = parse.parse("mode: {:w}\n", line)
                if parsed != None:
                    remapper_log_info.remapper_mode = RemapperType.from_string(parsed[0])
                    read_mode = True  
                    continue  
            
            parsed = parse.parse("total {:d} parallel remapping time: {:f}\n", line)
            if parsed != None:
                remapper_log_info.parallel_num = parsed[0]
                remapper_log_info.remapper_time = remapper_log_info.remapper_time + parsed[1]
                continue

    if remapper_log_info.RemapperType == None:
        raise ValueError("remapper log file is invalid")

    try:
        mapping_log_info = read_mapping_from_json(mapping_file_path)
    except:
        raise ValueError("mapping file is invalid")

    remapper_log_info.row = mapping_log_info.row_num
    remapper_log_info.column = mapping_log_info.column_num
    remapper_log_info.context_size = mapping_log_info.context_size
    remapper_log_info.memory_io = mapping_log_info.memory_io_type
    remapper_log_info.cgra_type = mapping_log_info.CGRA_type
    remapper_log_info.network_type = mapping_log_info.network_type

    return remapper_log_info
    
    