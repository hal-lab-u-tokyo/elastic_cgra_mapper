import sys
import os
sys.path.append(os.pardir)
from io_lib import *
from entity import *

class ExperimentProgress:
  def __init__(self):
    self.parallel_num_success = []
    self.parallel_num_fail = []

class UtilizationExperimentProgress:
  def __init__(self, benchmark_list):
    self.target_cgra = CGRA(CGRAType.Elastic,6,6,4,MemoryIOType.All,NetworkType.Orthogonal,1)
    self.benchmark_list = benchmark_list
    self.mapping_type_list = ["NoRemapping", "FullSearch", "Greedy", "DP"]

    self.exp_key_to_progress = {}
    self.max_parallel_num = 0

  def add_mapping(self, mapping_info):
    if mapping_info.benchmark not in self.benchmark_list:
      return
    memory_io_type = mapping_info.memory_io
    
    self.target_cgra.memory_io_type = memory_io_type
    info_cgra = mapping_info.get_cgra()

    if info_cgra == self.target_cgra:
      self.max_parallel_num = max(self.max_parallel_num, mapping_info.parallel_num)
      key = self.__create_key(info_cgra, mapping_info.benchmark, self.mapping_type_list[0])

      if key not in self.exp_key_to_progress:
        self.exp_key_to_progress[key] = ExperimentProgress()

      if mapping_info.mapping_succeed:
        self.exp_key_to_progress[key].parallel_num_success.append(mapping_info.parallel_num)
      else:
        self.exp_key_to_progress[key].parallel_num_fail.append(mapping_info.parallel_num)

  def add_remapping(self, remapping_info):
    if remapping_info.benchmark not in self.benchmark_list:
      return
    memory_io_type = remapping_info.memory_io
    
    self.target_cgra.memory_io_type = memory_io_type
    info_cgra = remapping_info.get_cgra()

    if info_cgra == self.target_cgra:
      self.max_parallel_num = max(self.max_parallel_num, remapping_info.parallel_num)
      key = self.__create_key(info_cgra, remapping_info.benchmark, remapping_info.remapper_mode.to_string())
      if key not in self.exp_key_to_progress:
        self.exp_key_to_progress[key] = ExperimentProgress()

      if remapping_info.mapping_succeed:
        self.exp_key_to_progress[key].parallel_num_success.append(remapping_info.parallel_num)
      else:
        self.exp_key_to_progress[key].parallel_num_fail.append(remapping_info.parallel_num)

  def output_to_csv(self, file_path):
    csv_file = open(file_path, "w")
    writer = csv.writer(csv_file)
    row = ["memory_io", "benchmark", "remapper_mode"]
    for i in range(1, self.max_parallel_num + 1):
      row.append(i)
    writer.writerow(row)
    for memory_io in [MemoryIOType.All, MemoryIOType.BothEnds]:
      for benchmark in self.benchmark_list:
        for remapper_mode in self.mapping_type_list:
          self.target_cgra.memory_io_type = memory_io
          row = [memory_io.to_string(), benchmark, remapper_mode]
          key = self.__create_key(self.target_cgra, benchmark, remapper_mode)

          if key in self.exp_key_to_progress:
            progress = self.exp_key_to_progress[key]
          else:
            progress = ExperimentProgress()

          for i in range(1, self.max_parallel_num + 1):
            if i in progress.parallel_num_success:
              row.append("○")
            elif i in progress.parallel_num_fail:
              row.append("×")
            else:
              row.append("-")
          writer.writerow(row)
    csv_file.close()


  @staticmethod
  def __create_key(cgra, bench_mark, mapping_type):
    return cgra.memory_io_type.to_string() + "_" + bench_mark + "_" + mapping_type

class ScalabilityExperimentProgress:
  def __init__(self, benchmark_list):
    self.target_cgra = CGRA(CGRAType.Elastic,6,6,4,MemoryIOType.All,NetworkType.Orthogonal,1)
    self.benchmark_list = benchmark_list

    self.mapping_type_list = ["NoRemapping", "FullSearch", "Greedy", "DP"]
    self.exp_key_to_progress = {}
    self.min_cgra_size = 100
    self.max_cgra_size = 0
    self.max_parallel_num = 0

  def add_mapping(self, mapping_info):
    if mapping_info.row != mapping_info.column:
      return
    if mapping_info.benchmark not in self.benchmark_list:
      return

    memory_io_type = mapping_info.memory_io

    self.target_cgra.row = mapping_info.row
    self.target_cgra.column = mapping_info.column

    self.target_cgra.memory_io_type = memory_io_type
    info_cgra = mapping_info.get_cgra()

    if info_cgra == self.target_cgra:
      self.min_cgra_size = min(self.min_cgra_size, mapping_info.row)
      self.max_cgra_size = max(self.max_cgra_size, mapping_info.row)
      self.max_parallel_num = max(self.max_parallel_num, mapping_info.parallel_num)
      key = self.__create_key(info_cgra, mapping_info.benchmark, self.mapping_type_list[0])

      if key not in self.exp_key_to_progress:
        self.exp_key_to_progress[key] = ExperimentProgress()

      if mapping_info.mapping_succeed:
        self.exp_key_to_progress[key].parallel_num_success.append(mapping_info.parallel_num)
      else:
        self.exp_key_to_progress[key].parallel_num_fail.append(mapping_info.parallel_num)

  def add_remapping(self, remapping_info):
    if remapping_info.row != remapping_info.column:
      return
    if remapping_info.benchmark not in self.benchmark_list:
      return
    
    memory_io_type = remapping_info.memory_io
    
    self.target_cgra.row = mapping_info.row
    self.target_cgra.column = mapping_info.column
    self.target_cgra.memory_io_type = memory_io_type
    info_cgra = remapping_info.get_cgra()

    if info_cgra == self.target_cgra:
      self.min_cgra_size = min(self.min_cgra_size, remapping_info.row)
      self.max_cgra_size = max(self.max_cgra_size, remapping_info.row)
      self.max_parallel_num = max(self.max_parallel_num, remapping_info.parallel_num)
      key = self.__create_key(info_cgra, remapping_info.benchmark, remapping_info.remapper_mode.to_string())
      if key not in self.exp_key_to_progress:
        self.exp_key_to_progress[key] = ExperimentProgress()

      if remapping_info.mapping_succeed:
        self.exp_key_to_progress[key].parallel_num_success.append(remapping_info.parallel_num)
      else:
        self.exp_key_to_progress[key].parallel_num_fail.append(remapping_info.parallel_num)

  def output_to_csv(self, file_path):
    csv_file = open(file_path, "w")
    writer = csv.writer(csv_file)
    row = ["memory_io", "benchmark", "remapper_mode", "size"] 
    for i in range(1, self.max_parallel_num + 1):
      row.append(i)
    writer.writerow(row)
    for memory_io in [MemoryIOType.All, MemoryIOType.BothEnds]:
      for benchmark in self.benchmark_list:
        for size in range(self.min_cgra_size, self.max_cgra_size + 1):
          for remapper_mode in self.mapping_type_list:
            self.target_cgra.memory_io_type = memory_io
            row = [memory_io.to_string(), benchmark, remapper_mode, size]
            key = self.__create_key(self.target_cgra, benchmark, remapper_mode)
            if key in self.exp_key_to_progress:
              progress = self.exp_key_to_progress[key]
            else:
              progress = ExperimentProgress()

            for i in range(1, self.max_parallel_num + 1):
              if i in progress.parallel_num_success:
                row.append("○")
              elif i in progress.parallel_num_fail:
                row.append("×")
              else:
                row.append("-")
            writer.writerow(row)
    csv_file.close()

  @staticmethod
  def __create_key(cgra, bench_mark, mapping_type):
    return cgra.memory_io_type.to_string() + "_" + str(cgra.row) + "_" + str(cgra.column) + "_" + bench_mark + "_" + mapping_type

if __name__ == "__main__":
  args = sys.argv
  config_path = args[1]

  plotter_config = load_plotter_config(config_path)

  mapping_info_list, remapping_info_list, db_info_list = load_result_from_csv("./output/csv/", plotter_config.get_all_benchmark_list())

  utilization_progress = UtilizationExperimentProgress(plotter_config.get_all_benchmark_list())
  scalability_progress = ScalabilityExperimentProgress(["fixed_fft_pro"])

  for mapping_info in mapping_info_list:
    utilization_progress.add_mapping(mapping_info)
    scalability_progress.add_mapping(mapping_info)

  for remapping_info in remapping_info_list:
    utilization_progress.add_remapping(remapping_info)
    scalability_progress.add_remapping(remapping_info)

  output_dir_path = "./output/csv/progress"
  if not os.path.exists(output_dir_path):
    os.makedirs(output_dir_path)

  utilization_progress.output_to_csv(os.path.join(output_dir_path, "utilization_progress.csv"))
  scalability_progress.output_to_csv(os.path.join(output_dir_path, "scalability_progress.csv"))


