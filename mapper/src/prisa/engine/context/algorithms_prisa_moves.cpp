#include <mapper/prisa/engine/context/prisa_context_engine.hpp>

namespace mapper::prisa_context {

// PRISA swap feasibility, deltas, and move proposals.

int ModuloPRISAEngine::EdgeMeshHop(const DFGEdgeInfo& edge,
                                   const std::vector<int>& dfg_to_mrrg) const {
  const int source_resource = dfg_to_mrrg[edge.source];
  const int target_resource = dfg_to_mrrg[edge.target];
  if (source_resource < 0 || target_resource < 0) return kInfDistance;
  return SpatialStepDistance(source_resource, target_resource);
}

ModuloPRISAEngine::CostAwarePRISAMetrics
ModuloPRISAEngine::ComputeCostAwarePRISAMetrics(
    const PlacementState& state) const {
  CostAwarePRISAMetrics metrics;
  const int node_count = dfg_.GetNodeNum();
  std::vector<int> indegree(node_count, 0);
  std::vector<std::vector<std::pair<int, int>>> weighted_successors(node_count);

  for (const auto& edge : dfg_edges_) {
    const int hop = EdgeMeshHop(edge, state.dfg_to_mrrg);
    if (hop >= kInfDistance) {
      metrics.mesh_hop_sum = kImpossibleCost;
      metrics.max_mesh_hop = kInfDistance;
      metrics.mapped_lp_mesh_hop = kInfDistance;
      return metrics;
    }
    metrics.mesh_hop_sum += hop;
    metrics.max_mesh_hop = std::max(metrics.max_mesh_hop, hop);
    if (hop <= 1) metrics.direct_edge_count++;
    weighted_successors[edge.source].push_back({edge.target, std::max(1, hop)});
    indegree[edge.target]++;
  }

  std::queue<int> q;
  std::vector<int> distance(node_count, 0);
  for (int node = 0; node < node_count; node++) {
    if (indegree[node] == 0) q.push(node);
  }

  int visited = 0;
  while (!q.empty()) {
    const int source = q.front();
    q.pop();
    visited++;
    metrics.mapped_lp_mesh_hop =
        std::max(metrics.mapped_lp_mesh_hop, distance[source]);
    for (const auto& [target, weight] : weighted_successors[source]) {
      distance[target] = std::max(distance[target], distance[source] + weight);
      indegree[target]--;
      if (indegree[target] == 0) q.push(target);
    }
  }

  if (visited != node_count) {
    metrics.mapped_lp_mesh_hop = static_cast<int>(metrics.mesh_hop_sum);
  } else {
    for (int value : distance) {
      metrics.mapped_lp_mesh_hop = std::max(metrics.mapped_lp_mesh_hop, value);
    }
  }
  return metrics;
}

double ModuloPRISAEngine::PRISAAcceptanceCost(
    const PlacementState& state) const {
  if (false) return CostAwarePRISAScore(state);
  return PlacementCost(state);
}

double ModuloPRISAEngine::PRISABestCost(const PlacementState& state) const {
  if (false) return CostAwarePRISAScore(state);
  return PlacementCost(state);
}

bool ModuloPRISAEngine::ApplyPRISAMove(PlacementState& state) {
  const auto& resources = Placement2DResourceOrder();
  if (resources.size() < 2) return false;
  {}
  const auto& resource_order = ResourceOrderIndex();
  const int resource_count = static_cast<int>(resources.size());
  std::vector<int> row_weak(resource_count, 0);
  std::vector<int> column_weak(resource_count, 0);
  std::vector<int> row_potential(resource_count, 0);
  std::vector<int> column_potential(resource_count, 0);
  std::vector<std::vector<int>> weak_columns_by_row(resource_count);
  std::vector<std::vector<int>> weak_rows_by_column(resource_count);

  for (const auto& edge : dfg_edges_) {
    const int source_resource = state.dfg_to_mrrg[edge.source];
    const int target_resource = state.dfg_to_mrrg[edge.target];
    if (source_resource < 0 || target_resource < 0) continue;
    const int row = resource_order[source_resource];
    const int column = resource_order[target_resource];
    if (row < 0 || column < 0) continue;
    if (IsPRISAWeakRegion(row, column, resource_count)) {
      row_weak[row]++;
      column_weak[column]++;
      weak_columns_by_row[row].push_back(column);
      weak_rows_by_column[column].push_back(row);
    } else if (IsPRISAPotentialRegion(row, column, resource_count)) {
      row_potential[row]++;
      column_potential[column]++;
    }
  }

  auto random_argmax = [&](const std::vector<int>& values) {
    const int best = *std::max_element(values.begin(), values.end());
    std::vector<int> indices;
    for (int i = 0; i < static_cast<int>(values.size()); i++) {
      if (values[i] == best) indices.push_back(i);
    }
    std::uniform_int_distribution<int> dist(
        0, static_cast<int>(indices.size()) - 1);
    return indices[dist(rng_)];
  };
  const int row_index = random_argmax(row_weak);
  const int column_index = random_argmax(column_weak);
  const int row_max = row_weak[row_index];
  const int column_max = column_weak[column_index];
  if (row_max == 0) {
    return ApplyPlacementCostSampledSwap(state, resources);
  }

  struct PRISACandidate {
    int first_index = -1;
    int second_index = -1;
    int weak_count = 0;
    int weak_delta = 0;
    int fixed_weak_count = 0;
    int potential_count = 0;
    double cost_delta = 0.0;
    int spatial_distance = 0;
    int random_order = 0;
  };

  auto best_candidate_for =
      [&](int first_index, int weak_count,
          bool row_side) -> std::optional<PRISACandidate> {
    const auto& weak_counterparts = row_side ? weak_columns_by_row[first_index]
                                             : weak_rows_by_column[first_index];
    if (weak_counterparts.empty()) return std::nullopt;
    std::vector<int> candidate_indices(resource_count);
    std::iota(candidate_indices.begin(), candidate_indices.end(), 0);
    std::shuffle(candidate_indices.begin(), candidate_indices.end(), rng_);

    const int first_resource = resources[first_index];
    std::vector<PRISACandidate> candidates;
    candidates.reserve(candidate_indices.size());
    for (int order = 0; order < static_cast<int>(candidate_indices.size());
         order++) {
      const int second_index = candidate_indices[order];
      if (second_index == first_index) continue;
      const int second_resource = resources[second_index];
      if (!CanSwapResources(state, first_resource, second_resource)) continue;
      int fixed_weak_count = 0;
      for (int counterpart : weak_counterparts) {
        const bool moves_to_pr =
            row_side ? IsPRISAPotentialRegion(second_index, counterpart,
                                              resource_count)
                     : IsPRISAPotentialRegion(counterpart, second_index,
                                              resource_count);
        if (moves_to_pr) fixed_weak_count++;
      }
      if (fixed_weak_count == 0) continue;
      candidates.push_back(PRISACandidate{
          first_index, second_index, weak_count,
          PRISAWeakEdgeDeltaForSwap(state, first_resource, second_resource),
          fixed_weak_count,
          row_side ? row_potential[second_index]
                   : column_potential[second_index],
          PlacementCostDeltaForSwap(state, first_resource, second_resource),
          SpatialStepDistance(first_resource, second_resource), order});
    }
    if (candidates.empty()) return std::nullopt;
    std::sort(candidates.begin(), candidates.end(),
              [](const PRISACandidate& a, const PRISACandidate& b) {
                // PRISA's second exchange part is selected from the
                // potential region with the fewest existing non-zero
                // elements. This keeps already compact rows/columns stable
                // while moving the selected weak entries toward PR.
                if (a.potential_count != b.potential_count) {
                  return a.potential_count < b.potential_count;
                }
                if (a.fixed_weak_count != b.fixed_weak_count) {
                  return a.fixed_weak_count > b.fixed_weak_count;
                }
                if (a.weak_delta != b.weak_delta) {
                  return a.weak_delta < b.weak_delta;
                }
                if (a.cost_delta != b.cost_delta) {
                  return a.cost_delta < b.cost_delta;
                }
                if (a.spatial_distance != b.spatial_distance) {
                  return a.spatial_distance < b.spatial_distance;
                }
                return a.random_order < b.random_order;
              });
    return candidates.front();
  };

  std::vector<PRISACandidate> candidates;
  const bool prefer_row = row_max > column_max ||
                          (row_max == column_max &&
                           std::uniform_int_distribution<int>(0, 1)(rng_) == 0);
  auto push_row_candidate = [&]() {
    if (row_max <= 0) return;
    auto candidate = best_candidate_for(row_index, row_max, true);
    if (candidate.has_value()) candidates.push_back(*candidate);
  };
  auto push_column_candidate = [&]() {
    if (column_max <= 0) return;
    auto candidate = best_candidate_for(column_index, column_max, false);
    if (candidate.has_value()) candidates.push_back(*candidate);
  };
  if (prefer_row) {
    push_row_candidate();
    if (candidates.empty()) push_column_candidate();
  } else {
    push_column_candidate();
    if (candidates.empty()) push_row_candidate();
  }
  if (candidates.empty()) {
    return ApplyPlacementCostSampledSwap(state, resources);
  }
  const auto& candidate = candidates.front();
  if (ApplyResourceSwap(state, resources[candidate.first_index],
                        resources[candidate.second_index])) {
    return true;
  }
  return ApplyPlacementCostSampledSwap(state, resources);
}

}  // namespace mapper::prisa_context
