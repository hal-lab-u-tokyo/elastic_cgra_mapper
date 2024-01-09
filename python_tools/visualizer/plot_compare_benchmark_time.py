import sys
import os
sys.path.append(os.pardir)

from matplotlib import pyplot as plt
import networkx as nx
from typing import List
from io_lib import *
from db_manager import *
import re
import enum
import math

def check_dir_availability(dir_name):
  if not os.path.exists(dir_name):
    os.makedirs(dir_name)

class MappingType(enum.Enum):
  dp = 0
  greedy = 1
  full_search = 2
  loop_unrolling = 3

class DataToPlot:
  def __init__(self):
    self.remapping_time: List[float] = [0,0,0]
    self.unrolling_time: List[float] = [-1,-1,-1]
    self.remapping_unix_time: List[int] = [0,0,0]
    self.unrolling_unix_time: List[int] = [0,0,0]

class AllDataToPlot:
  def __init__(self):
    self.data_of_each_benchmark: dict = {}
    self.benchmark_to_parallel_num = {}

  def add_benchmark_data(self, benchmark_name, mapping_type, parallel_num, time, unix_time):
    if benchmark_name not in self.data_of_each_benchmark.keys():
      self.data_of_each_benchmark[benchmark_name] = DataToPlot()
      self.benchmark_to_parallel_num[benchmark_name] = [0,0,0]

    if mapping_type == MappingType.loop_unrolling:
      for i in range(3):
        if parallel_num != self.benchmark_to_parallel_num[benchmark_name][i]:
          continue
        if unix_time < self.data_of_each_benchmark[benchmark_name].unrolling_unix_time[i]:
          return
        
        self.data_of_each_benchmark[benchmark_name].unrolling_time[i] = time
        self.data_of_each_benchmark[benchmark_name].unrolling_unix_time[i] = unix_time
    elif mapping_type.value <= 2:
      if unix_time < self.data_of_each_benchmark[benchmark_name].remapping_unix_time[mapping_type.value]:
        return

      if benchmark_name not in self.benchmark_to_parallel_num.keys():
        self.benchmark_to_parallel_num[benchmark_name] = [0,0,0]
      if parallel_num < self.benchmark_to_parallel_num[benchmark_name][mapping_type.value]:
        return
      
      self.data_of_each_benchmark[benchmark_name].remapping_time[mapping_type.value] = time
      self.data_of_each_benchmark[benchmark_name].remapping_unix_time[mapping_type.value] = unix_time
      self.benchmark_to_parallel_num[benchmark_name][mapping_type.value] = parallel_num

  def plot(self, image_name):
    check_dir_availability("./output/same_parallel_num/")
    dp_time_list = []
    greedy_time_list = []
    full_search_time_list = []
    for benchmark in self.data_of_each_benchmark.keys():
      dp_time_list.append(self.data_of_each_benchmark[benchmark].remapping_time[MappingType.dp.value] / self.data_of_each_benchmark[benchmark].unrolling_time[MappingType.dp.value])
      greedy_time_list.append(self.data_of_each_benchmark[benchmark].remapping_time[MappingType.greedy.value] / self.data_of_each_benchmark[benchmark].unrolling_time[MappingType.greedy.value])
      full_search_time_list.append(self.data_of_each_benchmark[benchmark].remapping_time[MappingType.full_search.value] / self.data_of_each_benchmark[benchmark].unrolling_time[MappingType.full_search.value])

      if dp_time_list[-1] < 0:
        dp_time_list[-1] = 0
      if greedy_time_list[-1] < 0:
        greedy_time_list[-1] = 0
      if full_search_time_list[-1] < 0:
        full_search_time_list[-1] = 0

    fig, ax = plt.subplots()
    loop_unrolling_pos = range(0, len(self.data_of_each_benchmark.keys()))
    full_search_pos = [pos + 0.2 for pos in loop_unrolling_pos]
    greedy_pos = [pos + 0.4 for pos in loop_unrolling_pos]
    dp_pos = [pos + 0.6 for pos in loop_unrolling_pos]

    ax.bar(full_search_pos, full_search_time_list, width=0.2, label="full seasrch")
    ax.bar(dp_pos, dp_time_list, width=0.2, label="dp")
    ax.bar(greedy_pos, greedy_time_list, width=0.2, label="greedy")
    ax.set_xlabel("benchmark")
    ax.set_ylabel("time")
    ax.legend()
    plt.xticks(greedy_pos, self.data_of_each_benchmark.keys())
    fig.savefig("./output/same_parallel_num/" + image_name + "_time.png")

    fig, ax = plt.subplots()
    full_search_time_log_list = []
    dp_time_log_list = []
    greedy_time_log_list = []

    def log(x):
      if x == 0:
        return 0
      else:
        return math.log(x)

    for i in range(len(full_search_time_list)):
      full_search_time_log_list.append(log(full_search_time_list[i]))
      dp_time_log_list.append(log(dp_time_list[i]))
      greedy_time_log_list.append(log(greedy_time_list[i]))
    ax.bar(full_search_pos, full_search_time_log_list, width=0.2, label="full seasrch")
    ax.bar(dp_pos, dp_time_log_list, width=0.2, label="dp")
    ax.bar(greedy_pos, greedy_time_log_list, width=0.2, label="greedy")
    ax.set_xlabel("benchmark")
    ax.set_ylabel("time")
    ax.legend()
    plt.xticks(greedy_pos, self.data_of_each_benchmark.keys())
    fig.savefig("./output/same_parallel_num/" + image_name + "_time_log.png")

if __name__ == "__main__": 
  args = sys.argv
  config_path = args[1]

  plotter_config = load_plotter_config(config_path)


  mapping_info_list, remapping_info_list, db_info_list = load_result_from_csv("./output/csv/", plotter_config.get_benchmark_list())

  db_manager = DatabaseManager(db_info_list)
  benchmark_node_num = {}

  for benchmark in plotter_config.get_benchmark_list():
    dfg_file_path = plotter_config.kernel_dir_path + benchmark + ".dot"
    G = nx.Graph(nx.nx_pydot.read_dot(dfg_file_path))
    dfg_node_size = len(G.nodes())

    for node in G.nodes():
      if len(node) == 2: # NOTE: skip "\n" node
        dfg_node_size = dfg_node_size - 1
    benchmark_node_num[benchmark] = dfg_node_size

  memory_io_to_all_data_to_plot = {}
  memory_io_to_all_data_to_plot["all"] = AllDataToPlot()
  memory_io_to_all_data_to_plot["both_ends"] = AllDataToPlot()
  all_context = plotter_config.compare_benchmark_config.row * plotter_config.compare_benchmark_config.column * plotter_config.compare_benchmark_config.context_size

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
    if row != plotter_config.compare_benchmark_config.row or column != plotter_config.compare_benchmark_config.column or context_size != plotter_config.compare_benchmark_config.context_size:
      continue
    if network_type != plotter_config.compare_benchmark_config.network_type:
      continue
    if cgra_type != CGRAType.Elastic:
      continue

    utilization = remapping_info.parallel_num * benchmark_node_num[benchmark]/ all_context

    database_info = db_manager.get_database_info(remapping_info)
    if database_info == None:
      continue
    if database_info.timeout != plotter_config.database_timeout:
      continue
    time = remapping_info.remapper_time + database_info.creating_time

    benchmark_idx = plotter_config.get_benchmark_list().index(benchmark)
    benchmark_name = plotter_config.get_benchmark_name_list()[benchmark_idx]

    if remapping_info.remapper_mode == RemapperType.FullSearch:
      memory_io_to_all_data_to_plot[memory_io.to_string()].add_benchmark_data(benchmark_name, MappingType.full_search, utilization, time, remapping_info.get_unix_time())
    elif remapping_info.remapper_mode == RemapperType.DP:
      memory_io_to_all_data_to_plot[memory_io.to_string()].add_benchmark_data(benchmark_name, MappingType.dp, utilization, time, remapping_info.get_unix_time())
    elif remapping_info.remapper_mode == RemapperType.Greedy:
      memory_io_to_all_data_to_plot[memory_io.to_string()].add_benchmark_data(benchmark_name, MappingType.greedy, utilization, time, remapping_info.get_unix_time())
  
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
    if row != plotter_config.compare_benchmark_config.row or column != plotter_config.compare_benchmark_config.column or context_size != plotter_config.compare_benchmark_config.context_size:
      continue
    if network_type != plotter_config.compare_benchmark_config.network_type:
      continue
    if cgra_type != CGRAType.Elastic:
      continue
    if mapping_succeed == False:
      continue

    utilization = mapping_info.parallel_num * benchmark_node_num[benchmark]/ all_context

    benchmark_idx = plotter_config.get_benchmark_list().index(benchmark)
    benchmark_name = plotter_config.get_benchmark_name_list()[benchmark_idx]

    memory_io_to_all_data_to_plot[memory_io.to_string()].add_benchmark_data(benchmark_name, MappingType.loop_unrolling, utilization, mapping_info.mapping_time, mapping_info.get_unix_time())

  memory_io_to_all_data_to_plot["all"].plot("all")
  memory_io_to_all_data_to_plot["both_ends"].plot("both_ends")



    

    








