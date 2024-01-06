import csv
from .mapping_log_reader import *
from .remapping_log_reader import *

def load_result_from_csv(csv_dir_path, benchmark_list):
  mapping_csv_file_path = csv_dir_path + "mapping_result.csv"
  remapping_csv_file_path = csv_dir_path + "remapping_result.csv" 
  db_csv_file_path = csv_dir_path + "db_result.csv"  

  mapping_log_info_list = []
  remapping_log_info_list = []
  database_log_info_list = []

  with open(mapping_csv_file_path) as f:
    reader = csv.reader(f)
    header = next(reader)
    for row in reader:
      dir_list = row[0].split("/")
      benchmark = ""
      for dir_name in dir_list:
        if dir_name in benchmark_list:
          benchmark = dir_name

      if benchmark == "":
        continue

      mapping_log_info = MappingLogInfo()
      mapping_log_info.log_file_path = row[0]
      mapping_log_info.benchmark = benchmark
      mapping_log_info.row = int(row[3])
      mapping_log_info.column = int(row[4])
      mapping_log_info.context_size = int(row[5])
      mapping_log_info.memory_io = MemoryIOType.get_from_string(row[6])
      mapping_log_info.cgra_type = CGRAType.get_from_string(row[7])
      mapping_log_info.network_type = NetworkType.get_from_string(row[8])
      mapping_log_info.local_reg_size = int(row[9])
      mapping_log_info.mapping_succeed = int(row[10])
      mapping_log_info.mapping_time = float(row[11])
      mapping_log_info.num_threads = int(row[12])
      mapping_log_info.timeout = float(row[13]) 
      mapping_log_info.parallel_num = int(row[14])
      mapping_log_info.mapping_file_path = row[15]
      mapping_log_info_list.append(mapping_log_info)

  
  with open(remapping_csv_file_path) as f:
    reader = csv.reader(f)
    header = next(reader)
    for row in reader:
      dir_list = row[0].split("/")
      benchmark = ""
      for dir_name in dir_list:
        if dir_name in benchmark_list:
          benchmark = dir_name

      remapping_log_info = RemapperLogInfo()
      remapping_log_info.log_file_path = row[0]
      remapping_log_info.benchmark = benchmark
      remapping_log_info.row = int(row[3])
      remapping_log_info.column = int(row[4])
      remapping_log_info.context_size = int(row[5])
      remapping_log_info.memory_io = MemoryIOType.get_from_string(row[6])
      remapping_log_info.cgra_type = CGRAType.get_from_string(row[7])
      remapping_log_info.network_type = NetworkType.get_from_string(row[8])      
      remapping_log_info.local_reg_size = int(row[9])
      remapping_log_info.parallel_num = int(row[10])
      if remapping_log_info.parallel_num > 0:
        remapping_log_info.mapping_succeed = True
      remapping_log_info.remapper_mode = RemapperType.from_string(row[11])
      remapping_log_info.remapper_time = float(row[12])
      remapping_log_info.mapping_json_list = row[13].strip("[]").replace("'", "").split(", ")
      remapping_log_info_list.append(remapping_log_info)

  with open(db_csv_file_path) as f:
    reader = csv.reader(f)
    header = next(reader)
    for row in reader:
      dir_list = row[0].split("/")
      benchmark = ""
      for dir_name in dir_list:
        if dir_name in benchmark_list:
          benchmark = dir_name

      db_log_info = DatabaseLogInfo()
      db_log_info.log_file_path = row[0]
      db_log_info.benchmark = benchmark
      db_log_info.row = int(row[3])
      db_log_info.column = int(row[4])
      db_log_info.context_size = int(row[5])
      db_log_info.memory_io = MemoryIOType.get_from_string(row[6])
      db_log_info.cgra_type = CGRAType.get_from_string(row[7])
      db_log_info.network_type = NetworkType.get_from_string(row[8])
      db_log_info.local_reg_size = int(row[9])
      db_log_info.timeout = float(row[10])
      db_log_info.creating_time = float(row[11])
      db_log_info.min_utilization = float(row[12])
      db_log_info.mapping_log_file_list = row[13].strip("[]").replace("'", "").split(", ")
      if db_log_info.mapping_log_file_list == [""]:
        db_log_info.mapping_log_file_list = []
      
      database_log_info_list.append(db_log_info)

  return mapping_log_info_list, remapping_log_info_list, database_log_info_list


