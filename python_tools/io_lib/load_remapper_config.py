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


class BenchmarkConfig:
  def __init__(self):
    self.kernel_name: str = ""
    self.name: str = ""
    self.visualize: bool = False

class PlotterConfig:
  def __init__(self):
    self.kernel_dir_path: str = ""
    self.output_dir_path: str = ""
    self.visualizer_output_dir_path: str = ""
    self.compare_benchmark_config: CompareBenchmarkConfig = CompareBenchmarkConfig()
    self.compare_cgra_size_config: CompareCGRASizeConfig = CompareCGRASizeConfig()
    self.benchmark_list: List[BenchmarkConfig] = []

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


def load_remapper_config(config_path):
  f = open(config_path, 'r')
  json_dict = json.load(f)

  remapper_config = PlotterConfig()

  # kernel dir path
  remapper_config.kernel_dir_path = json_dict["kernel_dir_path"]

  # compare benchmark config
  remapper_config.compare_benchmark_config.row = json_dict["compare_benchmark_config"]["row"]
  remapper_config.compare_benchmark_config.column = json_dict["compare_benchmark_config"]["column"]
  remapper_config.compare_benchmark_config.context_size = json_dict["compare_benchmark_config"]["context_size"]
  remapper_config.compare_benchmark_config.network_type = NetworkType.get_from_string(json_dict["compare_benchmark_config"]["network_type"])

  # compare cgra size config
  remapper_config.compare_cgra_size_config.min_size = json_dict["compare_cgra_size_config"]["min_size"]
  remapper_config.compare_cgra_size_config.max_size = json_dict["compare_cgra_size_config"]["max_size"]
  remapper_config.compare_cgra_size_config.context_size = json_dict["compare_cgra_size_config"]["context_size"]
  remapper_config.compare_cgra_size_config.network_type = NetworkType.get_from_string(json_dict["compare_cgra_size_config"]["network_type"])

  # benchmark list
  for benchmark in json_dict["benchmark_list"]:
    benchmark_config = BenchmarkConfig()
    benchmark_config.kernel_name = benchmark["kernel_name"]
    benchmark_config.name = benchmark["name"]
    benchmark_config.visualize = benchmark["visualize"]
    remapper_config.benchmark_list.append(benchmark_config)

  return remapper_config
