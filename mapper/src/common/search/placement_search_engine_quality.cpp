#include "placement_search_engine_internal.hpp"

namespace mapper::detail::placement_search {

double PlacementSearchEngine::ResourcePairPlacementCost(int from_mrrg_node,
                                 int to_mrrg_node) const {
  if (from_mrrg_node < 0 || to_mrrg_node < 0) return kImpossibleCost;
  const int distance = DirectedResourceDistance(from_mrrg_node, to_mrrg_node);
  if (distance >= kInfDistance) return kImpossibleCost;
  return DirectedDistanceCost(distance);
}

double PlacementSearchEngine::EdgePlacementCost(const DFGEdgeInfo& edge,
                         const std::vector<int>& dfg_to_mrrg) const {
  return ResourcePairPlacementCost(dfg_to_mrrg[edge.source],
                                   dfg_to_mrrg[edge.target]);
}

double PlacementSearchEngine::PlacementCost(const PlacementState& state) const {
  double cost = 0.0;
  for (const auto& edge : dfg_edges_) {
    const double edge_cost = EdgePlacementCost(edge, state.dfg_to_mrrg);
    if (edge_cost >= kImpossibleCost) return kImpossibleCost;
    cost += edge_cost;
  }
  return cost;
}

bool PlacementSearchEngine::PRISAEdgeIsWeak(int source_resource, int target_resource,
                     int resource_count) const {
  if (source_resource < 0 || target_resource < 0) return false;
  const auto& resource_order = ResourceOrderIndex();
  if (source_resource >= static_cast<int>(resource_order.size()) ||
      target_resource >= static_cast<int>(resource_order.size())) {
    return false;
  }
  const int row = resource_order[source_resource];
  const int column = resource_order[target_resource];
  if (row < 0 || column < 0) return false;
  return IsPRISAWeakRegion(row, column, resource_count);
}

int PlacementSearchEngine::PRISAWeakEdgeCount(const PlacementState& state) const {
  const int resource_count = static_cast<int>(Placement2DResourceOrder().size());
  int weak_edges = 0;
  for (const auto& edge : dfg_edges_) {
    if (PRISAEdgeIsWeak(state.dfg_to_mrrg[edge.source],
                        state.dfg_to_mrrg[edge.target], resource_count)) {
      weak_edges++;
    }
  }
  return weak_edges;
}

std::optional<PlacementState> PlacementSearchEngine::RandomLegalPlacement() {
  PlacementState state;
  state.dfg_to_mrrg.assign(dfg_.GetNodeNum(), -1);
  state.mrrg_to_dfg.assign(mrrg_.GetNodeNum(), -1);
  std::vector<int> node_order(dfg_.GetNodeNum());
  std::iota(node_order.begin(), node_order.end(), 0);
  if (search_kind_ == PlacementSearchKind::kPlacement2DSA || IsPRISALike()) {
    std::shuffle(node_order.begin(), node_order.end(), rng_);
  } else {
    std::sort(node_order.begin(), node_order.end(), [&](int a, int b) {
      return annotation_.degree[a] > annotation_.degree[b];
    });
  }
  for (int dfg_node_id : node_order) {
    auto candidates = CompatibleResources(dfg_node_id, state);
    if (candidates.empty()) return std::nullopt;
    std::uniform_int_distribution<int> dist(0,
                                            static_cast<int>(candidates.size()) - 1);
    int chosen = candidates[dist(rng_)];
    state.dfg_to_mrrg[dfg_node_id] = chosen;
    state.mrrg_to_dfg[chosen] = dfg_node_id;
  }
  return state;
}

}  // namespace mapper::detail::placement_search
