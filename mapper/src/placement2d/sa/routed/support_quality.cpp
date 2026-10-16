#include "engine.hpp"

namespace mapper::detail::placement2d_sa_routed {

double Placement2DSAEngine::ResourcePairPlacementCost(int from_mrrg_node,
                                                      int to_mrrg_node) const {
  if (from_mrrg_node < 0 || to_mrrg_node < 0) return kImpossibleCost;
  const int distance = DirectedResourceDistance(from_mrrg_node, to_mrrg_node);
  if (distance >= kInfDistance) return kImpossibleCost;
  return DirectedDistanceCost(distance);
}

double Placement2DSAEngine::EdgePlacementCost(
    const DFGEdgeInfo& edge, const std::vector<int>& dfg_to_mrrg) const {
  return ResourcePairPlacementCost(dfg_to_mrrg[edge.source],
                                   dfg_to_mrrg[edge.target]);
}

double Placement2DSAEngine::PlacementCost(const PlacementState& state) const {
  double cost = 0.0;
  for (const auto& edge : dfg_edges_) {
    const double edge_cost = EdgePlacementCost(edge, state.dfg_to_mrrg);
    if (edge_cost >= kImpossibleCost) return kImpossibleCost;
    cost += edge_cost;
  }
  return cost;
}

std::optional<PlacementState> Placement2DSAEngine::RandomLegalPlacement() {
  PlacementState state;
  state.dfg_to_mrrg.assign(dfg_.GetNodeNum(), -1);
  state.mrrg_to_dfg.assign(mrrg_.GetNodeNum(), -1);
  std::vector<int> node_order(dfg_.GetNodeNum());
  std::iota(node_order.begin(), node_order.end(), 0);
  if (true || false) {
    std::shuffle(node_order.begin(), node_order.end(), rng_);
  } else {
    std::sort(node_order.begin(), node_order.end(), [&](int a, int b) {
      return annotation_.degree[a] > annotation_.degree[b];
    });
  }
  for (int dfg_node_id : node_order) {
    auto candidates = CompatibleResources(dfg_node_id, state);
    if (candidates.empty()) return std::nullopt;
    std::uniform_int_distribution<int> dist(
        0, static_cast<int>(candidates.size()) - 1);
    int chosen = candidates[dist(rng_)];
    state.dfg_to_mrrg[dfg_node_id] = chosen;
    state.mrrg_to_dfg[chosen] = dfg_node_id;
  }
  return state;
}

}  // namespace mapper::detail::placement2d_sa_routed
