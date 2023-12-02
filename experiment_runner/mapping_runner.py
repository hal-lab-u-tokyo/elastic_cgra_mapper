import subprocess
import time
import json
import os
import datetime
import networkx as nx
import sys
import multiprocessing

global experiment_date
global experiment_log_file_path
global lock
global timeout_s

def get_date_string():
  dt = datetime.datetime.now()
  year = str(dt.year)
  month = str(dt.month)
  if len(month) == 1:
    month = "0" + month
  day = str(dt.day)
  if len(day) == 1:
    day = "0" + day

  return year + month + day

def get_time_string():
  dt = datetime.datetime.now()
  hour = str(dt.hour)
  if len(hour) == 1:
    hour = "0" + hour
  minute = str(dt.minute)
  if len(minute) == 1:
    minute = "0" + minute

  return get_date_string() + hour + minute

def check_dir_availability(dir_name):
  if not os.path.exists(dir_name):
    os.makedirs(dir_name)

def succeed_mapping(log_file):
    with open(log_file) as f:
      for line in f:
        if line == "Model is infeasible\n":
          return False

        if line.find("Optimal solution found(tolerance") != -1:
          return True

def exec(command):
  dfg_file = command[0]
  cgra_dict = command[1]
  benchmark = command[2]

  G = nx.Graph(nx.nx_pydot.read_dot(dfg_file))
  dfg_node_size = len(G.nodes())

  mapping_dir_path = "../output/mapping/"  + benchmark + "/" + experiment_date + "/"
  mapping_log_dir_path = "../output/log/" + benchmark + "/" + experiment_date + "/"
  cgra_dir_path = "./tmp_cgra/"

  lock.acquire()
  try:
    check_dir_availability(mapping_dir_path)
    check_dir_availability(mapping_log_dir_path)
    check_dir_availability(cgra_dir_path)
    cgra_file = cgra_dir_path + str(int(time.time())) + ".json"
    time.sleep(1)
  finally:
    lock.release()


  file = open(cgra_file, mode="w")
  json.dump(cgra_dict, file)
  file.close()

  parallel_num = int(int(cgra_dict["row"]) * int(cgra_dict["column"]) * int(cgra_dict["context_size"]) / dfg_node_size)

  while 1:
    lock.acquire()
    try:
      unixtime_str = str(int(time.time()))
      time.sleep(1)
    finally:
      lock.release()
    mapping_file =mapping_dir_path + "mapping_" + unixtime_str + ".json"
    mapping_log_file = mapping_log_dir_path + "log_" + unixtime_str + ".log"

    lock.acquire()
    try:
      experiment_log_file = open(experiment_log_file_path, "a")
      experiment_log_file.write("--- new exec ---\n")
      experiment_log_file.write("exec : ../build/mapping\n")
      experiment_log_file.write("dfg_file: " + dfg_file + "\n")
      experiment_log_file.write("mapping_file: " + mapping_file + "\n")
      experiment_log_file.write("mapping_log_file: " + mapping_log_file + "\n")
      experiment_log_file.close()
    finally:
      lock.release()

    subprocess.run(["../build/mapping", dfg_file, cgra_file, mapping_file, mapping_log_file, str(timeout_s), str(parallel_num)])

    
    is_success = succeed_mapping(mapping_log_file)

    if is_success:
      break
    else:
      parallel_num = parallel_num - 1
  
  os.remove(cgra_file)

if __name__ == "__main__":
  args = sys.argv
  process_num = int(args[1])
  
  benchmark_list = ["fixed_convolution2d", "fixed_ellpack", "fixed_fft_pro", "fixed_fir_pro", "fixed_latnrm_pro", "fixed_stencil", "fixed_susan_pro"]
  cgra_size_list = range(6,21)
  memory_io_list = ["all", "both_ends"]
  cgra_type_list = ["elastic", "default"]
  network_type = "orthogonal"
  local_reg_size = 1
  context_size = 4
  timeout_s = 3600

  experiment_log_dir = "./log/mapping_runner/"
  check_dir_availability(experiment_log_dir)
  experiment_log_file_path = experiment_log_dir + "log_" + get_time_string() + ".log"
  
  experiment_date = get_date_string()

  command_list = []

  for cgra_type in cgra_type_list:
    for cgra_size in cgra_size_list:
      for memory_io in memory_io_list:
        for benchmark in benchmark_list:
          dfg_file_path = "../benchmark/kernel/" + benchmark + ".dot"

          cgra_dict = {} 
          cgra_dict["row"] = str(cgra_size)
          cgra_dict["column"] = str(cgra_size)
          cgra_dict["memory_io"] = memory_io
          cgra_dict["CGRA_type"] = cgra_type
          cgra_dict["network_type"] = network_type
          cgra_dict["local_reg_size"] = local_reg_size
          cgra_dict["context_size"] = context_size

          command_list.append([dfg_file_path, cgra_dict, benchmark])
  
  lock = multiprocessing.Lock()
  pool = multiprocessing.Pool(process_num)
  pool.map(exec, command_list)






