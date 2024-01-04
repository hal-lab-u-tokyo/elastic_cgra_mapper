from entity import *
from .cgra_json_reader import *
import parse

def database_log_reader(file_path, benchmark_list=[]):
  log_info = DatabaseLogInfo()
  log_info.log_file_path = file_path

  dir_list = file_path.split("/")

  for dir_name in dir_list:
    if dir_name in benchmark_list:
      log_info.benchmark = dir_name

  with open(file_path) as f:
    line_num = 1
    mapping_file_num = 1000000
    for line in f:
      if line_num == 3:
        cgra_file_path = parse.parse("cgra file: {}\n", line)[0]
        cgra = cgra_json_reader(cgra_file_path)
        log_info.row = cgra.row
        log_info.column = cgra.column
        log_info.context_size = cgra.context_size
        log_info.memory_io = cgra.memory_io_type
        log_info.cgra_type = cgra.cgra_type
        log_info.network_type = cgra.network_type
      elif line_num == 5:
        log_info.timeout = float(parse.parse("timeout (s): {}\n", line)[0])
      elif line_num == 6:
        log_info.min_utilization = parse.parse("min utilization: {:f}\n", line)[0]
      elif line_num >= 7:
        parsed = parse.parse("mapping_log_file:{}\n", line)
        if parsed is None:
          mapping_file_num = len(log_info.mapping_log_file_list)
        else:
          mapping_file_path = parsed[0]
          log_info.mapping_log_file_list.append(mapping_file_path)
          mapping_file_num = mapping_file_num + 1
      if line_num == mapping_file_num + 8:
        log_info.creating_time = parse.parse("creating db time (s): {:f}\n", line)[0]

      line_num = line_num + 1

  return log_info
