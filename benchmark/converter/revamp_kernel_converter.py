import sys
import xml.etree.ElementTree as ET
from dfg import DFGNode, DFGEdge, DFG

def fix_op_str(input):
  if input == "ADD":
    return "add"
  elif input == "LOADB" or input == "LS" or input == "LOAD" or input == "OLOAD":
    return "load"
  elif input == "MUL":
    return "mul"
  elif input == "STORE" or input == "STOREB" or input == "OSTORE":
    return "output"

  return input


if __name__ == "__main__":
  args = sys.argv
  if len(args) <= 2:
    raise ValueError
  input_file = args[1]
  output_file = args[2]

  tree = ET.parse(input_file)
  root = tree.getroot()

  idx_to_node_map = {}
  edge_tuple_list = []

  dfg = DFG()

  for item in root.findall("./DFG/Node"):
    idx = item.attrib["idx"]
    const = None
    if "CONST" in item.attrib.keys():
      const = item.attrib["CONST"]
    for child in item:
      if child.tag == "OP":
        op_str = fix_op_str(child.text)
        op_name = op_str + idx
      if child.tag == "Outputs":
        for output in child.findall("./Output"):
          edge_tuple_list.append((idx, output.attrib["idx"]))
    tmp_node = DFGNode(idx, op_name, op_str, op_name, const)
    idx_to_node_map[idx] = tmp_node

    dfg.addNode(tmp_node)

  idx_to_input_num = {}
  for edge in edge_tuple_list:
    operand = 0
    if edge[1] in idx_to_input_num.keys():
      operand = idx_to_input_num[edge[1]]
      idx_to_input_num[edge[1]] = operand + 1
      if operand > 1:
        print("Warning: operand > 1 exists")
    else:
      idx_to_input_num[edge[1]] = 1

    tmp_edge = DFGEdge(edge[0], edge[1], operand)
    dfg.addEdge(tmp_edge)

  dfg.output(output_file)
