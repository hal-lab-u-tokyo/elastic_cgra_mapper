import networkx as nx
import sys
import re
from dfg import DFGNode, DFGEdge, DFG

def fix_op_str(input):
  if input == "store":
    return "output"
  elif input == "fsub":
    return "sub"
  elif input == "fmul":
    return "mul"
  elif input == "getelementptr":
    return "load"
  elif input == "phi":
    return "const"
  elif input == "fadd":
    return "add"
  return input

if __name__ == "__main__":
  args = sys.argv
  if len(args) <= 2:
    raise ValueError
  input_file = args[1]
  output_file = args[2]

  G = nx.Graph(nx.nx_pydot.read_dot(input_file))

  dfg = DFG()

  for v in G.nodes():
    idx = re.findall('[0-9]+', v)[0]
    op_str = fix_op_str(re.findall('[a-z]+$', v)[0])
    op_name = op_str + idx
    const = 0
    tmp_node = DFGNode(idx, op_name, op_str, op_name, const)
    dfg.addNode(tmp_node)

  idx_to_input_num = {}
  for edge in G.edges():
    operand = 0
    input_idx = re.findall('[0-9]+', edge[0])[0]
    output_idx = re.findall('[0-9]+', edge[1])[0]
    if output_idx in idx_to_input_num.keys():
      operand = idx_to_input_num[output_idx]
      idx_to_input_num[output_idx] = operand + 1
      if operand > 1:
        print("Warning: operand > 1 exists")
    else:
      idx_to_input_num[output_idx] = 1

    tmp_edge = DFGEdge(input_idx, output_idx, operand)
    dfg.addEdge(tmp_edge)

  dfg.output(output_file)
