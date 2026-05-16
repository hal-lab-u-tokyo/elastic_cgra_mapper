import matplotlib.pyplot as plt
import os
import sys
sys.path.append(os.pardir)
from entity.util import *
import numpy as np

use_geometric_mean = False
def mean(lst):
    if geometric_mean:
        return geometric_mean(lst)
    else:
        return sum(lst) / len(lst)

def geometric_mean(lst):
    product = 1
    for num in lst:
        product *= num
    return product ** (1/len(lst))

class RemappingResultPlotter:
    def __init__(self, remapper_results, plotter_config, output_dir_path):
        self.remapper_results = []
        for remapper_result in remapper_results:
            if remapper_result.mapping_succeed == True:
                    self.remapper_results.append(remapper_result)
        self.plotter_config = plotter_config
        self.output_dir_path = output_dir_path

    def plot_benchmark_comparison(self):
        output_dir_path = os.path.join(self.output_dir_path, "benchmark_comparison")
        check_dir_availability(output_dir_path)

        elastic_cgra_results_time = {}
        default_cgra_results_time = {}
        elastic_cgra_results_utilization = {}
        default_cgra_results_utilization = {}

        for remapper_result in self.remapper_results:
            valid_row = remapper_result.cgra.row == self.plotter_config.compare_benchmark_config.row
            valid_column = remapper_result.cgra.column == self.plotter_config.compare_benchmark_config.column
            valid_context_size = remapper_result.cgra.context_size == self.plotter_config.compare_benchmark_config.context_size
            valid_network_type = remapper_result.cgra.network_type == self.plotter_config.compare_benchmark_config.network_type

            is_valid_result = valid_row and valid_column and valid_context_size and valid_network_type
            if not is_valid_result:
                continue

            if remapper_result.cgra.cgra_type == CGRAType.Elastic:
                elastic_cgra_results_time[remapper_result.benchmark_name] = remapper_result.remapper_time
                elastic_cgra_results_utilization[remapper_result.benchmark_name] = remapper_result.utilization
            else:
                default_cgra_results_time[remapper_result.benchmark_name] = remapper_result.remapper_time
                default_cgra_results_utilization[remapper_result.benchmark_name] = remapper_result.utilization

        plt.figure(figsize=(10, 5))
        plt.bar(list(elastic_cgra_results_time.keys()), list(elastic_cgra_results_time.values()), label="Elastic CGRA", alpha=0.5, width=-0.4, align='edge')
        plt.bar(list(default_cgra_results_time.keys()), list(default_cgra_results_time.values()), label="Default CGRA", alpha=0.5, width=0.4, align='edge')
        plt.xlabel("Benchmark")
        plt.ylabel("Remapping Time (s)")
        plt.title("Remapping Time Comparison")
        plt.xticks(rotation=45)
        plt.legend()
        plt.tight_layout()
        plt.savefig(os.path.join(output_dir_path, "remapping_time_comparison.png"))
        plt.clf()

        plt.figure(figsize=(10, 5))
        plt.bar(list(elastic_cgra_results_utilization.keys()), list(elastic_cgra_results_utilization.values()), label="Elastic CGRA", alpha=0.5, width=-0.4, align='edge')
        plt.bar(list(default_cgra_results_utilization.keys()), list(default_cgra_results_utilization.values()), label="Default CGRA", alpha=0.5, width=0.4, align='edge')
        plt.xlabel("Benchmark")
        plt.ylabel("CGRA Utilization (%)")
        plt.title("CGRA Utilization Comparison")
        plt.xticks(rotation=45)
        plt.legend()
        plt.tight_layout()
        plt.savefig(os.path.join(output_dir_path, "cgra_utilization_comparison.png"))
        plt.clf()

    def plot_cgra_size_comparison(self):
        output_dir_path = os.path.join(self.output_dir_path, "cgra_size_comparison")
        check_dir_availability(output_dir_path)

        benchmark_names = []
        for benchmark_name in self.plotter_config.benchmark_list:
            if benchmark_name not in benchmark_names:
                benchmark_names.append(benchmark_name)
        benchmark_names_to_index = {benchmark_name: index for index, benchmark_name in enumerate(benchmark_names)}

        # {benchmark_name: {cgra_size: value}}
        elastic_cgra_results_time = {}
        default_cgra_results_time = {}
        elastic_cgra_results_utilization = {}
        default_cgra_results_utilization = {}

        for remapper_result in self.remapper_results:
            valid_shape = remapper_result.cgra.row == remapper_result.cgra.column
            valid_size = remapper_result.cgra.row in range(self.plotter_config.compare_cgra_type_config.min_size, self.plotter_config.compare_cgra_type_config.max_size + 1)
            valid_context_size = remapper_result.cgra.context_size == self.plotter_config.compare_cgra_size_config.context_size
            valid_network_type = remapper_result.cgra.network_type == self.plotter_config.compare_database_timeout.network_type

            is_valid_result = valid_shape and valid_size and valid_context_size and valid_network_type
            if not is_valid_result:
                continue

            benchmark_name = remapper_result.benchmark_name
            cgra_size = remapper_result.cgra.row
            utilization = remapper_result.utilization

            if remapper_result.cgra.cgra_type == CGRAType.Elastic:
                if benchmark_name not in elastic_cgra_results_time:
                    elastic_cgra_results_time[benchmark_name] = {}
                    elastic_cgra_results_utilization[benchmark_name] = {}
                elastic_cgra_results_time[benchmark_name][cgra_size] = remapper_result.remapper_time
                elastic_cgra_results_utilization[benchmark_name][cgra_size] = utilization
            else:
                if benchmark_name not in default_cgra_results_time:
                    default_cgra_results_time[benchmark_name] = {}
                    default_cgra_results_utilization[benchmark_name] = {}
                default_cgra_results_time[benchmark_name][cgra_size] = remapper_result.remapper_time
                default_cgra_results_utilization[benchmark_name][cgra_size] = utilization

        plt.figure(figsize=(10, 5))
        for benchmark_name in benchmark_names:
            if benchmark_name in elastic_cgra_results_time:
                cgra_sizes = sorted(elastic_cgra_results_time[benchmark_name].keys())
                times = [elastic_cgra_results_time[benchmark_name][cgra_size] for cgra_size in cgra_sizes]
                plt.plot(cgra_sizes, times, marker='o', label=f"{benchmark_name}")
        plt.xlabel("CGRA Size")
        plt.ylabel("Remapping Time (s)")
        plt.title("Remapping Time vs CGRA Size (Elastic CGRA)")
        plt.xticks(range(self.plotter_config.compare_cgra_type_config.min_size, self.plotter_config.compare_cgra_type_config.max_size + 1))
        plt.legend()
        plt.tight_layout()
        plt.savefig(os.path.join(output_dir_path, "remapping_time_vs_cgra_size_elastic_cgra.png"))
        plt.clf()

        plt.figure(figsize=(10, 5))
        for benchmark_name in benchmark_names:
            if benchmark_name in default_cgra_results_time:
                cgra_sizes = sorted(default_cgra_results_time[benchmark_name].keys())
                times = [default_cgra_results_time[benchmark_name][cgra_size] for cgra_size in cgra_sizes]
                plt.plot(cgra_sizes, times, marker='o', label=f"{benchmark_name}")
        plt.xlabel("CGRA Size")
        plt.ylabel("Remapping Time (s)")
        plt.title("Remapping Time vs CGRA Size (Default CGRA)")
        plt.xticks(range(self.plotter_config.compare_cgra_type_config.min_size, self.plotter_config.compare_cgra_type_config.max_size + 1))
        plt.legend()
        plt.tight_layout()
        plt.savefig(os.path.join(output_dir_path, "remapping_time_vs_cgra_size_default_cgra.png"))
        plt.clf()

        plt.figure(figsize=(10, 5))
        for benchmark_name in benchmark_names:
            if benchmark_name in elastic_cgra_results_utilization:
                cgra_sizes = sorted(elastic_cgra_results_utilization[benchmark_name].keys())
                utilizations = [elastic_cgra_results_utilization[benchmark_name][cgra_size] for cgra_size in cgra_sizes]
                plt.plot(cgra_sizes, utilizations, marker='o', label=f"{benchmark_name}")
        plt.xlabel("CGRA Size")
        plt.ylabel("CGRA Utilization (%)")
        plt.title("CGRA Utilization vs CGRA Size (Elastic CGRA)")
        plt.xticks(range(self.plotter_config.compare_cgra_type_config.min_size, self.plotter_config.compare_cgra_type_config.max_size + 1))
        plt.legend()
        plt.tight_layout()
        plt.savefig(os.path.join(output_dir_path, "cgra_utilization_vs_cgra_size_elastic_cgra.png"))
        plt.clf()

        plt.figure(figsize=(10, 5))
        for benchmark_name in benchmark_names:
            if benchmark_name in default_cgra_results_utilization:
                cgra_sizes = sorted(default_cgra_results_utilization[benchmark_name].keys())
                utilizations = [default_cgra_results_utilization[benchmark_name][cgra_size] for cgra_size in cgra_sizes]
                plt.plot(cgra_sizes, utilizations, marker='o', label=f"{benchmark_name}")
        plt.xlabel("CGRA Size")
        plt.ylabel("CGRA Utilization (%)")
        plt.title("CGRA Utilization vs CGRA Size (Default CGRA)")
        plt.xticks(range(self.plotter_config.compare_cgra_type_config.min_size, self.plotter_config.compare_cgra_type_config.max_size + 1))
        plt.legend()
        plt.tight_layout()
        plt.savefig(os.path.join(output_dir_path, "cgra_utilization_vs_cgra_size_default_cgra.png"))
        plt.clf()

    def plot_cgra_type_comparison(self):
        output_dir_path = os.path.join(self.output_dir_path, "cgra_type_comparison")
        check_dir_availability(output_dir_path)

        # {cgra_type: {cgra_size: [values]}}
        cgra_results_time = {}
        cgra_results_utilization = {}

        for remapper_result in self.remapper_results:
            valid_shape = remapper_result.cgra.row == remapper_result.cgra.column
            valid_size = remapper_result.cgra.row in range(self.plotter_config.compare_cgra_type_config.min_size, self.plotter_config.compare_cgra_type_config.max_size + 1)
            valid_context_size = remapper_result.cgra.context_size == self.plotter_config.compare_cgra_size_config.context_size
            valid_network_type = remapper_result.cgra.network_type == self.plotter_config.compare_database_timeout.network_type

            is_valid_result = valid_shape and valid_size and valid_context_size and valid_network_type
            if not is_valid_result:
                continue

            cgra_type = remapper_result.cgra.cgra_type
            cgra_size = remapper_result.cgra.row
            remapping_time = remapper_result.remapper_time
            utilization = remapper_result.utilization

            if cgra_type not in cgra_results_time:
                cgra_results_time[cgra_type] = {}
                cgra_results_utilization[cgra_type] = {}
            if cgra_size not in cgra_results_time[cgra_type]:
                cgra_results_time[cgra_type][cgra_size] = []
                cgra_results_utilization[cgra_type][cgra_size] = []
            cgra_results_time[cgra_type][cgra_size].append(remapping_time)
            cgra_results_utilization[cgra_type][cgra_size].append(utilization)

        plt.figure(figsize=(10, 5))
        for cgra_type, size_to_times in cgra_results_time.items():
            cgra_sizes = sorted(size_to_times.keys())
            means = [mean(size_to_times[cgra_size]) for cgra_size in cgra_sizes]
            plt.plot(cgra_sizes, means, marker='o', label=f"{cgra_type}")
        plt.xlabel("CGRA Size")
        if use_geometric_mean:
            plt.ylabel("Geometric Mean of Remapping Time (s)")
        else:
            plt.ylabel("Mean of Remapping Time (s)")
        plt.title("Remapping Time vs CGRA Size by CGRA Type")
        plt.xticks(range(self.plotter_config.compare_cgra_type_config.min_size, self.plotter_config.compare_cgra_type_config.max_size + 1))
        plt.legend()
        plt.tight_layout()
        plt.savefig(os.path.join(output_dir_path, "remapping_time_vs_cgra_size_by_cgra_type.png"))
        plt.clf()

        plt.figure(figsize=(10, 5))
        for cgra_type, size_to_utilizations in cgra_results_utilization.items():
            cgra_sizes = sorted(size_to_utilizations.keys())
            means = [mean(size_to_utilizations[cgra_size]) for cgra_size in cgra_sizes]
            plt.plot(cgra_sizes, means, marker='o', label=f"{cgra_type}")
        plt.xlabel("CGRA Size")
        if use_geometric_mean:
            plt.ylabel("Geometric Mean of CGRA Utilization (%)")
        else:
            plt.ylabel("Mean of CGRA Utilization (%)")
        plt.title("CGRA Utilization vs CGRA Size by CGRA Type")
        plt.xticks(range(self.plotter_config.compare_cgra_type_config.min_size, self.plotter_config.compare_cgra_type_config.max_size + 1))
        plt.legend()
        plt.tight_layout()
        plt.savefig(os.path.join(output_dir_path, "cgra_utilization_vs_cgra_size_by_cgra_type.png"))
        plt.clf()

    def plot_available_mapping_num_comparison(self):
        output_dir_path = os.path.join(self.output_dir_path, "available_mapping_num_comparison")
        check_dir_availability(output_dir_path)

        # {num_available_mappings: [values]}
        elastic_remapping_times_by_available_mapping_num = {}
        default_remapping_times_by_available_mapping_num = {}
        elastic_cgra_utilizations_by_available_mapping_num = {}
        default_cgra_utilizations_by_available_mapping_num = {}

        for remapper_result in self.remapper_results:
            valid_shape = remapper_result.cgra.row == remapper_result.cgra.column
            valid_size = self.plotter_config.compare_available_mapping_num_config.min_size <= remapper_result.cgra.row <= self.plotter_config.compare_available_mapping_num_config.max_size
            valid_context_size = remapper_result.cgra.context_size == self.plotter_config.compare_available_mapping_num_config.context_size
            valid_network_type = remapper_result.cgra.network_type == self.plotter_config.compare_available_mapping_num_config.network_type
            valid_num_available_mappings = remapper_result.num_available_mappings in self.plotter_config.compare_available_mapping_num_config.num_available_mappings

            is_valid_result = valid_shape and valid_size and valid_context_size and valid_network_type and valid_num_available_mappings
            if not is_valid_result:
                continue

            num_available_mappings = remapper_result.num_available_mappings
            remapping_time = remapper_result.remapper_time
            utilization = remapper_result.utilization

            is_elastic_cgra = remapper_result.cgra.cgra_type == CGRAType.Elastic
            if is_elastic_cgra:
                if num_available_mappings not in elastic_remapping_times_by_available_mapping_num:
                    elastic_remapping_times_by_available_mapping_num[num_available_mappings] = []
                    elastic_cgra_utilizations_by_available_mapping_num[num_available_mappings] = []
                elastic_remapping_times_by_available_mapping_num[num_available_mappings].append(remapping_time)
                elastic_cgra_utilizations_by_available_mapping_num[num_available_mappings].append(utilization)
            else:
                if num_available_mappings not in default_remapping_times_by_available_mapping_num:
                    default_remapping_times_by_available_mapping_num[num_available_mappings] = []
                    default_cgra_utilizations_by_available_mapping_num[num_available_mappings] = []
                default_remapping_times_by_available_mapping_num[num_available_mappings].append(remapping_time)
                default_cgra_utilizations_by_available_mapping_num[num_available_mappings].append(utilization)

        plt.figure(figsize=(10, 5))
        for num_available_mappings, times in elastic_remapping_times_by_available_mapping_num.items():
            mean_time = mean(times)
            label = "Elastic "
            if num_available_mappings == -1:
                label += "Available Mappings = All"
            else:
                label += f"Available Mappings = {num_available_mappings}"
            plt.bar(str(num_available_mappings), mean_time, label=label)
        for num_available_mappings, times in default_remapping_times_by_available_mapping_num.items():
            mean_time = mean(times)
            label = "Default "
            if num_available_mappings == -1:
                label += "Available Mappings = All"
            else:
                label += f"Available Mappings = {num_available_mappings}"
            plt.bar(str(num_available_mappings), mean_time, label=label)
        plt.xlabel("Number of Available Mappings")
        if use_geometric_mean:
            plt.ylabel("Geometric Mean of Remapping Time (s)")
        else:
            plt.ylabel("Mean of Remapping Time (s)")
        plt.title("Remapping Time vs Number of Available Mappings")
        plt.legend()
        plt.tight_layout()
        plt.savefig(os.path.join(output_dir_path, "remapping_time_vs_num_available_mappings.png"))
        plt.clf()

        plt.figure(figsize=(10, 5))
        for num_available_mappings, utilizations in elastic_cgra_utilizations_by_available_mapping_num.items():
            mean_utilization = mean(utilizations)
            label = "Elastic "
            if num_available_mappings == -1:
                label += "Available Mappings = All"
            else:
                label += f"Available Mappings = {num_available_mappings}"
            plt.bar(str(num_available_mappings), mean_utilization, label=label)
        for num_available_mappings, utilizations in default_cgra_utilizations_by_available_mapping_num.items():
            mean_utilization = mean(utilizations)
            label = "Default "
            if num_available_mappings == -1:
                label += "Available Mappings = All"
            else:
                label += f"Available Mappings = {num_available_mappings}"
            plt.bar(str(num_available_mappings), mean_utilization, label=label)
        plt.xlabel("Number of Available Mappings")
        if use_geometric_mean:
            plt.ylabel("Geometric Mean of CGRA Utilization (%)")
        else:
            plt.ylabel("Mean of CGRA Utilization (%)")
        plt.title("CGRA Utilization vs Number of Available Mappings")
        plt.legend()
        plt.tight_layout()
        plt.savefig(os.path.join(output_dir_path, "cgra_utilization_vs_num_available_mappings.png"))
        plt.clf()
