from matplotlib import pyplot as plt
import networkx as nx
import os
import re
import json
import parse
from mapping_log_reader import mapping_log_reader

# param
benchmark_list = ["fixed_convolution2d", "fixed_ellpack", "fixed_fft_pro", "fixed_fir_pro", "fixed_latnrm_pro", "fixed_stencil", "fixed_susan_pro", "convolution_no_loop", "fixed_matrixmultiply_const"]
benchmark_list_name = ["conv2d", "ellpack", "fft", "fir", "latnrm", "stencil", "susan", "conv1d", "matmul"]

kernel_dir_path = "../benchmark/kernel/"
remapper_dir_path = "../output/remapper/20231205/"
memory_io_list = ["all", "both_ends"]
database_dir_path = "../output/utilization_comparison/20231205/"
loop_unrolling_date = "20231205"
target_cgra_row = 6
target_cgra_column = 6
target_cgra_config_size = 4
target_cgra_network_type = "orthogonal"

def get_remapper_log(log_file_path):
  parallel_num = 0
  remapper_time = 0
  with open(log_file_path) as f:
    for line in f:
      parsed = parse.parse(
              "total {:d} parallel remapping time: {:f}\n", line)
      if parsed != None:
        parallel_num = parsed[0]
        remapper_time = remapper_time + parsed[1]
  return parallel_num, remapper_time
        
  
def get_unrolling_log(log_file_path):
  parallel_num = 0
  mapping_time = 0
  with open(log_file_path) as f:
    for line in f:
      parsed = parse.parse(
              "parallel num: {:d}\n", line)
      if parsed != None:
        parallel_num = parsed[0]
        continue
      
      parsed = parse.parse(
                "Explored {:d} nodes ({:d} simplex iterations) in {:f} seconds\n", line)
      if parsed != None:
          mapping_time = parsed[2]
  
  return parallel_num, mapping_time


def create_cgra_id(mapping_file_path):
  f = open(mapping_file_path, "r")
  mapping = json.load(f)
  return mapping["column"] + "_" + mapping["row"] + "_" + mapping["context_size"] + "_" + mapping["memory_io_type"] + "_" + mapping["cgra_type"] + "_" + mapping["network_type"]

def get_all_context_num(mapping_file_path):
  f = open(mapping_file_path, "r")
  mapping = json.load(f)
  return int(mapping["column"]) *  int(mapping["row"]) * int(mapping["context_size"])

if __name__ == "__main__": 
  benchmark_node_num = {}
  dp_parallel_num = {}
  greedy_parallel_num = {}
  loop_unrolling_parallel_num = {}
  dp_time = {}
  greedy_time = {}
  loop_unrolling_time = {}
  dp_util = {}
  greedy_util = {}
  loop_unrolling_util = {}

  database_time = {}

  for benchmark in benchmark_list:
    dfg_file_path = kernel_dir_path + benchmark + ".dot"
    G = nx.Graph(nx.nx_pydot.read_dot(dfg_file_path))
    dfg_node_size = len(G.nodes())

    benchmark_node_num[benchmark] = dfg_node_size

    # database_time
    benchmark_database_dir_path = database_dir_path + benchmark + "/database/"
    for file in os.listdir(benchmark_database_dir_path + "mapping/"):
      unix_time_str = re.findall(r"\d+", file)[0]
      log_file_path = benchmark_database_dir_path + "log/log" + unix_time_str + ".log"
      mapping_log = mapping_log_reader(log_file_path)
      if benchmark not in database_time.keys():
        database_time[benchmark] = 0
      database_time[benchmark] = database_time[benchmark] + mapping_log.mapping_time


    # dp
    log_dir_path = remapper_dir_path + benchmark +  "/dp/log/"
    mapping_dir_path = remapper_dir_path + benchmark +  "/dp/mapping/"
    for file in os.listdir(mapping_dir_path):
      exp_time = result = re.findall(r"\d+", file)[0]
      result_id = create_cgra_id(mapping_dir_path + file) + "_" + benchmark
      log_file_path = log_dir_path + "log" + exp_time + "_mode2.log"
      parallel_num, remapper_time = get_remapper_log(log_file_path)

      dp_parallel_num[result_id] = parallel_num
      dp_time[result_id] = remapper_time + database_time[benchmark]
      dp_util[result_id] = parallel_num * dfg_node_size / get_all_context_num(mapping_dir_path + file)

    # greedy
    log_dir_path = remapper_dir_path + benchmark +  "/naive/log/"
    mapping_dir_path = remapper_dir_path + benchmark +  "/naive/mapping/"
    for file in os.listdir(mapping_dir_path):
      exp_time = result = re.findall(r"\d+", file)[0]
      result_id = create_cgra_id(mapping_dir_path + file) + "_" + benchmark
      log_file_path = log_dir_path + "log" + exp_time + "_mode1.log"
      parallel_num, remapper_time = get_remapper_log(log_file_path)

      greedy_parallel_num[result_id] = parallel_num
      greedy_time[result_id] = remapper_time + database_time[benchmark]
      greedy_util[result_id] = parallel_num * dfg_node_size / get_all_context_num(mapping_dir_path + file)

    # loop unrolling
    log_dir_path = "../output/log/" + benchmark +  "/"+ loop_unrolling_date +"/"
    mapping_dir_path = "../output/mapping/" + benchmark +  "/"+ loop_unrolling_date +"/"
    if not os.path.exists(log_dir_path):
      continue

    for file in os.listdir(mapping_dir_path):
      exp_time = result = re.findall(r"\d+", file)[0]
      result_id = create_cgra_id(mapping_dir_path + file) + "_" + benchmark
      log_file_path = log_dir_path + "log_" + exp_time + ".log"
      parallel_num, mapping_time = get_unrolling_log(log_file_path)
      
      if result_id in loop_unrolling_parallel_num.keys() and loop_unrolling_parallel_num[result_id] > parallel_num:
        continue
      loop_unrolling_parallel_num[result_id] = parallel_num
      loop_unrolling_time[result_id] = mapping_time
      loop_unrolling_util[result_id] = parallel_num * dfg_node_size / get_all_context_num(mapping_dir_path + file)


  for memory_io in memory_io_list:
    dp_utilization = []
    greedy_utilization = []
    loop_unrolling_utilization = []
    dp_time_list = []
    greedy_time_list = []
    loop_unrolling_time_list = []

    for benchmark in benchmark_list:
      result_id = target_cgra_row + "_" + target_cgra_column + "_" + target_cgra_config_size + memory_io + "_elastic_" + target_cgra_network_type + benchmark
      
      if result_id in dp_time.keys():
        dp_utilization.append(dp_util[result_id])
        dp_time_list.append(dp_time[result_id])
      else:
        dp_utilization.append(0)
        dp_time_list.append(0)

      if result_id in greedy_time.keys():
        greedy_utilization.append(greedy_util[result_id])
        greedy_time_list.append(greedy_time[result_id])
      else:
        greedy_utilization.append(0)
        greedy_time_list.append(0)

      if result_id in loop_unrolling_time.keys():
        tmp_util = loop_unrolling_util[result_id]
        if tmp_util == 0:
          tmp_util = 1
        loop_unrolling_utilization.append(tmp_util)
        loop_unrolling_time_list.append(loop_unrolling_time[result_id])
      else:
        loop_unrolling_utilization.append(1)
        loop_unrolling_time_list.append(3600)

    for i in range(0, len(benchmark_list) - 1):
      dp_utilization[i] = dp_utilization[i] / loop_unrolling_utilization[i]
      greedy_utilization[i] = greedy_utilization[i] / loop_unrolling_utilization[i]

      dp_time_list[i] = dp_time_list[i] / loop_unrolling_time_list[i]
      greedy_time_list[i] = greedy_time_list[i] / loop_unrolling_time_list[i]

    fig, ax = plt.subplots()  
    ax.bar(benchmark_list_name, dp_utilization, align="edge", width=-0.3, label="dp")
    ax.bar(benchmark_list_name, greedy_utilization, align="edge", width=0.3, label="greedy")
    ax.set_xlabel("benchmark")
    ax.set_ylabel("utilization rate")
    fig.savefig("./output/utilization_comparison/" + memory_io + "_util.png")

    fig, ax = plt.subplots()
    ax.bar(benchmark_list_name, dp_time_list, align="edge", width=-0.3, label="dp")
    ax.bar(benchmark_list_name, greedy_time_list, align="edge", width=0.3, label="greedy")
    ax.set_xlabel("benchmark")
    ax.set_ylabel("time")
    fig.savefig("./output/utilization_comparison/" + memory_io + "_time.png")








