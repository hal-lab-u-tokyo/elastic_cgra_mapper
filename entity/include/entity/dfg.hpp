#pragma once

#include <entity/graph.hpp>

namespace entity {
enum OpType { ADD, SUB, MUL, DIV, CONST, LOAD, OUTPUT };
std::string OpTypeToString(OpType op);
OpType OpTypeFromString(std::string op_string);

struct DFGNodeProperty {
  OpType op;
  std::string op_str;
};

struct DFGEdgeProperty {
  int operand;
};

struct DFGGraphProperty {};

typedef BaseGraph<DFGNodeProperty, DFGEdgeProperty, DFGGraphProperty> DFGGraph;

class DFG : public BaseGraphClass<DFGNodeProperty, DFGEdgeProperty,
                                  DFGGraphProperty> {
 public:
  using BaseGraphClass<DFGNodeProperty, DFGEdgeProperty,
                       DFGGraphProperty>::BaseGraphClass;
  DFG(DFGGraph dfg_graph);
};
}  // namespace entity