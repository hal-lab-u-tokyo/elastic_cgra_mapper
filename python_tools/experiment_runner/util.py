import networkx as nx
import os

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
  cgra_str = ""
  cgra_str += cgra.column + "_"
  cgra_str += cgra.row + "_"
  cgra_str += str(cgra.context_size) + "_"
  cgra_str += cgra.memory_io_type.to_string() + "_"
  cgra_str += cgra.cgra_type.to_string()
  cgra_str += cgra.network_type.to_string() + "_"
  cgra_str += str(cgra.local_reg_size) +  "_"
  database_id = str(database_timeout_s)

  return database_id