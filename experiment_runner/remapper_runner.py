import datetime
import multiprocessing
import os
import time
import subprocess
import json
import sys
import networkx as nx

global experiment_result_dir
global db_timeout_s
global build_dir_name

def check_dir_availability(dir_name):
  if not os.path.exists(dir_name):
    os.makedirs(dir_name)

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

def succeed_mapping(log_file):
    with open(log_file) as f:
      for line in f:
        if line == "Model is infeasible\n":
          return False

        if line.find("Optimal solution found(tolerance") != -1:
          return True

def create_db(command):
  dfg_file = command[0]
  benchmark = command[1]

  mapping_dir_path = experiment_result_dir + benchmark + "/database/mapping/" 
  mapping_log_dir_path = experiment_result_dir + benchmark + "/database/log/" 

  lock.acquire()
  try:
    check_dir_availability(mapping_dir_path)
    check_dir_availability(mapping_log_dir_path)
  finally:
    lock.release()
  
  lock.acquire()
  try:
    experiment_log_file = open(experiment_log_file_path, "a")
    experiment_log_file.write("--- create database ---\n")
    experiment_log_file.write("exec : ../" + build_dir_name+ "/bulk_mapping\n")
    experiment_log_file.write("dfg_file: " + dfg_file + "\n")
    experiment_log_file.write("mapping_dir: " + mapping_dir_path + "\n")
    experiment_log_file.write("mapping_log_dir: " + mapping_log_dir_path + "\n")
    experiment_log_file.close()
  finally:
    lock.release()

  subprocess.run(["../"+ build_dir_name +"/bulk_mapping", dfg_file, mapping_dir_path, mapping_log_dir_path, str(db_timeout_s)])

def loop_unrolling(command):
  dfg_file = command[0]
  cgra_dict = command[1]
  benchmark = command[2]

  G = nx.Graph(nx.nx_pydot.read_dot(dfg_file))
  dfg_node_size = len(G.nodes())

  mapping_dir_path = experiment_result_dir + benchmark + "/loop_unrolling/mapping/"
  mapping_log_dir_path = experiment_result_dir + benchmark + "/loop_unrolling/log/"
  cgra_dir_path = experiment_result_dir + benchmark + "/loop_unrolling/tmp_cgra/"

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
    mapping_file = mapping_dir_path + "mapping_" + unixtime_str + ".json"
    mapping_log_file = mapping_log_dir_path + "log_" + unixtime_str + ".log"

    lock.acquire()
    try:
      experiment_log_file = open(experiment_log_file_path, "a")
      experiment_log_file.write("--- unrolling exec ---\n")
      experiment_log_file.write("exec : ../"+build_dir_name+"/mapping\n")
      experiment_log_file.write("dfg_file: " + dfg_file + "\n")
      experiment_log_file.write("mapping_file: " + mapping_file + "\n")
      experiment_log_file.write("mapping_log_file: " + mapping_log_file + "\n")
      experiment_log_file.close()
    finally:
      lock.release()

    subprocess.run(["../"+build_dir_name+ "/mapping", dfg_file, cgra_file, mapping_file, mapping_log_file, str(timeout_s), str(parallel_num)])

    is_success = succeed_mapping(mapping_log_file)

    if is_success:
      break
    else:
      parallel_num = parallel_num - 1

    if parallel_num == 0:
      break
  
  os.remove(cgra_file)

def remapper(command):
  dfg_file = command[0]
  cgra_dict = command[1]
  benchmark = command[2]
  mode = command[3]

  database_dir_path = experiment_result_dir + benchmark + "/database/mapping/" 
  mapping_dir_path = experiment_result_dir + benchmark + "/" + mode + "/mapping/"
  mapping_log_dir_path = experiment_result_dir + benchmark + "/" + mode + "/log/"
  cgra_dir_path = experiment_result_dir + benchmark + "/" + mode + "/tmp_cgra/"

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

  lock.acquire()
  try:
    experiment_log_file = open(experiment_log_file_path, "a")
    experiment_log_file.write("--- remapper exec ---\n")
    experiment_log_file.write("exec : ../"+ build_dir_name +"/remapping\n")
    experiment_log_file.write("mapping_dir_path: " + database_dir_path + "\n")
    experiment_log_file.write("output_mapping_dir: " + mapping_dir_path + "\n")
    experiment_log_file.write("output_log_dir: " + mapping_log_dir_path + "\n")
    experiment_log_file.close()
  finally:
    lock.release()

  mode_input = "0"
  if mode == "naive":
    mode_input = "1"
  elif mode == "dp":
    mode_input = "2"

  subprocess.run(["../"+ build_dir_name + "/remapping", database_dir_path, cgra_file, mapping_dir_path, mapping_log_dir_path, mode_input])

  os.remove(cgra_file)

def exec(command):
  mode = command[3]

  if mode == "loop_unrolling":
    loop_unrolling(command)
  else:
    remapper(command)


  
if __name__ == "__main__":
  args = sys.argv
  process_num = int(args[1])

  benchmark_list = ["fixed_convolution2d", "fixed_ellpack", "fixed_fft_pro", "fixed_fir_pro", "fixed_latnrm_pro", "fixed_stencil", "fixed_susan_pro", "convolution_no_loop", "fixed_matrixmultiply_const"]
  cgra_size_list = range(6,21)
  memory_io_list = ["all", "both_ends"]
  cgra_type = "elastic"
  network_type = "orthogonal"
  local_reg_size = 1
  context_size = 4
  db_timeout_s = 900
  timeout_s = 3600
  build_dir_name = "build"

  experiment_log_dir = "./log/remapper_runner/"
  check_dir_availability(experiment_log_dir)
  experiment_log_file_path = experiment_log_dir + "log_" + get_time_string() + ".log"
  
  experiment_date = get_date_string()

  command_list = []

  experiment_result_dir = "../output/remapper/" + experiment_date + "/"
  check_dir_availability(experiment_log_dir)

  mode_list = ["loop_unrolling", "full_search", "naive", "dp"]


  # create database
  for benchmark in benchmark_list:
    dfg_file_path = "../benchmark/kernel/" + benchmark + ".dot"

    command_list.append([dfg_file_path, benchmark])

  lock = multiprocessing.Lock()
  pool = multiprocessing.Pool(process_num)
  pool.map(create_db, command_list)

  command_list = []

  # multi mapping
  for cgra_size in cgra_size_list:
    for memory_io in memory_io_list:
      for benchmark in benchmark_list:
        for mode in mode_list:
          dfg_file_path = "../benchmark/kernel/" + benchmark + ".dot"

          cgra_dict = {} 
          cgra_dict["row"] = str(cgra_size)
          cgra_dict["column"] = str(cgra_size)
          cgra_dict["memory_io"] = memory_io
          cgra_dict["CGRA_type"] = cgra_type
          cgra_dict["network_type"] = network_type
          cgra_dict["local_reg_size"] = local_reg_size
          cgra_dict["context_size"] = context_size

          command_list.append([dfg_file_path, cgra_dict, benchmark, mode])

  pool.map(exec, command_list)






