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
import csv

def check_dir_availability(dir_name):
  if not os.path.exists(dir_name):
    os.makedirs(dir_name)

class MappingType(enum.Enum):
  dp = 0
  greedy = 1

  @staticmethod
  def create_mapping_type(id):
    if id == 0:
      return MappingType.dp
    elif id == 1:
      return MappingType.greedy

  def to_string(self):
    if self == MappingType.dp:
      return "dp"
    elif self == MappingType.greedy:
      return "greedy"

class DataToPlot:
  def __init__(self, database_timeout_list):
    self.utilization = []
    self.time = []
    self.unix_time = []
    self.timeout_to_index = {}
    for i in range(2):
      self.utilization.append([])
      self.time.append([])
      self.unix_time.append([])
      for j, timeout in enumerate(database_timeout_list):
        self.timeout_to_index[timeout] = j
        self.utilization[i].append(-1)
        self.time[i].append(-1)
        self.unix_time[i].append(-1)

  def add_data(self, timeout, util, time, unix_time, mapping_type):
    if unix_time < self.unix_time[mapping_type.value][self.timeout_to_index[timeout]]:
      return
    # if util < self.utilization[mapping_type.value][self.timeout_to_index[timeout]]:
    #   return
    self.utilization[mapping_type.value][self.timeout_to_index[timeout]] = util
    self.time[mapping_type.value][self.timeout_to_index[timeout]] = time
    self.unix_time[mapping_type.value][self.timeout_to_index[timeout]] = unix_time

  def get_util(self, timeout, mapping_type):
    return self.utilization[mapping_type.value][self.timeout_to_index[timeout]]

  def get_time(self, timeout, mapping_type):
    return self.time[mapping_type.value][self.timeout_to_index[timeout]]

class AllDataToPlot:
  def __init__(self, db_timeout_s_list):
    self.data_of_each_benchmark: dict = {}
    self.db_timeout_s_list = db_timeout_s_list

  def add_benchmark_data(self, benchmark_name, mapping_type, utilization, time, unix_time, db_timeout_s):
    if benchmark_name not in self.data_of_each_benchmark.keys():
      self.data_of_each_benchmark[benchmark_name] = DataToPlot(self.db_timeout_s_list)
    self.data_of_each_benchmark[benchmark_name].add_data(db_timeout_s, utilization, time, unix_time, mapping_type)

  def output_csv(self, file_name):
    check_dir_availability("./output/compare_db_timeout/")
    f = open("./output/compare_db_timeout/" + file_name + ".csv", 'w', encoding='utf-8', newline='')
    data_writer = csv.writer(f)
    data_writer.writerow(["benchmark_name","database_timeout", "dp_util", "dp_time", "greedy_util", "greedy_time"])
    for benchmark in self.data_of_each_benchmark.keys():
      for timeout in self.db_timeout_s_list:
        row = [benchmark, timeout]
        for mapping_type_idx in range(0,2):
          mapping_type = MappingType.create_mapping_type(mapping_type_idx)
          row.append(self.data_of_each_benchmark[benchmark].get_util(timeout, mapping_type))
          row.append(self.data_of_each_benchmark[benchmark].get_time(timeout, mapping_type))
        data_writer.writerow(row)
    f.close()

  def plot(self, image_name):
    check_dir_availability("./output/compare_db_timeout/")
    label_list = ["two-phase:dp", "two-phase:greedy", "two-phase: full_search"]
    marker_list = ["v", "^"]
    for benchmark in self.data_of_each_benchmark.keys():
      fig, ax = plt.subplots()
      for mapping_type_idx in range(0,2):
        mapping_type = MappingType.create_mapping_type(mapping_type_idx)
        util_list = []
        for timeout in self.db_timeout_s_list:
          util = self.data_of_each_benchmark[benchmark].get_util(timeout, mapping_type)
          util_list.append(util)
        ax.plot(self.db_timeout_s_list, util_list, marker=marker_list[mapping_type.value], label=label_list[mapping_type.value])
      ax.set_xlabel("db timeout (s)")
      ax.set_ylabel("utilization")
      ax.legend()

      fig.savefig("./output/compare_db_timeout/" + image_name + "_"+ benchmark + "_util.pdf")

      fig, ax = plt.subplots()
      for mapping_type_idx in range(0,2):
        mapping_type = MappingType.create_mapping_type(mapping_type_idx)
        time_list = []
        for timeout in self.db_timeout_s_list:
          time = self.data_of_each_benchmark[benchmark].get_time(timeout, mapping_type)
          time_list.append(time)
        ax.plot(self.db_timeout_s_list, time_list, marker=marker_list[mapping_type.value], label=label_list[mapping_type.value])
      ax.set_xlabel("db timeout (s)")
      ax.set_ylabel("time")
      ax.legend()

      fig.savefig("./output/compare_db_timeout/" + image_name + "_" + benchmark + "_time.pdf")

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
  memory_io_to_all_data_to_plot["all"] = AllDataToPlot(plotter_config.compare_database_timeout.timeout_list)
  memory_io_to_all_data_to_plot["both_ends"] = AllDataToPlot(plotter_config.compare_database_timeout.timeout_list)

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

    if row != plotter_config.compare_database_timeout.row:
      continue
    if column != plotter_config.compare_database_timeout.column:
      continue
    if context_size != plotter_config.compare_database_timeout.context_size:
      continue
    if network_type != plotter_config.compare_database_timeout.network_type:
      continue
    if cgra_type != CGRAType.Elastic:
      continue

    all_context = row * column * context_size
    utilization = remapping_info.parallel_num * benchmark_node_num[benchmark]/ all_context

    database_info = db_manager.get_database_info(remapping_info)
    if database_info is None:
      continue
    if database_info.timeout not in plotter_config.compare_database_timeout.timeout_list:
      continue
    time = remapping_info.remapper_time + database_info.creating_time

    if remapping_info.remapper_mode == RemapperType.DP:
      memory_io_to_all_data_to_plot[memory_io.to_string()].add_benchmark_data(benchmark, MappingType.dp, utilization, time, remapping_info.get_unix_time(), database_info.timeout)
    elif remapping_info.remapper_mode == RemapperType.Greedy:
      memory_io_to_all_data_to_plot[memory_io.to_string()].add_benchmark_data(benchmark, MappingType.greedy, utilization, time, remapping_info.get_unix_time(), database_info.timeout)

  memory_io_to_all_data_to_plot["all"].plot("all")
  memory_io_to_all_data_to_plot["both_ends"].plot("both_ends")

  memory_io_to_all_data_to_plot["all"].output_csv("all")
  memory_io_to_all_data_to_plot["both_ends"].output_csv("both_ends")
