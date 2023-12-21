from matplotlib import pyplot as plt
import networkx as nx
import sys
from typing import List
from mapping_log_reader import *
from load_remapper_config import *
from load_result_from_csv import *
import re
import enum

class MappingType(enum.Enum):
  dp = 0
  greedy = 1
  full_search = 2
  loop_unrolling = 3

class DataToPlot:
  def __init__(self):
    self.utilization: List[float] = [0,0,0,0]
    self.time: List[float] = [0,0,0,0]
    self.unix_time: List[int] = [0,0,0,0]

class AllDataToPlot:
  def __init__(self):
    self.data_of_each_benchmark: dict = {}

  def add_benchmark_data(self, benchmark_name, mapping_type, utilization, time, unix_time):
    if benchmark_name not in self.data_of_each_benchmark.keys():
      self.data_of_each_benchmark[benchmark_name] = DataToPlot()
    if utilization < self.data_of_each_benchmark[benchmark_name].utilization[mapping_type.value]:
      return
    if unix_time < self.data_of_each_benchmark[benchmark_name].unix_time[mapping_type.value]:
      return
    
    self.data_of_each_benchmark[benchmark_name].utilization[mapping_type.value] = utilization
    self.data_of_each_benchmark[benchmark_name].time[mapping_type.value] = time
    self.data_of_each_benchmark[benchmark_name].unix_time[mapping_type.value] = unix_time

  def plot(self, image_name):
    dp_utilization = []
    greedy_utilization = []
    full_search_utilization = []
    loop_unrolling_utilization = []
    dp_time_list = []
    greedy_time_list = []
    full_search_time_list = []
    for benchmark in self.data_of_each_benchmark.keys():
      dp_utilization.append(self.data_of_each_benchmark[benchmark].utilization[MappingType.dp.value])
      greedy_utilization.append(self.data_of_each_benchmark[benchmark].utilization[MappingType.greedy.value])
      full_search_utilization.append(self.data_of_each_benchmark[benchmark].utilization[MappingType.full_search.value])
      loop_unrolling_utilization.append(self.data_of_each_benchmark[benchmark].utilization[MappingType.loop_unrolling.value])
      if self.data_of_each_benchmark[benchmark].time[MappingType.loop_unrolling.value] == 0:
        dp_time_list.append(-1)
        greedy_time_list.append(-1)
        full_search_utilization.append(-1)
      else:
        dp_time_list.append(self.data_of_each_benchmark[benchmark].time[MappingType.dp.value] / self.data_of_each_benchmark[benchmark].time[MappingType.loop_unrolling.value])
        greedy_time_list.append(self.data_of_each_benchmark[benchmark].time[MappingType.greedy.value] / self.data_of_each_benchmark[benchmark].time[MappingType.loop_unrolling.value])
        full_search_time_list.append(self.data_of_each_benchmark[benchmark].time[MappingType.full_search.value] / self.data_of_each_benchmark[benchmark].time[MappingType.loop_unrolling.value])

    fig, ax = plt.subplots() 
    loop_unrolling_pos = range(0, len(self.data_of_each_benchmark.keys()))
    full_search_pos = [pos + 0.2 for pos in loop_unrolling_pos]
    greedy_pos = [pos + 0.4 for pos in loop_unrolling_pos]
    dp_pos = [pos + 0.6 for pos in loop_unrolling_pos]
    label_pos = [pos + 0.3 for pos in loop_unrolling_pos]

    ax.bar(loop_unrolling_pos, loop_unrolling_utilization, width=0.2, label="no remapping")
    ax.bar(full_search_pos, full_search_utilization, width=0.2, label="remapping: full search")
    ax.bar(greedy_pos, greedy_utilization, width=0.2, label="remapping: greedy")
    ax.bar(dp_pos, dp_utilization, width=-0.2, label="remapping: dp")
    ax.set_xlabel("benchmark")
    ax.set_ylabel("utilization")
    ax.legend()
    plt.xticks(label_pos, self.data_of_each_benchmark.keys())
    fig.savefig("./output/utilization_comparison/" + image_name + "_util.png")

    fig, ax = plt.subplots()
    full_search_pos = [pos + 0.3 for pos in loop_unrolling_pos]
    greedy_pos = [pos + 0.6 for pos in loop_unrolling_pos]
    dp_pos = [pos + 0.9 for pos in loop_unrolling_pos]
    ax.bar(full_search_pos, full_search_time_list, width=0.3, label="full seasrch")
    ax.bar(dp_pos, dp_time_list, width=0.3, label="dp")
    ax.bar(greedy_pos, greedy_time_list, width=0.3, label="greedy")
    ax.set_xlabel("benchmark")
    ax.set_ylabel("time")
    ax.legend()
    plt.xticks(greedy_pos, self.data_of_each_benchmark.keys())
    fig.savefig("./output/utilization_comparison/" + image_name + "_time.png")

if __name__ == "__main__": 
  args = sys.argv
  config_path = args[1]

  remapper_config = load_remapper_config(config_path)

  mapping_info_list, remapping_info_list = load_result_from_csv("./output/csv/", remapper_config.get_benchmark_list())

  benchmark_node_num = {}

  database_time = {}
  for benchmark in remapper_config.get_benchmark_list():
    dfg_file_path = remapper_config.kernel_dir_path + benchmark + ".dot"
    G = nx.Graph(nx.nx_pydot.read_dot(dfg_file_path))
    dfg_node_size = len(G.nodes())

    benchmark_node_num[benchmark] = dfg_node_size

    # database_time
    benchmark_database_dir_path = remapper_config.database_dir_path + benchmark + "/database/"
    for file in os.listdir(benchmark_database_dir_path + "mapping/"):
      unix_time_str = re.findall(r"\d+", file)[0]
      log_file_path = benchmark_database_dir_path + "log/log" + unix_time_str + ".log"
      mapping_log = mapping_log_reader(log_file_path, remapper_config.get_benchmark_list())
      if benchmark not in database_time.keys():
        database_time[benchmark] = 0
      database_time[benchmark] = database_time[benchmark] + mapping_log.mapping_time


  memory_io_to_all_data_to_plot = {}
  memory_io_to_all_data_to_plot["all"] = AllDataToPlot()
  memory_io_to_all_data_to_plot["both_ends"] = AllDataToPlot()
  all_context = remapper_config.compare_benchmark_config.row * remapper_config.compare_benchmark_config.column * remapper_config.compare_benchmark_config.context_size

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
    if row != remapper_config.compare_benchmark_config.row or column != remapper_config.compare_benchmark_config.column or context_size != remapper_config.compare_benchmark_config.context_size:
      continue
    if network_type != remapper_config.compare_benchmark_config.network_type:
      continue
    if cgra_type != CGRAType.Elastic:
      continue
    if mapping_succeed == False:
      continue

    utilization = mapping_info.parallel_num * benchmark_node_num[benchmark]/ all_context

    benchmark_idx = remapper_config.get_benchmark_list().index(benchmark)
    benchmark_name = remapper_config.get_benchmark_name_list()[benchmark_idx]

    memory_io_to_all_data_to_plot[memory_io.to_string()].add_benchmark_data(benchmark_name, MappingType.loop_unrolling, utilization, mapping_info.mapping_time, mapping_info.get_unix_time())

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
    if row != remapper_config.compare_benchmark_config.row or column != remapper_config.compare_benchmark_config.column or context_size != remapper_config.compare_benchmark_config.context_size:
      continue
    if network_type != remapper_config.compare_benchmark_config.network_type:
      continue
    if cgra_type != CGRAType.Elastic:
      continue

    utilization = remapping_info.parallel_num * benchmark_node_num[benchmark]/ all_context
    time = remapping_info.remapper_time + database_time[benchmark]

    benchmark_idx = remapper_config.get_benchmark_list().index(benchmark)
    benchmark_name = remapper_config.get_benchmark_name_list()[benchmark_idx]

    if remapping_info.remapper_mode == RemapperType.FullSearch:
      memory_io_to_all_data_to_plot[memory_io.to_string()].add_benchmark_data(benchmark_name, MappingType.full_search, utilization, time, remapping_info.get_unix_time())
    elif remapping_info.remapper_mode == RemapperType.DP:
      memory_io_to_all_data_to_plot[memory_io.to_string()].add_benchmark_data(benchmark_name, MappingType.dp, utilization, time, remapping_info.get_unix_time())
    elif remapping_info.remapper_mode == RemapperType.Greedy:
      memory_io_to_all_data_to_plot[memory_io.to_string()].add_benchmark_data(benchmark_name, MappingType.greedy, utilization, time, remapping_info.get_unix_time())
  
  memory_io_to_all_data_to_plot["all"].plot("all")
  memory_io_to_all_data_to_plot["both_ends"].plot("both_ends")



    

    








