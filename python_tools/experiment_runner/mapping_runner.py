import subprocess
import time
import json
import os
import datetime
import sys
import multiprocessing
import networkx as nx
from util import *
sys.path.append(os.pardir)
from entity import *
from io_lib import *
from exec import *

class MappingRunnerConfig:
  def __init__(self):
    # exec setting
    self.overwrite = False
    self.kernel_dir_path = ""
    self.output_dir_path = ""
    self.auto_mapping = False
    self.timeout_s = 0
    self.process_num = 32

    # auto mapping
    self.cgra_type_list = []
    self.cgra_size_list = []
    self.memory_io_list = []
    self.network_type_list = []
    self.local_reg_size = 0
    self.context_size = 0

    self.benchmark_name_list = []

    # manual mapping
    self.manual_mapping_list = []

  def load(self, config_path):
    file = open(config_path, mode="r")
    config_dict = json.load(file)
    file.close()

    self.overwrite = config_dict["exec_setting"]["overwrite"]
    self.kernel_dir_path = config_dict["exec_setting"]["kernel_dir_path"]
    self.output_dir_path = config_dict["exec_setting"]["output_dir_path"]
    self.auto_mapping = config_dict["exec_setting"]["auto_mapping"]
    self.timeout_s = config_dict["exec_setting"]["timeout_s"]
    self.process_num = config_dict["exec_setting"]["process_num"]

    self.cgra_type_list = []
    for cgra_type_str in config_dict["auto_mapping"]["cgra_settings"]["cgra_type"]:
      self.cgra_type_list.append(CGRAType.get_from_string(cgra_type_str))
    cgra_size_min = config_dict["auto_mapping"]["cgra_settings"]["cgra_size"]["min"]
    cgra_size_max = config_dict["auto_mapping"]["cgra_settings"]["cgra_size"]["max"]
    self.cgra_size_list = list(range(cgra_size_min, cgra_size_max + 1))
    self.memory_io_list = []
    for memory_io in config_dict["auto_mapping"]["cgra_settings"]["memory_io"]:
      self.memory_io_list.append(MemoryIOType.get_from_string(memory_io))
    self.network_type_list = []
    for network_type in config_dict["auto_mapping"]["cgra_settings"]["network_type"]:
      self.network_type_list.append(NetworkType.get_from_string(network_type))
    self.local_reg_size = config_dict["auto_mapping"]["cgra_settings"]["local_reg_size"]
    self.context_size = config_dict["auto_mapping"]["cgra_settings"]["context_size"]

    self.benchmark_name_list = config_dict["auto_mapping"]["benchmark_name"]

    self.manual_mapping_list = []
    for mapping_input in config_dict["manual_mapping"]:
      dfg_file_path = os.path.join(self.kernel_dir_path, mapping_input["benchmark_name"] + ".dot")
      output_dir_path = os.path.join(self.output_dir_path, mapping_input["benchmark_name"])
      cgra = CGRA(CGRAType.get_from_string(mapping_input["cgra"]["cgra_type"]), mapping_input["cgra"]["row"], mapping_input["cgra"]["column"], mapping_input["cgra"]["context_size"], MemoryIOType.get_from_string(mapping_input["cgra"]["memory_io"]), NetworkType.get_from_string(mapping_input["cgra"]["network_type"]), mapping_input["cgra"]["local_reg_size"])
      benchmark = mapping_input["benchmark_name"]
      timeout_s = self.timeout_s
      for parallel_num in mapping_input["parallel_num_list"]:
        self.manual_mapping_list.append(MappingInput(dfg_file_path, output_dir_path, cgra, timeout_s, parallel_num, self.overwrite))

  def get_mapping_input_list(self):
    mapping_input_list = []

    if self.auto_mapping:
      for benchmark_name in self.benchmark_name_list:
        dfg_file_path = os.path.join(self.kernel_dir_path, benchmark_name + ".dot")
        dfg_node_num = get_dfg_node_size(dfg_file_path)
        benchmark_output_dir_path = os.path.join(self.output_dir_path, benchmark_name)
        for cgra_type in self.cgra_type_list:
          for cgra_size in self.cgra_size_list:
            max_parallel_num = int(int(cgra_size) * int(cgra_size) * self.context_size / dfg_node_num)
            for memory_io in self.memory_io_list:
              for network_type in self.network_type_list:
                for parallel_num in range(1, max_parallel_num + 1):

                  cgra = CGRA(cgra_type, cgra_size, cgra_size, self.context_size, memory_io, network_type, self.local_reg_size)
                  benchmark = benchmark_name
                  timeout_s = self.timeout_s

                  mapping_input_list.append(MappingInput(dfg_file_path, benchmark_output_dir_path, cgra, timeout_s, parallel_num, self.overwrite))
    else:
      mapping_input_list = self.manual_mapping_list

    return mapping_input_list

if __name__ == "__main__":
  args = sys.argv
  config_path = args[1]

  config = MappingRunnerConfig()
  config.load(config_path)

  lock = multiprocessing.Lock()

  log_file_path = os.path.join(os.getcwd(), "log/mapping/" + str(int(time.time())) + ".log")
  check_dir_availability(os.path.dirname(log_file_path))

  pool = multiprocessing.Pool(config.process_num, initializer=init, initargs=(lock, log_file_path))
  pool.map(mapping_exec, config.get_mapping_input_list())
