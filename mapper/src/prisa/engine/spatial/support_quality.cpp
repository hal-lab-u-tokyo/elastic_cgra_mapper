#include <mapper/prisa/engine/spatial/prisa_spatial_engine.hpp>

namespace mapper::prisa_spatial {

double Placement2DPRISAEngine::EdgePlacementCost(
    const DFGEdgeInfo& edge, const std::vector<int>& dfg_to_mrrg) const {
  return ResourcePairPlacementCost(dfg_to_mrrg[edge.source],
                                   dfg_to_mrrg[edge.target]);
}

std::optional<PlacementState> Placement2DPRISAEngine::RandomLegalPlacement() {
  PlacementState state;
  state.dfg_to_mrrg.assign(dfg_.GetNodeNum(), -1);
  state.mrrg_to_dfg.assign(mrrg_.GetNodeNum(), -1);
  std::vector<int> node_order(dfg_.GetNodeNum());
  std::iota(node_order.begin(), node_order.end(), 0);
  if (false || IsPRISALike()) {
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

}  // namespace mapper::prisa_spatial
