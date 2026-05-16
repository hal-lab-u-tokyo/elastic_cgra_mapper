import json
from entity import *
from typing import List

class CompareBenchmarkConfig:
  def __init__(self):
    self.row: int = 0
    self.column: int = 0
    self.context_size: int = 0
    self.network_type: NetworkType = NetworkType.Orthogonal

class CompareCGRASizeConfig:
  def __init__(self):
    self.min_size: int = 0
    self.max_size: int = 0
    self.context_size: int = 0
    self.network_type: NetworkType = NetworkType.Orthogonal

class CompareCGRATypeConfig:
  def __init__(self):
    self.min_size: int = 0
    self.max_size: int = 0
    self.context_size: int = 0
    self.network_type: NetworkType = NetworkType.Orthogonal

class CompareAvailableMappingNumConfig:
  def __init__(self):
    self.min_size: int = 0
    self.max_size: int = 0
    self.context_size: int = 0
    self.network_type: NetworkType = NetworkType.Orthogonal
    self.num_available_mappings: List[int] = []

class CompareDatabaseTimeout:
  def __init__(self):
    self.row: int = 0
    self.column: int = 0
    self.context_size: int = 0
    self.network_type: NetworkType = NetworkType.Orthogonal
    self.timeout_list = []

class BenchmarkConfig:
  def __init__(self):
    self.kernel_name: str = ""
    self.name: str = ""
    self.visualize: bool = False

class PlotterConfig:
  def __init__(self):
    self.color_settings: dict = {}
    self.kernel_dir_path: str =  ""
    self.database_dir_path: str = ""
    self.database_timeout: float = 0
    self.compare_benchmark_config: CompareBenchmarkConfig = CompareBenchmarkConfig()
    self.compare_cgra_size_config: CompareCGRASizeConfig = CompareCGRASizeConfig()
    self.compare_cgra_type_config: CompareCGRATypeConfig = CompareCGRATypeConfig()
    self.compare_available_mapping_num_config: CompareAvailableMappingNumConfig = CompareAvailableMappingNumConfig()
    self.compare_database_timeout: CompareDatabaseTimeout = CompareDatabaseTimeout()
    self.benchmark_list: List[BenchmarkConfig] = []
    self.mapping_output_dir_path: str = ""
    self.remapper_output_dir_path: str = ""
    self.visualizer_output_dir_path: str = ""

  def get_benchmark_list(self):
    benchmark_list = []
    for benchmark in self.benchmark_list:
      if benchmark.visualize:
        benchmark_list.append(benchmark.kernel_name)

    return benchmark_list

  def get_all_benchmark_list(self):
    benchmark_list = []
    for benchmark in self.benchmark_list:
      benchmark_list.append(benchmark.kernel_name)

    return benchmark_list

  def get_benchmark_name_list(self):
    benchmark_name_list = []
    for benchmark in self.benchmark_list:
      if benchmark.visualize:
        benchmark_name_list.append(benchmark.name)

    return benchmark_name_list


def load_plotter_config(config_path):
  f = open(config_path, 'r')
  json_dict = json.load(f)

  plotter_config = PlotterConfig()

  # kernel dir path
  plotter_config.color_settings = json_dict["color_settings"]
  plotter_config.database_timeout = json_dict["database_timeout"]

  # compare benchmark config
  plotter_config.compare_benchmark_config.row = json_dict["compare_benchmark_config"]["row"]
  plotter_config.compare_benchmark_config.column = json_dict["compare_benchmark_config"]["column"]
  plotter_config.compare_benchmark_config.context_size = json_dict["compare_benchmark_config"]["context_size"]
  plotter_config.compare_benchmark_config.network_type = NetworkType.get_from_string(json_dict["compare_benchmark_config"]["network_type"])

  # compare cgra size config
  plotter_config.compare_cgra_size_config.min_size = json_dict["compare_cgra_size_config"]["min_size"]
  plotter_config.compare_cgra_size_config.max_size = json_dict["compare_cgra_size_config"]["max_size"]
  plotter_config.compare_cgra_size_config.context_size = json_dict["compare_cgra_size_config"]["context_size"]
  plotter_config.compare_cgra_size_config.network_type = NetworkType.get_from_string(json_dict["compare_cgra_size_config"]["network_type"])

  # compare cgra type config
  plotter_config.compare_cgra_type_config.min_size = json_dict["compare_cgra_type_config"]["min_size"]
  plotter_config.compare_cgra_type_config.max_size = json_dict["compare_cgra_type_config"]["max_size"]
  plotter_config.compare_cgra_type_config.context_size = json_dict["compare_cgra_type_config"]["context_size"]
  plotter_config.compare_cgra_type_config.network_type = NetworkType.get_from_string(json_dict["compare_cgra_type_config"]["network_type"])

  # compare available mapping num config
  plotter_config.compare_available_mapping_num_config.min_size = json_dict["compare_available_mapping_num_config"]["min_size"]
  plotter_config.compare_available_mapping_num_config.max_size = json_dict["compare_available_mapping_num_config"]["max_size"]
  plotter_config.compare_available_mapping_num_config.context_size = json_dict["compare_available_mapping_num_config"]["context_size"]
  plotter_config.compare_available_mapping_num_config.network_type = NetworkType.get_from_string(json_dict["compare_available_mapping_num_config"]["network_type"])
  plotter_config.compare_available_mapping_num_config.num_available_mappings = json_dict["compare_available_mapping_num_config"]["num_available_mappings"]

  # compare database timeout
  plotter_config.compare_database_timeout.row = json_dict["compare_database_timeout"]["row"]
  plotter_config.compare_database_timeout.column = json_dict["compare_database_timeout"]["column"]
  plotter_config.compare_database_timeout.context_size = json_dict["compare_database_timeout"]["context_size"]
  plotter_config.compare_database_timeout.network_type = NetworkType.get_from_string(json_dict["compare_database_timeout"]["network_type"])
  plotter_config.compare_database_timeout.timeout_list = json_dict["compare_database_timeout"]["timeout_list"]

  # benchmark list
  for benchmark in json_dict["benchmark_list"]:
    benchmark_config = BenchmarkConfig()
    benchmark_config.kernel_name = benchmark["kernel_name"]
    benchmark_config.name = benchmark["name"]
    benchmark_config.visualize = benchmark["visualize"]
    plotter_config.benchmark_list.append(benchmark_config)

  return plotter_config
