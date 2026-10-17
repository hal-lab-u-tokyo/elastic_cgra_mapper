#include "engine.hpp"

namespace mapper::detail::yott_core_array {

double Placement2DYOTTCoreEngine::PlacementCost(
    const PlacementState& state) const {
  double cost = 0.0;
  for (const auto& edge : edges_) {
    const int from = state.dfg_to_cell[edge.source];
    const int to = state.dfg_to_cell[edge.target];
    if (from < 0 || to < 0) return kImpossibleCost;
    cost += DistanceCost(from, to);
  }
  return cost;
}

void Placement2DYOTTCoreEngine::PlaceNode(int dfg_node, int cell,
                                          PlacementState& state) const {
  state.dfg_to_cell[dfg_node] = cell;
  state.cell_to_dfg[cell] = dfg_node;
}

}  // namespace mapper::detail::yott_core_array
