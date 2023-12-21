import csv
import os
import re
import datetime
from mapping_log_reader import *
from remapping_log_reader import remapping_log_reader
from load_result_from_csv import *
from load_remapper_config import *

JST = datetime.timezone(datetime.timedelta(hours=+9), 'JST')

def is_mapping_log_file(file_path):
  with open(file_path) as f:
    for line in f:
      if line == "-- CGRA setting --\n":
        return True
  return False

def check_dir_availability(dir_name):
  if not os.path.exists(dir_name):
    os.makedirs(dir_name)

def get_ymd_from_unix(unixtime_str):
  return datetime.datetime.fromtimestamp(int(unixtime_str), JST).strftime('%Y%m%d')

def get_hms_from_unix(unixtime_str):
  hour= datetime.datetime.fromtimestamp(int(unixtime_str), JST).strftime('%H')
  min= datetime.datetime.fromtimestamp(int(unixtime_str), JST).strftime('%M')
  sec= datetime.datetime.fromtimestamp(int(unixtime_str), JST).strftime('%S')
  if len(hour) == 1:
    hour = "0" + hour
  if len(min) == 1:
    min = "0" + min
  if len(sec) == 1:
    sec = "0" + sec
  return hour + min + sec

if __name__ == "__main__":
  args = sys.argv
  config_path = args[1]

  remapper_config = load_remapper_config(config_path)

  output_dir_path = "./output/csv/"
  log_dir = "../output/"

  check_dir_availability(output_dir_path)

  mapping_output_file = output_dir_path + "mapping_result.csv"
  remapping_output_file = output_dir_path + "remapping_result.csv"

  file_exist = os.path.exists(mapping_output_file) and os.path.exists(remapping_output_file)

  mapping_log_info_list = []
  remapping_log_info_list = []
  max_unix_time = 0
  if file_exist:
    # get max unix time
    mapping_log_info_list, remapping_log_info_list = load_result_from_csv(output_dir_path, remapper_config.get_all_benchmark_list())
    for mapping_log_info in mapping_log_info_list:
      if max_unix_time < mapping_log_info.get_unix_time():
        max_unix_time = mapping_log_info.get_unix_time()

    for remapping_log_info in remapping_log_info_list:
      if max_unix_time < remapping_log_info.get_unix_time():
        max_unix_time = remapping_log_info.get_unix_time()
    max_unix_time = max_unix_time


  # id =  dir (without log or mapping) , unixtime
  id_to_log_file_path = {}
  id_to_mapping_file_path = {}

  # create unix_time to file_path dictionary
  for path_name, dir_names, file_names in os.walk(log_dir):
    if len(file_names) == 0:
      continue
    for file_name in file_names:
      if "log" in file_name:
        file_path = path_name + "/" + file_name
        find_number = re.findall(r"\d+", file_name)
        if len(find_number) == 0:
          continue
        unix_time = int(find_number[0])
        id = path_name.replace("log", "")
        if unix_time > max_unix_time:
          id_to_log_file_path[(id, unix_time)] = file_path
      if "mapping" in file_name:
        file_path = path_name + "/" + file_name
        find_number = re.findall(r"\d+", file_name)
        if len(find_number) == 0:
          continue
        unix_time = int(find_number[0])
        id = path_name.replace("mapping", "")
        if unix_time > max_unix_time:
          id_to_mapping_file_path[(id, unix_time)] = file_path

  # summarize mapping and remapping result
  mapping_input_str_to_unix_time_and_info = {}
  remapping_input_str_to_unix_time_and_info = {}

  for mapping_log_info in mapping_log_info_list:
    mapping_input_str_to_unix_time_and_info[mapping_log_info.get_input_as_str()] = (mapping_log_info.get_unix_time(), mapping_log_info)
  
  for remapping_log_info in remapping_log_info_list:
    remapping_input_str_to_unix_time_and_info[remapping_log_info.get_input_as_str()] = (remapping_log_info.get_unix_time(), remapping_log_info)

  for id in id_to_log_file_path.keys():
    unix_time = id[1]
    log_file_path = id_to_log_file_path[id]
    if is_mapping_log_file(log_file_path):
      mapping_log_info = mapping_log_reader(log_file_path, remapper_config.get_all_benchmark_list())
      input_str = mapping_log_info.get_input_as_str()
      if id in id_to_mapping_file_path.keys():
        mapping_file_path = id_to_mapping_file_path[id]
        try: 
          read_mapping_from_json(mapping_file_path)
        except AssertionError as e:
          continue


      if input_str in mapping_input_str_to_unix_time_and_info.keys() and mapping_input_str_to_unix_time_and_info[input_str][0] > unix_time:
        continue
      mapping_input_str_to_unix_time_and_info[input_str] = (unix_time, mapping_log_info)
    else:
      if id in id_to_mapping_file_path.keys():
        mapping_file_path = id_to_mapping_file_path[id]
      else:
        continue
      
      try:
        remapping_log_info = remapping_log_reader(log_file_path, mapping_file_path, remapper_config.get_all_benchmark_list())
      except ValueError as e:
        continue
      input_str = remapping_log_info.get_input_as_str()
      if input_str in remapping_input_str_to_unix_time_and_info.keys() and remapping_input_str_to_unix_time_and_info[input_str][0] > unix_time:
        continue
      remapping_input_str_to_unix_time_and_info[input_str] = (unix_time, remapping_log_info)

  # output to csv 
  mapping_csv_file = open(mapping_output_file, "w")
  mapping_writer = csv.writer(mapping_csv_file)
  mapping_writer.writerow(["log_file_path", "date", "time", "row", "column", "context_size", "memory_io", "cgra_type", "network_type", "mapping_succeed", "mapping_time", "num_threads", "timeout", "parallel_num"])
  for unix_time_and_info in mapping_input_str_to_unix_time_and_info.values():
    unix_time = unix_time_and_info[0]
    log_info = unix_time_and_info[1]
    mapping_writer.writerow([log_info.log_file_path, get_ymd_from_unix(unix_time), get_hms_from_unix(unix_time), log_info.row, log_info.column, log_info.context_size, log_info.memory_io.to_string(), log_info.cgra_type.to_string(), log_info.network_type.to_string(), log_info.mapping_succeed, log_info.mapping_time, log_info.num_threads, log_info.timeout, log_info.parallel_num])
  
  remapping_csv_file = open(remapping_output_file, "w")
  remapping_writer = csv.writer(remapping_csv_file)
  remapping_writer.writerow(["log_file_path", "date", "time", "row", "column", "context_size", "memory_io", "cgra_type", "network_type", "parallel_num", "remapper_mode", "remapper_time"])
  for unix_time_and_info in remapping_input_str_to_unix_time_and_info.values():
    unix_time = unix_time_and_info[0]
    log_info = unix_time_and_info[1]
    remapping_writer.writerow([log_info.log_file_path, get_ymd_from_unix(unix_time), get_hms_from_unix(unix_time), log_info.row, log_info.column, log_info.context_size, log_info.memory_io.to_string(), log_info.cgra_type.to_string(), log_info.network_type.to_string(), log_info.parallel_num, log_info.remapper_mode.to_string(), log_info.remapper_time])


  mapping_csv_file.close()
  remapping_csv_file.close()



    
