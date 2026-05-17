from enum import Enum
import os
import re
from entity import cgra

class RemapperType(Enum):
    FullSearch = 0
    Greedy = 1
    DP = 2
    DPAndFullSearch = 3

    def to_string(self):
        if self == RemapperType.FullSearch:
            return "FullSearch"
        elif self == RemapperType.Greedy:
            return "Greedy"
        elif self == RemapperType.DP:
            return "DP"
        elif self == RemapperType.DPAndFullSearch:
            return "DPAndFullSearch"
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
        elif input == "DPAndFullSearch" or input == "dp_and_full_search":
            return RemapperType.DPAndFullSearch
        else:
            return None


class RemapperLogInfo:
    def __init__(self):
        self.server_name = ""
        self.git_commit_id = ""
        self.num_available_mappings = 0
        self.database_mapping_files_num = 0
        self.result_path = ""
        self.timeout_s = 0
        self.benchmark_name = ""
        self.cgra: CGRA = None
        self.mapping_succeed: bool = False
        self.remapper_time: float = None
        self.parallel_num: int = 0
        self.mapping_type_num: int = 0
        self.remapper_mode: RemapperType
        self.utilization = 0

    def get_cgra(self):
        return self.cgra
