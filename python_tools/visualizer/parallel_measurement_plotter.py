import sys
import os
sys.path.append(os.pardir)

from io_lib import * 
import re
import networkx as nx
from matplotlib import pyplot as plt

def estimate_parallel_num(mapping_file_path, dfg_node_num):
  max_op_index = 0
  mapping = read_mapping_from_json(mapping_file_path)
  for row_id in range(mapping.row_num):
    for column_id in range(mapping.column_num):
      pe = mapping.PE_array[row_id][column_id]
      for config in pe.config_list:
        op_index = re.sub(r"\D", "", config.operation_name)
        if op_index != "":
          max_op_index = max(int(op_index), max_op_index)
  return (max_op_index + 1) // dfg_node_num


if __name__ == "__main__":
  args = sys.argv

  mapping_dir_path = args[1]
  log_dir_path = args[2]

  dfg_file_path = log_dir_path + "/dfg_1.dot"
  dfg = nx.nx_pydot.read_dot(dfg_file_path)
  dfg_node_num = len(dfg)
  if "\\n" in dfg.nodes:
    dfg_node_num = dfg_node_num - 1
œ
  files = os.listdir(mapping_dir_path)
  
  time_vec = [[], []] # [memory_type][parallel_num]
  time_vec_parallel_num = [[], []] # [memory_type][parallel_num]
  perf_vec = [[], []] # [memory_type][parallel_num]
  perf_vec_parallel_num = [[], []] # [memory_type][parallel_num]

  for mapping_file_name in files:
    mapping_file_path = mapping_dir_path + "/" + mapping_file_name
    id = re.sub(r"\D", "", mapping_file_name)

    parallel_num = estimate_parallel_num(mapping_file_path, dfg_node_num)

    log_file_path = log_dir_path + "/" + "log" + id + ".log"
    mapping_log_info = mapping_log_reader(log_file_path)

    time_vec[int(mapping_log_info.memory_io)].append(mapping_log_info.mapping_time)
    time_vec_parallel_num[int(mapping_log_info.memory_io)].append(str(parallel_num))
    perf_vec[int(mapping_log_info.memory_io)].append(mapping_log_info.context_size / parallel_num)
    perf_vec_parallel_num[int(mapping_log_info.memory_io)].append(str(parallel_num))

  if not os.path.exists("./output/parallel_measurement/"):
    os.makedirs("./output/parallel_measurement/")

  fig, ax = plt.subplots()
  ax.set_xlabel("parallel num")
  ax.set_ylabel("time [s]")
  ax.bar(time_vec_parallel_num[0], time_vec[0])
  fig.savefig("./output/parallel_measurement/time_mamory_io_all.pdf")

  fig, ax = plt.subplots()
  ax.set_xlabel("parallel num")
  ax.set_ylabel("time [s]")
  ax.bar(time_vec_parallel_num[1], time_vec[1])
  fig.savefig("./output/parallel_measurement/time_mamory_io_bothends.pdf")

  fig, ax = plt.subplots()
  ax.set_xlabel("parallel num")
  ax.set_ylabel("II / parallel_num")
  ax.bar(perf_vec_parallel_num[0], perf_vec[0])
  fig.savefig("./output/parallel_measurement/perf_mamory_io_all.pdf")

  fig, ax = plt.subplots()
  ax.set_xlabel("parallel num")
  ax.set_ylabel("II / parallel_num")
  ax.bar(perf_vec_parallel_num[1], perf_vec[1])
  fig.savefig("./output/parallel_measurement/perf_mamory_io_bothends.pdf")




