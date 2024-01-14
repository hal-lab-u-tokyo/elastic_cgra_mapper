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
  database = 4

class DataToPlot:
  def __init__(self):
    self.utilization: List[float] = [0,0,0,0,0]
    self.time: List[float] = [0,0,0,-1,0]
    self.unix_time: List[int] = [0,0,0,0,0]

class AllDataToPlot:
  def __init__(self, color_settings):
    self.data_of_each_benchmark: dict = {}
    self.color_settings: dict = color_settings

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
    check_dir_availability("./output/compare_benchmark/")
    dp_utilization = []
    greedy_utilization = []
    full_search_utilization = []
    loop_unrolling_utilization = []
    dp_time_list = []
    greedy_time_list = []
    full_search_time_list = []
    db_time_list = []
    for benchmark in self.data_of_each_benchmark.keys():
      dp_utilization.append(self.data_of_each_benchmark[benchmark].utilization[MappingType.dp.value])
      greedy_utilization.append(self.data_of_each_benchmark[benchmark].utilization[MappingType.greedy.value])
      full_search_utilization.append(self.data_of_each_benchmark[benchmark].utilization[MappingType.full_search.value])
      loop_unrolling_utilization.append(self.data_of_each_benchmark[benchmark].utilization[MappingType.loop_unrolling.value])

      dp_time_list.append(self.data_of_each_benchmark[benchmark].time[MappingType.dp.value] / self.data_of_each_benchmark[benchmark].time[MappingType.loop_unrolling.value])
      greedy_time_list.append(self.data_of_each_benchmark[benchmark].time[MappingType.greedy.value] / self.data_of_each_benchmark[benchmark].time[MappingType.loop_unrolling.value])
      full_search_time_list.append(self.data_of_each_benchmark[benchmark].time[MappingType.full_search.value] / self.data_of_each_benchmark[benchmark].time[MappingType.loop_unrolling.value])
      db_time_list.append(self.data_of_each_benchmark[benchmark].time[MappingType.database.value] / self.data_of_each_benchmark[benchmark].time[MappingType.loop_unrolling.value])

    fig, ax = plt.subplots()
    width = 0.15
    loop_unrolling_opt_pos = range(0, len(self.data_of_each_benchmark.keys()))
    loop_unrolling_timeout_pos = [pos + width for pos in loop_unrolling_opt_pos]
    full_search_pos = [pos + width * 2 for pos in loop_unrolling_opt_pos]
    greedy_pos = [pos + width * 3 for pos in loop_unrolling_opt_pos]
    dp_pos = [pos + width * 4 for pos in loop_unrolling_opt_pos]
    label_pos = [pos + width * 2 for pos in loop_unrolling_opt_pos]

    loop_unrolling_opt_utilization = []
    for i in range(len(loop_unrolling_utilization)):
      max_remapping = max(dp_utilization[i], greedy_utilization[i], full_search_utilization[i])
      if loop_unrolling_utilization[i] < max_remapping:
        loop_unrolling_opt_utilization.append(0)
      else:
        loop_unrolling_opt_utilization.append(loop_unrolling_utilization[i])

    rects = ax.bar(loop_unrolling_opt_pos, loop_unrolling_opt_utilization, width=width, label="single-phase: optimal", color=self.color_settings["single-phase"]["optimal"])
    for rect in rects:
      height = rect.get_height()
      if height == 0:
        ax.annotate("✖️", xy=(rect.get_x() + rect.get_width() / 2, height), xytext=(0, 3), textcoords="offset points", ha="center", va="bottom")
    ax.bar(loop_unrolling_timeout_pos, loop_unrolling_utilization, width=width, label="single-phase: timeout", color=self.color_settings["single-phase"]["timeout"])
    rects = ax.bar(full_search_pos, full_search_utilization, width=width, label="two-phase: full search", color=self.color_settings["two-phase"]["full_search"])
    for rect in rects:
      height = rect.get_height()
      if height == 0:
        ax.annotate("✖️", xy=(rect.get_x() + rect.get_width() / 2, height), xytext=(0, 3), textcoords="offset points", ha="center", va="bottom")
    ax.bar(greedy_pos, greedy_utilization, width=width, label="two-phase: greedy", color=self.color_settings["two-phase"]["greedy"])
    ax.bar(dp_pos, dp_utilization, width=-width, label="two-phase: dp", color=self.color_settings["two-phase"]["dp"])
    ax.set_xlabel("benchmark")
    ax.set_ylabel("utilization")
    ax.legend()
    plt.xticks(label_pos, self.data_of_each_benchmark.keys())
    fig.savefig("./output/compare_benchmark/" + image_name + "_util.png")

    fig, ax = plt.subplots()
    loop_unrolling_pos = range(0, len(self.data_of_each_benchmark.keys()))
    full_search_pos = [pos + 0.2 for pos in loop_unrolling_pos]
    greedy_pos = [pos + 0.4 for pos in loop_unrolling_pos]
    dp_pos = [pos + 0.6 for pos in loop_unrolling_pos]
    database_pos = [pos + 0.8 for pos in loop_unrolling_pos]


    ax.bar(full_search_pos, full_search_time_list, width=0.2, label="full search", color=self.color_settings["two-phase"]["full_search"])
    ax.bar(greedy_pos, greedy_time_list, width=0.2, label="greedy", color=self.color_settings["two-phase"]["greedy"])
    ax.bar(dp_pos, dp_time_list, width=0.2, label="dp", color=self.color_settings["two-phase"]["dp"])
    ax.bar(database_pos, db_time_list, width=0.2, label="database", color=self.color_settings["two-phase"]["database"])
    ax.set_xlabel("benchmark")
    ax.set_ylabel("time")
    ax.legend()
    plt.xticks(greedy_pos, self.data_of_each_benchmark.keys())
    fig.savefig("./output/compare_benchmark/" + image_name + "_time.png")

    fig, ax = plt.subplots()
    full_search_pos = [pos + 0.2 for pos in loop_unrolling_pos]
    greedy_pos = [pos + 0.4 for pos in loop_unrolling_pos]
    dp_pos = [pos + 0.6 for pos in loop_unrolling_pos]
    database_pos = [pos + 0.8 for pos in loop_unrolling_pos]

    full_search_time_log_list = []
    dp_time_log_list = []
    greedy_time_log_list = []
    database_time_log_list = []

    def log(x):
      if x <= 0:
        return 0
      else:
        return math.log(x)

    for i in range(len(full_search_time_list)):
      full_search_time_log_list.append(log(full_search_time_list[i]))
      dp_time_log_list.append(log(dp_time_list[i]))
      greedy_time_log_list.append(log(greedy_time_list[i]))
      database_time_log_list.append(log(db_time_list[i]))

    ax.bar(full_search_pos, full_search_time_log_list, width=0.2, label="full search", color=self.color_settings["two-phase"]["full_search"])
    ax.bar(dp_pos, dp_time_log_list, width=0.2, label="dp", color=self.color_settings["two-phase"]["dp"])
    ax.bar(greedy_pos, greedy_time_log_list, width=0.2, label="greedy", color=self.color_settings["two-phase"]["greedy"])
    ax.bar(database_pos, database_time_log_list, width=0.2, label="database", color=self.color_settings["two-phase"]["database"])
    ax.set_xlabel("benchmark")
    ax.set_ylabel("time")
    ax.legend()
    plt.xticks(greedy_pos, self.data_of_each_benchmark.keys())
    fig.savefig("./output/compare_benchmark/" + image_name + "_time_log.png")

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
  memory_io_to_all_data_to_plot["all"] = AllDataToPlot(plotter_config.color_settings)
  memory_io_to_all_data_to_plot["both_ends"] = AllDataToPlot(plotter_config.color_settings)
  all_context = plotter_config.compare_benchmark_config.row * plotter_config.compare_benchmark_config.column * plotter_config.compare_benchmark_config.context_size

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
      memory_io_to_all_data_to_plot[memory_io.to_string()].add_benchmark_data(benchmark_name, MappingType.database, 0, database_info.creating_time, remapping_info.get_unix_time())
    elif remapping_info.remapper_mode == RemapperType.Greedy:
      memory_io_to_all_data_to_plot[memory_io.to_string()].add_benchmark_data(benchmark_name, MappingType.greedy, utilization, time, remapping_info.get_unix_time())
  
  memory_io_to_all_data_to_plot["all"].plot("all")
  memory_io_to_all_data_to_plot["both_ends"].plot("both_ends")



    

    








