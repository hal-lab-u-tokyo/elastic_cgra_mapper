import os
import re

class MappingLogInfo:
    def __init__(self):
        self.log_file_path = ""
        self.mapping_file_path = ""

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