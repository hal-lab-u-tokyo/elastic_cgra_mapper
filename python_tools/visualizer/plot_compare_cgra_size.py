import sys
import os
sys.path.append(os.pardir)
from matplotlib import pyplot as plt
import networkx as nx
import re
from io_lib import *
from entity import *
from typing import List
from db_manager import *
import enum

def check_dir_availability(dir_name):
  if not os.path.exists(dir_name):
    os.makedirs(dir_name)

class MappingType(enum.Enum):
  database = 0
  dp = 1
  greedy = 2
  full_search = 3
  loop_unrolling = 4

class DataToPlot:
  def __init__(self, cgra_num):
    self.utilization = []
    self.time = []
    self.unix_time = []
    for i in range(5):
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
    check_dir_availability("./output/utilization_comparison/")
    label_list = ["remapping:database","remapping:dp", "remapping:greedy", "remapping: full_search" , "not remapping"]
    for benchmark in self.data_of_each_benchmark.keys():
      fig, ax = plt.subplots()
      for mapping_type in range(1,5):
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
      for mapping_type in range(5):
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

if __name__ == "__main__": 
  args = sys.argv
  config_path = args[1]

  plotter_config = load_plotter_config(config_path)

  mapping_info_list, remapping_info_list, database_info_list = load_result_from_csv("./output/csv/", plotter_config.get_benchmark_list())

  benchmark_node_num = {}
 
  db_manager = DatabaseManager(database_info_list)

  for benchmark in plotter_config.get_benchmark_list():
    dfg_file_path = plotter_config.kernel_dir_path + benchmark + ".dot"
    G = nx.Graph(nx.nx_pydot.read_dot(dfg_file_path))
    dfg_node_size = len(G.nodes())
    for node in G.nodes():
      if len(node) == 2: # NOTE: skip "\n" node
        dfg_node_size = dfg_node_size - 1

    benchmark_node_num[benchmark] = dfg_node_size

  memory_io_to_all_data_to_plot = {}
  memory_io_to_all_data_to_plot["all"] = AllDataToPlot(plotter_config.compare_cgra_size_config.min_size, plotter_config.compare_cgra_size_config.max_size)
  memory_io_to_all_data_to_plot["both_ends"] = AllDataToPlot(plotter_config.compare_cgra_size_config.min_size, plotter_config.compare_cgra_size_config.max_size)

  for mapping_info in mapping_info_list:
    row = mapping_info.row
    column = mapping_info.column
    context_size = mapping_info.context_size
    memory_io = mapping_info.memory_io
    cgra_type = mapping_info.cgra_type
    network_type = mapping_info.network_type
    mapping_succeed = mapping_info.mapping_succeed
    benchmark = mapping_info.benchmark

    if benchmark not in plotter_config.get_benchmark_list():
      continue 
    if row != column:
      continue
    if row < plotter_config.compare_cgra_size_config.min_size or plotter_config.compare_cgra_size_config.max_size < row:
      continue
    if column < plotter_config.compare_cgra_size_config.min_size or plotter_config.compare_cgra_size_config.max_size < column:
      continue
    if context_size != plotter_config.compare_benchmark_config.context_size:
      continue
    if network_type != plotter_config.compare_benchmark_config.network_type:
      continue
    if cgra_type != CGRAType.Elastic:
      continue
    if mapping_succeed == "0":
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

    if benchmark not in plotter_config.get_benchmark_list():
      continue 
    if row != column:
      continue
    if row < plotter_config.compare_cgra_size_config.min_size or plotter_config.compare_cgra_size_config.max_size < row:
      continue
    if column < plotter_config.compare_cgra_size_config.min_size or plotter_config.compare_cgra_size_config.max_size < column:
      continue
    if context_size != plotter_config.compare_benchmark_config.context_size:
      continue
    if network_type != plotter_config.compare_benchmark_config.network_type:
      continue
    if cgra_type != CGRAType.Elastic:
      continue

    all_context = row * column * context_size
    utilization = remapping_info.parallel_num * benchmark_node_num[benchmark]/ all_context

    database_info = db_manager.get_database_info(remapping_info)
    if database_info is None:
      continue
    if database_info.timeout != plotter_config.database_timeout:
      continue
    time = remapping_info.remapper_time + database_info.creating_time

    if remapping_info.remapper_mode == RemapperType.DP:
      memory_io_to_all_data_to_plot[memory_io.to_string()].add_benchmark_data(benchmark, MappingType.dp, row, utilization, time, remapping_info.get_unix_time())
    elif remapping_info.remapper_mode == RemapperType.Greedy:
      memory_io_to_all_data_to_plot[memory_io.to_string()].add_benchmark_data(benchmark, MappingType.greedy, row, utilization, time, remapping_info.get_unix_time())
    elif remapping_info.remapper_mode == RemapperType.FullSearch:
      memory_io_to_all_data_to_plot[memory_io.to_string()].add_benchmark_data(benchmark, MappingType.full_search, row, utilization, time, remapping_info.get_unix_time())
  
  for benchmark in plotter_config.get_benchmark_list():
    memory_io_to_all_data_to_plot["all"].plot("all")
    memory_io_to_all_data_to_plot["both_ends"].plot("both_ends")



    

    








