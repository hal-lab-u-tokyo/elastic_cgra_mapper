#include <mapper/prisa/engine/spatial/prisa_spatial_engine.hpp>

namespace mapper::prisa_spatial {

// DFG traversal roots, order, and YOTT annotations.

int Placement2DPRISAEngine::OtherEndpoint(const DFGEdgeInfo& edge,
                                          int node_id) const {
  if (edge.source == node_id) return edge.target;
  if (edge.target == node_id) return edge.source;
  return -1;
}

}  // namespace mapper::prisa_spatial
