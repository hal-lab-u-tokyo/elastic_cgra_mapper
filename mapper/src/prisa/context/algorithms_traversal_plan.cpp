#include "engine.hpp"

namespace mapper::detail::prisa_context {

// DFG traversal roots, order, and YOTT annotations.

int ModuloPRISAEngine::OtherEndpoint(const DFGEdgeInfo& edge,
                                     int node_id) const {
  if (edge.source == node_id) return edge.target;
  if (edge.target == node_id) return edge.source;
  return -1;
}

}  // namespace mapper::detail::prisa_context
