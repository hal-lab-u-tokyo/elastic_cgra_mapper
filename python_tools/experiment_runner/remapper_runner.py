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

class RemappingRunnerConfig:
  def __init__(self):
    # exec setting
    self.kernel_dir_path = ""
    self.output_dir_path = ""
    self.create_database = 0
    self.database_timeout_s = 0
    self.remapper_timeout_s = 0
    self.mapping_config_path = ""
    self.process_num = 32

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
    self.create_database = config_dict["exec_setting"]["create_database"]
    self.database_path = config_dict["exec_setting"]["database_path"]
    self.mapping_config_path = config_dict["exec_setting"]["mapping_config_path"]
    self.database_timeout_s = config_dict["exec_setting"]["database_timeout_s"]
    self.remapper_timeout_s = config_dict["exec_setting"]["remapper_timeout_s"]
    self.process_num = config_dict["exec_setting"]["process_num"]

    self.cgra_type_list = []
    for cgra_type_str in config_dict["remapping_setting"]["cgra_type"]:
      self.cgra_type_list.append(CGRAType.get_from_string(cgra_type_str))
    cgra_size_min = config_dict["remapping_setting"]["cgra_size"]["min"]
    cgra_size_max = config_dict["remapping_setting"]["cgra_size"]["max"]
    self.cgra_size_list = list(range(cgra_size_min, cgra_size_max + 1))
    self.memory_io_list = []
    for memory_io in config_dict["remapping_setting"]["memory_io"]:
      self.memory_io_list.append(MemoryIOType.get_from_string(memory_io))
    self.network_type_list = []
    for network_type in config_dict["remapping_setting"]["network_type"]:
      self.network_type_list.append(NetworkType.get_from_string(network_type))
    self.local_reg_size = config_dict["remapping_setting"]["local_reg_size"]
    self.context_size = config_dict["remapping_setting"]["context_size"]
    self.loop_controller_list = config_dict["remapping_setting"]["loop_controllers"]

    self.benchmark_name_list = config_dict["benchmark_name"]

    self.remapper_mode = []
    for mode in config_dict["remapper_mode"]:
      self.remapper_mode.append(RemapperType.from_string(mode))

  def get_database_input_list(self, output_dir_path):
    input_list = []

    for benchmark_name in self.benchmark_name_list:
      dfg_file_path = os.path.join(self.kernel_dir_path, benchmark_name + ".dot")
      output_dir_path = os.path.join(output_dir_path)

      for cgra_type in self.cgra_type_list:
        for cgra_size in self.cgra_size_list:
          for memory_io in self.memory_io_list:
            for network_type in self.network_type_list:
              cgra = CGRA(cgra_type, cgra_size, cgra_size, self.context_size, memory_io, network_type, self.local_reg_size, self.loop_controller_list)
              input = CreateDatabaseInput(dfg_file_path, output_dir_path, cgra, self.mapping_config_path, self.database_timeout_s)
              input_list.append(input)

    return input_list

  def get_remapper_input_list(self, database_timeout_s, remapper_output_dir_path, database_output_dir_path):
    input_list = []

    for benchmark_name in self.benchmark_name_list:
      output_dir_path = os.path.join(remapper_output_dir_path)

      for cgra_type in self.cgra_type_list:
        for cgra_size in self.cgra_size_list:
          for memory_io in self.memory_io_list:
            for network_type in self.network_type_list:
              cgra = CGRA(cgra_type, cgra_size, cgra_size, self.context_size, memory_io, network_type, self.local_reg_size, self.loop_controller_list)
              if self.create_database:
                database_dir_path = database_output_dir_path
              else:
                database_dir_path = self.database_path
              for mode in self.remapper_mode:
                input = RemapperInput(database_dir_path, cgra, output_dir_path, mode, self.remapper_timeout_s - self.database_timeout_s)
                input_list.append(input)

    return input_list

if __name__ == "__main__":
  args = sys.argv
  config_path = args[1]

  config = RemappingRunnerConfig()
  config.load(config_path)

  lock = multiprocessing.Lock()

  experiment_output_dir_path = os.path.join(config.output_dir_path, "experiments/" + get_datetime_string())
  os.makedirs(experiment_output_dir_path, exist_ok=True)
  shutil.copy(config_path, os.path.join(experiment_output_dir_path, "remapper_config.json"))

  log_file_path = os.path.join(experiment_output_dir_path, get_datetime_string() + ".log")
  check_dir_availability(os.path.dirname(log_file_path))

  pool = multiprocessing.Pool(config.process_num, initializer=init, initargs=(lock, log_file_path))

  database_output_dir_path = ""
  if config.create_database:
    database_output_dir_path = os.path.join(experiment_output_dir_path, "database")
    os.makedirs(database_output_dir_path, exist_ok=True)
    pool.map(create_database_exec, config.get_database_input_list(database_output_dir_path))

  remapper_output_dir_path = os.path.join(experiment_output_dir_path, "remapper")
  os.makedirs(remapper_output_dir_path, exist_ok=True)

  pool.map(remapper_exec, config.get_remapper_input_list(config.database_timeout_s, remapper_output_dir_path, database_output_dir_path))
