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

class RemappingRunnerConfig:
  def __init__(self):
    # exec setting
    self.overwrite = False
    self.kernel_dir_path = ""
    self.output_dir_path = ""
    self.create_database = 0
    self.database_timeout_s = 0
    self.remapper_timeout_s = 0
    self.process_num = 32

    # auto mapping
    self.cgra_type_list = []
    self.cgra_size_list = []
    self.memory_io_list = []
    self.network_type_list = []
    self.local_reg_size = 0
    self.db_num = -1
    self.context_size = 0

    self.benchmark_name_list = []

    # manual mapping
    self.remapper_mode = []

  def load(self, config_path):
    file = open(config_path, mode="r")
    config_dict = json.load(file)
    file.close()

    self.overwrite = config_dict["exec_setting"]["overwrite"]
    self.kernel_dir_path = config_dict["exec_setting"]["kernel_dir_path"]
    self.output_dir_path = config_dict["exec_setting"]["output_dir_path"]
    self.create_database = config_dict["exec_setting"]["create_database"]
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
    self.db_num = config_dict["remapping_setting"]["db_num"]

    self.benchmark_name_list = config_dict["benchmark_name"]

    self.remapper_mode = []
    for mode in config_dict["remapper_mode"]:
      self.remapper_mode.append(RemapperType.from_string(mode))

  def get_database_input_list(self):
    input_list = []

    for benchmark_name in self.benchmark_name_list:
      dfg_file_path = os.path.join(self.kernel_dir_path, benchmark_name + ".dot")
      benchmark_output_dir_path = os.path.join(self.output_dir_path, benchmark_name)

      for cgra_type in self.cgra_type_list:
        for cgra_size in self.cgra_size_list:
          for memory_io in self.memory_io_list:
            for network_type in self.network_type_list:
              cgra = CGRA(cgra_type, cgra_size, cgra_size, self.context_size, memory_io, network_type, self.local_reg_size)
              input = CreateDatabaseInput(dfg_file_path, benchmark_output_dir_path, cgra, self.database_timeout_s, self.overwrite)
              input_list.append(input)

    return input_list

  def get_remapper_input_list(self, database_timeout_s):
    input_list = []

    for benchmark_name in self.benchmark_name_list:
      benchmark_output_dir_path = os.path.join(self.output_dir_path, benchmark_name)

      for cgra_type in self.cgra_type_list:
        for cgra_size in self.cgra_size_list:
          for memory_io in self.memory_io_list:
            for network_type in self.network_type_list:
              cgra = CGRA(cgra_type, cgra_size, cgra_size, self.context_size, memory_io, network_type, self.local_reg_size)
              database_dir_path = os.path.join(benchmark_output_dir_path, "database/mapping/" + get_database_id(cgra, database_timeout_s) + "/mapping/mapping")
              for mode in self.remapper_mode:
                input = RemapperInput(database_dir_path, cgra, benchmark_output_dir_path, mode, self.remapper_timeout_s - self.database_timeout_s, self.db_num)
                input_list.append(input)

    return input_list

if __name__ == "__main__":
  args = sys.argv
  config_path = args[1]

  config = RemappingRunnerConfig()
  config.load(config_path)

  lock = multiprocessing.Lock()

  log_file_path = os.path.join(os.getcwd(), "log/remapper/" + str(int(time.time())) + ".log")
  check_dir_availability(os.path.dirname(log_file_path))

  pool = multiprocessing.Pool(config.process_num, initializer=init, initargs=(lock, log_file_path))

  if config.create_database:
    pool.map(create_database_exec, config.get_database_input_list())

  pool.map(remapper_exec, config.get_remapper_input_list(config.database_timeout_s))
