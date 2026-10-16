#include "engine.hpp"

namespace mapper::detail::yoto_context {

double ModuloYOTOEngine::ResourcePairPlacementCost(int from_mrrg_node,
                                                   int to_mrrg_node) const {
  if (from_mrrg_node < 0 || to_mrrg_node < 0) return kImpossibleCost;
  const int distance = DirectedResourceDistance(from_mrrg_node, to_mrrg_node);
  if (distance >= kInfDistance) return kImpossibleCost;
  return DirectedDistanceCost(distance);
}

double ModuloYOTOEngine::EdgePlacementCost(
    const DFGEdgeInfo& edge, const std::vector<int>& dfg_to_mrrg) const {
  return ResourcePairPlacementCost(dfg_to_mrrg[edge.source],
                                   dfg_to_mrrg[edge.target]);
}

double ModuloYOTOEngine::PlacementCost(const PlacementState& state) const {
  double cost = 0.0;
  for (const auto& edge : dfg_edges_) {
    const double edge_cost = EdgePlacementCost(edge, state.dfg_to_mrrg);
    if (edge_cost >= kImpossibleCost) return kImpossibleCost;
    cost += edge_cost;
  }
  return cost;
}

}  // namespace mapper::detail::yoto_context
