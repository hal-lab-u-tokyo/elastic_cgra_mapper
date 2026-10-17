#include <mapper/prisa/engine/spatial/prisa_spatial_engine.hpp>

namespace mapper::prisa_spatial {

// PRISA resource ordering, weak regions, and initial placement.

const std::vector<int>& Placement2DPRISAEngine::Placement2DResourceOrder()
    const {
  if (IsPRISALike() && !prisa_resources_.empty()) return prisa_resources_;
  return placement2d_resources_;
}

const std::vector<int>& Placement2DPRISAEngine::ResourceOrderIndex() const {
  if (IsPRISALike() && !prisa_resource_order_index_.empty()) {
    return prisa_resource_order_index_;
  }
  return placement2d_resource_order_index_;
}

std::optional<PlacementState>
Placement2DPRISAEngine::ConstructPRISAInitialPlacementFromOrder(
    const std::vector<int>& node_order) {
  const auto& resources = Placement2DResourceOrder();
  if (resources.empty()) return std::nullopt;
  PlacementState state;
  state.dfg_to_mrrg.assign(dfg_.GetNodeNum(), -1);
  state.mrrg_to_dfg.assign(mrrg_.GetNodeNum(), -1);
  for (int label = 0; label < static_cast<int>(node_order.size()); label++) {
    const int node = node_order[label];
    int best_resource = -1;
    int best_distance = kInfDistance;
    for (int order_index = 0; order_index < static_cast<int>(resources.size());
         order_index++) {
      const int r = resources[order_index];
      if (!CanOccupyResource(node, r, state)) continue;
      const int label_distance = std::abs(order_index - label);
      if (label_distance < best_distance) {
        best_distance = label_distance;
        best_resource = r;
      }
    }
    if (best_resource < 0) return std::nullopt;
    state.dfg_to_mrrg[node] = best_resource;
    state.mrrg_to_dfg[best_resource] = node;
  }
  return state;
}

std::optional<PlacementState>
Placement2DPRISAEngine::ConstructPRISAInitialPlacement() {
  if (!UsesPRISASIS()) return RandomLegalPlacement();
  std::optional<PlacementState> best;
  double best_cost = std::numeric_limits<double>::infinity();
  const int sample_count = PRISAInitialSolutionSampleCount();
  for (int sample = 0; sample < sample_count; sample++) {
    auto placement =
        ConstructPRISAInitialPlacementFromOrder(LowBandwidthNodeOrder());
    if (!placement.has_value()) continue;
    const double cost = PlacementCost(*placement);
    if (cost < best_cost) {
      best = placement;
      best_cost = cost;
    }
  }
  return best;
}

}  // namespace mapper::prisa_spatial
