#pragma once

#include <entity/dfg.hpp>
#include <entity/graph.hpp>

namespace entity {
struct MRRGNodeProperty {
  bool is_elastic;
  bool is_memory_accessible;
  int local_reg_size;
  int context_size;
  std::pair<int, int> position;
  std::vector<OpType> support_op;
};

struct MRRGEdgeProperty {};

struct MRRGGraphProperty {};

typedef BaseGraph<MRRGNodeProperty, MRRGEdgeProperty, MRRGGraphProperty>
    MRRGGraph;

class MRRG : public BaseGraphClass<MRRGNodeProperty, MRRGEdgeProperty,
                                   MRRGGraphProperty> {
 public:
  using BaseGraphClass<MRRGNodeProperty, MRRGEdgeProperty,
                       MRRGGraphProperty>::BaseGraphClass;
  MRRG(MRRGGraph mrrg_graph);
};
}  // namespace entity