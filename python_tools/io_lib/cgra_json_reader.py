import json
from entity import *

def cgra_json_reader(file_path):
  j = json.load(open(file_path))
  row = j["row"]
  column = j["column"]
  memory_io = MemoryIOType.get_from_string(j["memory_io"])
  cgra_type = CGRAType.get_from_string(j["CGRA_type"])
  network_type = NetworkType.get_from_string(j["network_type"])
  local_reg_size = j["local_reg_size"]
  context_size = j["context_size"]
  cgra = CGRA(cgra_type, row, column, context_size, memory_io, network_type, local_reg_size)

  return cgra
