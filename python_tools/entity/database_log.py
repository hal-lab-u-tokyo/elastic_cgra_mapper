import os
import re

class DatabaseLogInfo:
  def __init__(self):
    self.log_file_path = ""
    self.benchmark = ""

    self.row: int = 0
    self.column: int = 0
    self.context_size: int = 0
    self.memory_io: MemoryIOType
    self.cgra_type: CGRAType
    self.network_type: NetworkType

    self.timeout: float = -1
    self.creating_time = -1
    self.min_utilization: float = 0

    self.mapping_log_file_list = []

  def get_unix_time(self):
    file_name = os.path.basename(self.log_file_path)
    find_number = re.findall(r"\d+", file_name)
    if len(find_number) == 0:
        return -1
    return int(find_number[0])