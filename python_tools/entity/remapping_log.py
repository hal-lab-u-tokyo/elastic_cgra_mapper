from enum import Enum
import os
import re
from .cgra import *

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
        if input == "FullSearch" or input == "full_search":
            return RemapperType.FullSearch
        elif input == "Greedy" or input == "Naive" or input == "greedy":
            return RemapperType.Greedy
        elif input == "DP" or input == "dp":
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
        self.local_reg_size: int = 0
        self.mapping_succeed: bool = False
        self.remapper_time: float = 0
        self.parallel_num: int
        self.remapper_mode: RemapperType
        self.mapping_json_list = []

    def get_input_as_str(self):
        return self.benchmark + str(self.row) + "_" + str(self.column) + "_" + str(self.context_size) + "_" + str(self.memory_io.value) + "_" + str(self.cgra_type.value) + "_" + str(self.network_type.value) + "_" + str(self.parallel_num) + "_" + str(self.remapper_mode.value)
    
    def get_unix_time(self):
        file_name = os.path.basename(self.log_file_path)
        find_number = re.findall(r"\d+", file_name)
        if len(find_number) == 0:
            return -1
        return int(find_number[0])

    def get_cgra(self):
        return CGRA(self.cgra_type, self.row, self.column, self.context_size, self.memory_io, self.network_type, self.local_reg_size)