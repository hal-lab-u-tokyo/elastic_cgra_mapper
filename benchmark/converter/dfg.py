from typing import List

class DFGNode:
  def __init__(self, id, op, op_str, op_name, const_value = None):
    self.id = id
    self.op = op
    self.op_str = op_str
    self.op_name = op_name
    self.const_value = const_value

class DFGEdge:
  def __init__(self, from_id, to_id, operand):
    self.from_id = from_id
    self.to_id = to_id
    self.operand = operand

class DFG:
  def __init__(self):
    self.node_list: List[DFGNode] = []
    self.edge_list: List[DFGEdge] = []
    self.node_id_to_node_map = {}


  def addNode(self, node: DFGNode):
    self.node_list.append(node)
    self.node_id_to_node_map[node.id] = node

  def addEdge(self, edge: DFGEdge):
    self.edge_list.append(edge)

  def output(self, file_path):
    f = open(file_path, "w")
    f.write("digraph G {\n")

    for node in self.node_list:
      line = node.op_name + "[op_code=" + node.op_str
      if node.op_str == "const":
        line = line + ", const_value=\" " + str(node.const_value) + "\""
      line = line + "];"
      f.write(line + "\n")

    for edge in self.edge_list:
      from_node: DFGNode = self.node_id_to_node_map[edge.from_id]
      to_node: DFGNode = self.node_id_to_node_map[edge.to_id]

      line = from_node.op_name + "->" + to_node.op_name + "[operand=" + str(edge.operand) + "]; //" + from_node.op_str + "->" + to_node.op_str
      f.write(line + "\n")

    f.write("}\n")

    f.close()