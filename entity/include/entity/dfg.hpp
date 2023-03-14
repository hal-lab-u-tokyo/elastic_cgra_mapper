#pragma once

#include <entity/graph.hpp>
#include <entity/operation.hpp>

namespace entity {
struct DFGNodeProperty {
  int id;
  OpType op;
  std::string op_str;
  std::string op_name;
  int const_value;
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