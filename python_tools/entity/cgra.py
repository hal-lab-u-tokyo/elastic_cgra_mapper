import json

class CGRA:
  def __init__(self, cgra_type, row, column, context_size, memory_io_type, network_type, local_reg_size):
    self.row = row
    self.column = column
    self.memory_io_type = memory_io_type
    self.cgra_type = cgra_type
    self.network_type = network_type
    self.local_reg_size = local_reg_size
    self.context_size = context_size

  def __eq__(self, other):
    if self.row != other.row:
      return False
    if self.column != other.column:
      return False
    if self.memory_io_type != other.memory_io_type:
      return False
    if self.cgra_type != other.cgra_type:
      return False
    if self.network_type != other.network_type:
      return False
    if self.local_reg_size != other.local_reg_size:
      return False
    if self.context_size != other.context_size:
      return False
    return True


  def get_cgra_dict(self):
    cgra_dict = {}
    cgra_dict["row"] = self.row
    cgra_dict["column"] = self.column
    cgra_dict["memory_io"] = self.memory_io_type.to_string()
    cgra_dict["CGRA_type"] = self.cgra_type.to_string()
    cgra_dict["network_type"] = self.network_type.to_string()
    cgra_dict["local_reg_size"] = self.local_reg_size
    cgra_dict["context_size"] = self.context_size
    return cgra_dict

  def dump_to_json(self, file_path):
    cgra_dict = self.get_cgra_dict()

    file = open(file_path, mode="w")
    json.dump(cgra_dict, file)
    file.close()