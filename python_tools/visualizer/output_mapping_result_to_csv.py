import sys
import os
sys.path.append(os.pardir)
import csv
import re
import datetime
from io_lib import *

JST = datetime.timezone(datetime.timedelta(hours=+9), 'JST')

def check_dir_availability(dir_name):
  if not os.path.exists(dir_name):
    os.makedirs(dir_name)

def get_ymd_from_unix(unixtime_str):
  return datetime.datetime.fromtimestamp(int(unixtime_str) / 1000000000, JST).strftime('%Y%m%d')

def get_hms_from_unix(unixtime_str):
  hour= datetime.datetime.fromtimestamp(int(unixtime_str) / 1000000000, JST).strftime('%H')
  min= datetime.datetime.fromtimestamp(int(unixtime_str) / 1000000000, JST).strftime('%M')
  sec= datetime.datetime.fromtimestamp(int(unixtime_str) / 1000000000, JST).strftime('%S')
  if len(hour) == 1:
    hour = "0" + hour
  if len(min) == 1:
    min = "0" + min
  if len(sec) == 1:
    sec = "0" + sec
  return hour + min + sec

def get_unix_time_from_file_path(file_path):
  file_name = os.path.basename(file_path)
  find_number = re.findall(r"\d+", file_name)
  if len(find_number) == 0:
    return -1
  return int(find_number[0])

if __name__ == "__main__":
  args = sys.argv
  config_path = args[1]

  plotter_config = load_plotter_config(config_path)

  check_dir_availability(plotter_config.visualizer_output_dir_path)

  csv_dir = os.path.join(plotter_config.visualizer_output_dir_path, "csv/")
  check_dir_availability(csv_dir)
  mapping_output_file = os.path.join(plotter_config.visualizer_output_dir_path, "csv/mapping_result.csv")
  remapping_output_file = os.path.join(plotter_config.visualizer_output_dir_path, "csv/remapping_result.csv")
  db_output_file = os.path.join(plotter_config.visualizer_output_dir_path, "csv/db_result.csv")

  file_exist = os.path.exists(mapping_output_file) and os.path.exists(remapping_output_file) and os.path.exists(db_output_file)

  mapping_log_info_list = []
  remapping_log_info_list = []
  database_log_info_list = []
  max_unix_time = 0

  if file_exist:
    # get max unix time
    csv_dir_path = os.path.join(plotter_config.visualizer_output_dir_path, "csv/")
    mapping_log_info_list, remapping_log_info_list, database_log_info_list =     load_result_from_csv(csv_dir_path, plotter_config.get_all_benchmark_list())
    for mapping_log_info in mapping_log_info_list:
      if max_unix_time < mapping_log_info.get_unix_time():
        max_unix_time = mapping_log_info.get_unix_time()

    for remapping_log_info in remapping_log_info_list:
      if max_unix_time < remapping_log_info.get_unix_time():
        max_unix_time = remapping_log_info.get_unix_time()

    for db_log_info in database_log_info_list:
      if max_unix_time < db_log_info.get_unix_time():
        max_unix_time = db_log_info.get_unix_time()

  # create unix_time to file_path dictionary
  for benchmark in plotter_config.get_all_benchmark_list():
    mapping_log_dir = os.path.join(plotter_config.output_dir_path, benchmark, "mapping/log")
    remapper_dir = os.path.join(plotter_config.output_dir_path, benchmark, "remapping")
    database_log_dir = os.path.join(plotter_config.output_dir_path, benchmark, "database/log")

    for file in os.listdir(mapping_log_dir):
      if file.endswith(".log"):
        file_path = os.path.join(mapping_log_dir, file)
        unix_time = get_unix_time_from_file_path(file_path)
        if unix_time > max_unix_time:
          success, mapping_log_info = mapping_log_reader(file_path)
          if success:
            mapping_log_info_list.append(mapping_log_info)

    for mode in ["full_search", "greedy", "dp"]:
      remapper_mode_dir = os.path.join(remapper_dir, mode, "log")
      if not os.path.exists(remapper_mode_dir):
        continue
      for file in os.listdir(remapper_mode_dir):
        if file.endswith(".log"):
          file_path = os.path.join(remapper_mode_dir, file)
          unix_time = get_unix_time_from_file_path(file_path)
          if unix_time > max_unix_time:
            success, remapping_log_info = remapping_log_reader(file_path)
            if success:
              remapping_log_info_list.append(remapping_log_info)
    
    for file in os.listdir(database_log_dir):
      if file.endswith(".log"):
        file_path = os.path.join(database_log_dir, file)
        unix_time = get_unix_time_from_file_path(file_path)
        if unix_time > max_unix_time:
          db_log_info = database_log_reader(file_path)
          database_log_info_list.append(db_log_info)              

  # output to csv 
  mapping_csv_file = open(mapping_output_file, "w")
  mapping_writer = csv.writer(mapping_csv_file)
  mapping_writer.writerow(["log_file_path", "date", "time", "row", "column", "context_size", "memory_io", "cgra_type", "network_type", "local_reg_size" "mapping_succeed", "mapping_time", "num_threads", "timeout", "parallel_num", "mapping_file"])
  for log_info in mapping_log_info_list:
    mapping_writer.writerow([log_info.log_file_path, get_ymd_from_unix(unix_time), get_hms_from_unix(unix_time), log_info.row, log_info.column, log_info.context_size, log_info.memory_io.to_string(), log_info.cgra_type.to_string(), log_info.network_type.to_string(), log_info.local_reg_size,log_info.mapping_succeed, log_info.mapping_time, log_info.num_threads, log_info.timeout, log_info.parallel_num, log_info.mapping_file_path])
  mapping_csv_file.close()
  
  remapping_csv_file = open(remapping_output_file, "w")
  remapping_writer = csv.writer(remapping_csv_file)
  remapping_writer.writerow(["log_file_path", "date", "time", "row", "column", "context_size", "memory_io", "cgra_type", "network_type", "local_reg_size", "parallel_num", "remapper_mode", "remapper_time", "mapping_json_list"])
  for log_info in remapping_log_info_list:
    remapping_writer.writerow([log_info.log_file_path, get_ymd_from_unix(unix_time), get_hms_from_unix(unix_time), log_info.row, log_info.column, log_info.context_size, log_info.memory_io.to_string(), log_info.cgra_type.to_string(), log_info.network_type.to_string(), log_info.local_reg_size, log_info.parallel_num, log_info.remapper_mode.to_string(), log_info.remapper_time, log_info.mapping_json_list])
  remapping_csv_file.close()

  database_csv_file = open(db_output_file, "w")
  database_writer = csv.writer(database_csv_file)
  database_writer.writerow(["log_file_path", "date", "time", "row", "column", "context_size", "memory_io", "cgra_type", "network_type", "local_reg_size", "timeout", "creating_time", "min_utilization", "mapping_log_file_list"])
  for log_info in database_log_info_list:
    database_writer.writerow([log_info.log_file_path, get_ymd_from_unix(unix_time), get_hms_from_unix(unix_time), log_info.row, log_info.column, log_info.context_size, log_info.memory_io.to_string(), log_info.cgra_type.to_string(), log_info.network_type.to_string(), log_info.local_reg_size, log_info.timeout, log_info.creating_time, log_info.min_utilization, log_info.mapping_log_file_list])
  database_csv_file.close()



    
