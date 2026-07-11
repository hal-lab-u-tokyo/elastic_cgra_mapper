#include "../engine_internal.hpp"

namespace mapper::detail::placement_search {

// Cost-aware PRISA cleanup passes.

int PlacementSearchEngine::CostAwarePRISAPolishPasses() const {
  return std::max(4, std::min(12, PRISAMaxIterations() / 125));
}

PlacementState PlacementSearchEngine::PolishCostAwarePRISA(
    PlacementState state, std::chrono::steady_clock::time_point start) {
  const auto& resources = Placement2DResourceOrder();
  if (resources.size() < 2) return state;

  struct Candidate {
    double local_score = std::numeric_limits<double>::infinity();
    int first_resource = -1;
    int second_resource = -1;
  };

  double current_score = CostAwarePRISAScore(state);
  for (int pass = 0;
       pass < CostAwarePRISAPolishPasses() && !HasTimedOut(start, 0.05);
       pass++) {
    std::vector<Candidate> candidates;
    const int resource_count = static_cast<int>(resources.size());
    candidates.reserve(resource_count * resource_count / 2);
    for (int i = 0; i < resource_count; i++) {
      for (int j = i + 1; j < resource_count; j++) {
        const int first_resource = resources[i];
        const int second_resource = resources[j];
        if (!CanSwapResources(state, first_resource, second_resource)) {
          continue;
        }
        candidates.push_back(
            Candidate{CostAwarePRISALocalSwapScore(state, first_resource,
                                                   second_resource),
                      first_resource, second_resource});
      }
    }
    if (candidates.empty()) break;

    const int reevaluate_count =
        std::min(64, static_cast<int>(candidates.size()));
    if (reevaluate_count < static_cast<int>(candidates.size())) {
      std::nth_element(candidates.begin(),
                       candidates.begin() + reevaluate_count, candidates.end(),
                       [](const Candidate& a, const Candidate& b) {
                         return a.local_score < b.local_score;
                       });
    }
    candidates.resize(reevaluate_count);

    bool improved = false;
    PlacementState best_state = state;
    double best_score = current_score;
    for (const auto& candidate : candidates) {
      PlacementState next = state;
      if (!ApplyResourceSwap(next, candidate.first_resource,
                             candidate.second_resource)) {
        continue;
      }
      const double next_score = CostAwarePRISAScore(next);
      if (next_score + 1.0e-9 < best_score) {
        best_score = next_score;
        best_state = std::move(next);
        improved = true;
      }
    }
    if (!improved) break;
    state = std::move(best_state);
    current_score = best_score;
  }
  return state;
}

int PlacementSearchEngine::DirectEdgeGainForSwap(const PlacementState& state,
                                                 int first_resource,
                                                 int second_resource) const {
  std::vector<int> affected_edges;
  const int first_node = state.mrrg_to_dfg[first_resource];
  const int second_node = state.mrrg_to_dfg[second_resource];
  if (first_node >= 0) {
    affected_edges.insert(affected_edges.end(),
                          incident_edge_ids_[first_node].begin(),
                          incident_edge_ids_[first_node].end());
  }
  if (second_node >= 0) {
    affected_edges.insert(affected_edges.end(),
                          incident_edge_ids_[second_node].begin(),
                          incident_edge_ids_[second_node].end());
  }
  std::sort(affected_edges.begin(), affected_edges.end());
  affected_edges.erase(
      std::unique(affected_edges.begin(), affected_edges.end()),
      affected_edges.end());

  int gain = 0;
  for (int edge_id : affected_edges) {
    const auto& edge = dfg_edges_[edge_id];
    const int before = EdgeMeshHop(edge, state.dfg_to_mrrg);
    const int after = SpatialStepDistance(
        ResourceAfterSwap(edge.source, first_resource, second_resource, state),
        ResourceAfterSwap(edge.target, first_resource, second_resource, state));
    if (before <= 1 && after > 1) gain--;
    if (before > 1 && after <= 1) gain++;
  }
  return gain;
}

PlacementState PlacementSearchEngine::PolishCostAwarePRISADirectEdges(
    PlacementState state, std::chrono::steady_clock::time_point start) {
  const auto& resources = Placement2DResourceOrder();
  if (resources.size() < 2) return state;

  struct Candidate {
    int direct_gain = 0;
    double local_score = std::numeric_limits<double>::infinity();
    int first_resource = -1;
    int second_resource = -1;
  };

  CostAwarePRISAMetrics current = ComputeCostAwarePRISAMetrics(state);
  for (int pass = 0;
       pass < CostAwarePRISAPolishPasses() && !HasTimedOut(start, 0.05);
       pass++) {
    std::vector<Candidate> candidates;
    const int resource_count = static_cast<int>(resources.size());
    candidates.reserve(resource_count * resource_count / 2);
    for (int i = 0; i < resource_count; i++) {
      for (int j = i + 1; j < resource_count; j++) {
        const int first_resource = resources[i];
        const int second_resource = resources[j];
        if (!CanSwapResources(state, first_resource, second_resource)) {
          continue;
        }
        const int direct_gain =
            DirectEdgeGainForSwap(state, first_resource, second_resource);
        if (direct_gain <= 0) continue;
        candidates.push_back(
            Candidate{direct_gain,
                      CostAwarePRISALocalSwapScore(state, first_resource,
                                                   second_resource),
                      first_resource, second_resource});
      }
    }
    if (candidates.empty()) break;

    const int reevaluate_count =
        std::min(256, static_cast<int>(candidates.size()));
    if (reevaluate_count < static_cast<int>(candidates.size())) {
      std::nth_element(candidates.begin(),
                       candidates.begin() + reevaluate_count, candidates.end(),
                       [](const Candidate& a, const Candidate& b) {
                         if (a.direct_gain != b.direct_gain) {
                           return a.direct_gain > b.direct_gain;
                         }
                         return a.local_score < b.local_score;
                       });
    }
    candidates.resize(reevaluate_count);

    bool improved = false;
    PlacementState best_state = state;
    CostAwarePRISAMetrics best_metrics = current;
    for (const auto& candidate : candidates) {
      PlacementState next = state;
      if (!ApplyResourceSwap(next, candidate.first_resource,
                             candidate.second_resource)) {
        continue;
      }
      const CostAwarePRISAMetrics next_metrics =
          ComputeCostAwarePRISAMetrics(next);
      if (next_metrics.direct_edge_count <= best_metrics.direct_edge_count) {
        continue;
      }
      if (next_metrics.max_mesh_hop > current.max_mesh_hop) continue;
      if (next_metrics.mapped_lp_mesh_hop > current.mapped_lp_mesh_hop) {
        continue;
      }
      if (next_metrics.mesh_hop_sum > current.mesh_hop_sum + 2.0 + 1.0e-9) {
        continue;
      }
      if (next_metrics.direct_edge_count > best_metrics.direct_edge_count ||
          (next_metrics.direct_edge_count == best_metrics.direct_edge_count &&
           next_metrics.mesh_hop_sum < best_metrics.mesh_hop_sum)) {
        best_state = std::move(next);
        best_metrics = next_metrics;
        improved = true;
      }
    }
    if (!improved) break;
    state = std::move(best_state);
    current = best_metrics;
  }
  return state;
}

}  // namespace mapper::detail::placement_search
