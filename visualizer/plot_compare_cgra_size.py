from matplotlib import pyplot as plt
import networkx as nx
import os
import re
from io_lib import *
from typing import List
import enum

class MappingType(enum.Enum):
  dp = 0
  greedy = 1
  full_search = 2
  loop_unrolling = 3

class DataToPlot:
  def __init__(self, cgra_num):
    self.utilization = []
    self.time = []
    self.unix_time = []
    for i in range(4):
      self.utilization.append([])
      self.time.append([])
      self.unix_time.append([])
      for _ in range(0, cgra_num):
        self.utilization[i].append(-1)
        self.time[i].append(-1)
        self.unix_time[i].append(-1)

class AllDataToPlot:
  def __init__(self, min_cgra_size, max_cgra_size):
    self.data_of_each_benchmark: dict = {}
    self.min_cgra_size = min_cgra_size
    self.max_cgra_size = max_cgra_size

  def add_benchmark_data(self, benchmark_name, mapping_type, cgra_size, utilization, time, unix_time):
    cgra_size_idx = cgra_size - self.min_cgra_size
    if benchmark_name not in self.data_of_each_benchmark.keys():
      self.data_of_each_benchmark[benchmark_name] = DataToPlot(self.max_cgra_size - self.min_cgra_size + 1)
    if utilization < self.data_of_each_benchmark[benchmark_name].utilization[mapping_type.value][cgra_size_idx]:
      return
    if unix_time < self.data_of_each_benchmark[benchmark_name].unix_time[mapping_type.value][cgra_size_idx]:
      return
    self.data_of_each_benchmark[benchmark_name].utilization[mapping_type.value][cgra_size_idx] = utilization
    self.data_of_each_benchmark[benchmark_name].time[mapping_type.value][cgra_size_idx] = time
    self.data_of_each_benchmark[benchmark_name].unix_time[mapping_type.value][cgra_size_idx] = unix_time

  def plot(self, image_name):
    label_list = ["remapping:dp", "remapping:greedy", "remapping: full_search" , "not remapping"]
    for benchmark in self.data_of_each_benchmark.keys():
      fig, ax = plt.subplots()
      for mapping_type in range(4):
        util_list = []
        cgra_size_list = []
        for i in range(self.max_cgra_size - self.min_cgra_size + 1):
          util = self.data_of_each_benchmark[benchmark].utilization[mapping_type][i]
          if util != -1:
            util_list.append(util)
            cgra_size_list.append(i + self.min_cgra_size)
        ax.plot(cgra_size_list, util_list, marker=".", label=label_list[mapping_type])
      ax.set_xlabel("cgra size")
      ax.set_ylabel("utilization")
      ax.legend()

      fig.savefig("./output/utilization_comparison/" + image_name + "_"+ benchmark + "_util.png")

      fig, ax = plt.subplots()
      for mapping_type in range(4):
        time_list = []
        cgra_size_list = []
        for i in range(self.max_cgra_size - self.min_cgra_size + 1):
          time = self.data_of_each_benchmark[benchmark].time[mapping_type][i]
          if time != -1:
            time_list.append(time)
            cgra_size_list.append(i + self.min_cgra_size)
        ax.plot(cgra_size_list, time_list, marker=".", label=label_list[mapping_type])
      ax.set_xlabel("cgra size")
      ax.set_ylabel("time")
      ax.legend()

      fig.savefig("./output/utilization_comparison/" + image_name + "_" + benchmark + "_time.png")

class DatabaseManager:
  def __init__(self):
    self.remapper_database_dir_to_time = {}

  def get_database_time(self, remapper_log_file_path, remapper_mode):
    result = 0

    remapper_mode_dir_name = "dp"
    if remapper_mode == RemapperType.DP:
      remapper_mode_dir_name = "dp"
    elif remapper_mode == RemapperType.Greedy:
      remapper_mode_dir_name = "naive"
    elif remapper_mode == RemapperType.FullSearch:
      remapper_mode_dir_name = "full_search"

    benchmark_database_dir_path = os.path.dirname(remapper_log_file_path).replace(remapper_mode_dir_name, "database") + "/"

    if benchmark_database_dir_path in self.remapper_database_dir_to_time.keys():
      return self.remapper_database_dir_to_time[benchmark_database_dir_path]

    if not os.path.exists(benchmark_database_dir_path):
      return -100000

    for file in os.listdir(benchmark_database_dir_path):
      log_file_path = benchmark_database_dir_path + file
      mapping_log = mapping_log_reader(log_file_path, remapper_config.get_benchmark_list())
      if benchmark_database_dir_path not in self.remapper_database_dir_to_time.keys():
        self.remapper_database_dir_to_time[benchmark_database_dir_path] = 0
        result = result + mapping_log.mapping_time

    self.remapper_database_dir_to_time[benchmark_database_dir_path] = result

    return result

if __name__ == "__main__": 
  args = sys.argv
  config_path = args[1]

  remapper_config = load_remapper_config(config_path)

  mapping_info_list, remapping_info_list = load_result_from_csv("./output/csv/", remapper_config.get_benchmark_list())

  benchmark_node_num = {}
 
  db_manager = DatabaseManager()

  for benchmark in remapper_config.get_benchmark_list():

    dfg_file_path = remapper_config.kernel_dir_path + benchmark + ".dot"
    G = nx.Graph(nx.nx_pydot.read_dot(dfg_file_path))
    dfg_node_size = len(G.nodes())

    benchmark_node_num[benchmark] = dfg_node_size

  memory_io_to_all_data_to_plot = {}
  memory_io_to_all_data_to_plot["all"] = AllDataToPlot(remapper_config.compare_cgra_size_config.min_size, remapper_config.compare_cgra_size_config.max_size)
  memory_io_to_all_data_to_plot["both_ends"] = AllDataToPlot(remapper_config.compare_cgra_size_config.min_size, remapper_config.compare_cgra_size_config.max_size)

  for mapping_info in mapping_info_list:
    row = mapping_info.row
    column = mapping_info.column
    context_size = mapping_info.context_size
    memory_io = mapping_info.memory_io
    cgra_type = mapping_info.cgra_type
    network_type = mapping_info.network_type
    mapping_succeed = mapping_info.mapping_succeed
    benchmark = mapping_info.benchmark

    if benchmark not in remapper_config.get_benchmark_list():
      continue 
    if row != column:
      continue
    if row < remapper_config.compare_cgra_size_config.min_size or remapper_config.compare_cgra_size_config.max_size < row:
      continue
    if column < remapper_config.compare_cgra_size_config.min_size or remapper_config.compare_cgra_size_config.max_size < column:
      continue
    if context_size != remapper_config.compare_benchmark_config.context_size:
      continue
    if network_type != remapper_config.compare_benchmark_config.network_type:
      continue
    if cgra_type != CGRAType.Elastic:
      continue
    if mapping_succeed == False:
      continue
    
    all_context = row * column * context_size
    utilization = mapping_info.parallel_num * benchmark_node_num[benchmark]/ all_context

    memory_io_to_all_data_to_plot[memory_io.to_string()].add_benchmark_data(benchmark, MappingType.loop_unrolling, row, utilization, mapping_info.mapping_time, mapping_info.get_unix_time())

  for remapping_info in remapping_info_list:
    row = remapping_info.row
    column = remapping_info.column
    context_size = remapping_info.context_size
    memory_io = remapping_info.memory_io
    cgra_type = remapping_info.cgra_type
    network_type = remapping_info.network_type
    benchmark = remapping_info.benchmark

    if benchmark not in remapper_config.get_benchmark_list():
      continue 
    if row != column:
      continue
    if row < remapper_config.compare_cgra_size_config.min_size or remapper_config.compare_cgra_size_config.max_size < row:
      continue
    if column < remapper_config.compare_cgra_size_config.min_size or remapper_config.compare_cgra_size_config.max_size < column:
      continue
    if context_size != remapper_config.compare_benchmark_config.context_size:
      continue
    if network_type != remapper_config.compare_benchmark_config.network_type:
      continue
    if cgra_type != CGRAType.Elastic:
      continue

    all_context = row * column * context_size
    utilization = remapping_info.parallel_num * benchmark_node_num[benchmark]/ all_context

    if benchmark not in database_time.keys():
      database_time[benchmark] = get_database_time(remapping_info.log_file_path, remapping_info.remapper_mode)

    database_time = db_manager.get_database_time(remapping_info.log_file_path, remapping_info.remapper_mode)
    time = remapping_info.remapper_time + database_time

    if remapping_info.remapper_mode == RemapperType.DP:
      memory_io_to_all_data_to_plot[memory_io.to_string()].add_benchmark_data(benchmark, MappingType.dp, row, utilization, time, remapping_info.get_unix_time())
    elif remapping_info.remapper_mode == RemapperType.Greedy:
      memory_io_to_all_data_to_plot[memory_io.to_string()].add_benchmark_data(benchmark, MappingType.greedy, row, utilization, time, remapping_info.get_unix_time())
    elif remapping_info.remapper_mode == RemapperType.FullSearch:
      memory_io_to_all_data_to_plot[memory_io.to_string()].add_benchmark_data(benchmark, MappingType.full_search, row, utilization, time, remapping_info.get_unix_time())
  
  for benchmark in remapper_config.get_benchmark_list():
    memory_io_to_all_data_to_plot["all"].plot("all")
    memory_io_to_all_data_to_plot["both_ends"].plot("both_ends")



    

    








