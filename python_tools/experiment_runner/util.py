import networkx as nx
import os
import sys
sys.path.append(os.pardir)
from io_lib import *

def check_dir_availability(dir_path):
  if not os.path.exists(dir_path):
    os.makedirs(dir_path)

def get_dfg_node_size(dfg_file):
  G = nx.Graph(nx.nx_pydot.read_dot(dfg_file))

  dfg_node_size = len(G.nodes())

  for node in G.nodes():
    if len(node) == 2: # NOTE: skip "\n" node
      dfg_node_size = dfg_node_size - 1

  return dfg_node_size

def get_database_id(cgra, database_timeout_s):
  database_id = ""
  database_id += str(cgra.column) + "_"
  database_id += str(cgra.row) + "_"
  database_id += str(cgra.context_size) + "_"
  database_id += cgra.memory_io_type.to_string() + "_"
  database_id += cgra.cgra_type.to_string() + "_"
  database_id += cgra.network_type.to_string() + "_"
  database_id += str(cgra.local_reg_size) +  "_"
  database_id += str(database_timeout_s)

  return database_id

def remapping_type_to_int(remapper_type):
  if remapper_type == RemapperType.FullSearch:
    return 0
  elif remapper_type == RemapperType.Greedy:
    return 1
  elif remapper_type == RemapperType.DP:
    return 2
  elif remapper_type == RemapperType.DPAndFullSearch:
    return 3
  else:
    raise ValueError("invalid remapper type")
