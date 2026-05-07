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
import shutil

class MappingRunnerConfig:
  def __init__(self):
    # exec setting
    self.kernel_dir_path = ""
    self.output_dir_path = ""
    self.timeout_s = 0
    self.process_num = 32
    self.mapping_config_path = ""

    # auto mapping
    self.cgra_type_list = []
    self.cgra_size_list = []
    self.memory_io_list = []
    self.network_type_list = []
    self.local_reg_size = 0
    self.context_size = 0
    self.loop_controller_list = []

    self.benchmark_name_list = []

  def load(self, config_path):
    file = open(config_path, mode="r")
    config_dict = json.load(file)
    file.close()

    self.kernel_dir_path = config_dict["exec_setting"]["kernel_dir_path"]
    self.output_dir_path = config_dict["exec_setting"]["output_dir_path"]
    self.timeout_s = config_dict["exec_setting"]["timeout_s"]
    self.process_num = config_dict["exec_setting"]["process_num"]
    self.mapping_config_path = config_dict["exec_setting"]["mapper_config_path"]

    self.cgra_type_list = []
    for cgra_type_str in config_dict["cgra_settings"]["cgra_type"]:
      self.cgra_type_list.append(CGRAType.get_from_string(cgra_type_str))
    cgra_size_min = config_dict["cgra_settings"]["cgra_size"]["min"]
    cgra_size_max = config_dict["cgra_settings"]["cgra_size"]["max"]
    self.cgra_size_list = list(range(cgra_size_min, cgra_size_max + 1))
    self.memory_io_list = []
    for memory_io in config_dict["cgra_settings"]["memory_io"]:
      self.memory_io_list.append(MemoryIOType.get_from_string(memory_io))
    self.network_type_list = []
    for network_type in config_dict["cgra_settings"]["network_type"]:
      self.network_type_list.append(NetworkType.get_from_string(network_type))
    self.local_reg_size = config_dict["cgra_settings"]["local_reg_size"]
    self.context_size = config_dict["cgra_settings"]["context_size"]
    self.loop_controller_list = config_dict["cgra_settings"]["loop_controllers"]
    self.benchmark_name_list = config_dict["benchmark_name"]

  def get_mapping_input_list(self, experiment_output_dir_path):
    mapping_input_list = []

    for benchmark_name in self.benchmark_name_list:
      dfg_file_path = os.path.join(self.kernel_dir_path, benchmark_name + ".dot")
      dfg_node_num = get_dfg_node_size(dfg_file_path)
      output_dir_path = experiment_output_dir_path
      mapping_config_path = self.mapping_config_path
      for cgra_type in self.cgra_type_list:
        for cgra_size in self.cgra_size_list:
          max_parallel_num = int(int(cgra_size) * int(cgra_size) * self.context_size / dfg_node_num)
          for memory_io in self.memory_io_list:
            for network_type in self.network_type_list:
              for parallel_num in range(1, max_parallel_num + 1):

                cgra = CGRA(cgra_type, cgra_size, cgra_size, self.context_size, memory_io, network_type, self.local_reg_size, self.loop_controller_list)
                benchmark = benchmark_name
                timeout_s = self.timeout_s

                mapping_input_list.append(MappingInput(dfg_file_path, output_dir_path, mapping_config_path, cgra, timeout_s, parallel_num))

    return mapping_input_list

if __name__ == "__main__":
  args = sys.argv
  config_path = args[1]

  config = MappingRunnerConfig()
  config.load(config_path)

  lock = multiprocessing.Lock()


  experiment_output_dir_path = os.path.join(config.output_dir_path, "experiments/" + get_datetime_string() + "/mapping")
  os.makedirs(experiment_output_dir_path, exist_ok=True)
  shutil.copy(config_path, os.path.join(experiment_output_dir_path, "mapping_config.json"))

  log_file_path = os.path.join(experiment_output_dir_path, get_datetime_string() + ".log")
  check_dir_availability(os.path.dirname(log_file_path))

  pool = multiprocessing.Pool(config.process_num, initializer=init, initargs=(lock, log_file_path))
  pool.map(mapping_exec, config.get_mapping_input_list(experiment_output_dir_path))
