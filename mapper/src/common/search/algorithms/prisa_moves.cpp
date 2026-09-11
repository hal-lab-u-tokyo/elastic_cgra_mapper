#include "../engine_internal.hpp"

namespace mapper::detail::placement_search {

// PRISA swap feasibility, deltas, and move proposals.

bool PlacementSearchEngine::CanSwapResources(const PlacementState& state,
                                             int ra, int rb) const {
  if (ra == rb || ra < 0 || rb < 0) return false;
  const int a = state.mrrg_to_dfg[ra];
  const int b = state.mrrg_to_dfg[rb];
  if (a < 0 && b < 0) return false;
  if (a >= 0 && !IsCompatible(a, rb)) return false;
  if (b >= 0 && !IsCompatible(b, ra)) return false;
  return true;
}

bool PlacementSearchEngine::ApplyResourceSwap(PlacementState& state, int ra,
                                              int rb) const {
  if (!CanSwapResources(state, ra, rb)) return false;
  const int a = state.mrrg_to_dfg[ra];
  const int b = state.mrrg_to_dfg[rb];
  state.mrrg_to_dfg[ra] = b;
  state.mrrg_to_dfg[rb] = a;
  if (a >= 0) state.dfg_to_mrrg[a] = rb;
  if (b >= 0) state.dfg_to_mrrg[b] = ra;
  return true;
}

bool PlacementSearchEngine::ApplyRandomResourceSwap(
    PlacementState& state, const std::vector<int>& resources) {
  if (resources.size() < 2) return false;
  std::uniform_int_distribution<int> dist(
      0, static_cast<int>(resources.size()) - 1);
  for (int attempt = 0; attempt < 32; attempt++) {
    const int ra = resources[dist(rng_)];
    const int rb = resources[dist(rng_)];
    if (ApplyResourceSwap(state, ra, rb)) return true;
  }
  return false;
}

bool PlacementSearchEngine::ApplyPlacementCostSampledSwap(
    PlacementState& state, const std::vector<int>& resources) {
  if (resources.size() < 2) return false;

  struct Candidate {
    double cost_delta = std::numeric_limits<double>::infinity();
    int first_resource = -1;
    int second_resource = -1;
    int random_order = 0;
  };

  const int resource_count = static_cast<int>(resources.size());
  const int total_pairs = resource_count * (resource_count - 1) / 2;
  const int sample_count =
      std::min(total_pairs, std::max(16, resource_count * 4));

  auto normalize_pair = [](int lhs, int rhs) {
    if (lhs > rhs) std::swap(lhs, rhs);
    return std::pair<int, int>(lhs, rhs);
  };

  std::set<std::pair<int, int>> sampled_pairs;
  if (sample_count == total_pairs) {
    for (int i = 0; i < resource_count; i++) {
      for (int j = i + 1; j < resource_count; j++) {
        sampled_pairs.insert({i, j});
      }
    }
  } else {
    std::uniform_int_distribution<int> resource_dist(0, resource_count - 1);
    int attempts = 0;
    while (static_cast<int>(sampled_pairs.size()) < sample_count &&
           attempts < sample_count * 16) {
      attempts++;
      const int first = resource_dist(rng_);
      const int second = resource_dist(rng_);
      if (first == second) continue;
      sampled_pairs.insert(normalize_pair(first, second));
    }
  }

  std::vector<Candidate> candidates;
  candidates.reserve(sampled_pairs.size());
  int random_order = 0;
  for (const auto& [first_index, second_index] : sampled_pairs) {
    const int first_resource = resources[first_index];
    const int second_resource = resources[second_index];
    if (!CanSwapResources(state, first_resource, second_resource)) continue;
    candidates.push_back(Candidate{
        PlacementCostDeltaForSwap(state, first_resource, second_resource),
        first_resource, second_resource, random_order++});
  }
  if (candidates.empty()) return ApplyRandomResourceSwap(state, resources);

  std::shuffle(candidates.begin(), candidates.end(), rng_);
  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate& a, const Candidate& b) {
              if (a.cost_delta != b.cost_delta) {
                return a.cost_delta < b.cost_delta;
              }
              return a.random_order < b.random_order;
            });

  int selection_count = 1;
  while (selection_count < static_cast<int>(candidates.size()) &&
         candidates[selection_count].cost_delta ==
             candidates.front().cost_delta) {
    selection_count++;
  }
  std::uniform_int_distribution<int> dist(0, selection_count - 1);
  const Candidate& chosen = candidates[dist(rng_)];
  return ApplyResourceSwap(state, chosen.first_resource,
                           chosen.second_resource);
}

int PlacementSearchEngine::ResourceAfterSwap(
    int dfg_node_id, int first_resource, int second_resource,
    const PlacementState& state) const {
  int resource = state.dfg_to_mrrg[dfg_node_id];
  if (resource == first_resource) return second_resource;
  if (resource == second_resource) return first_resource;
  return resource;
}

double PlacementSearchEngine::PlacementCostDeltaForSwap(
    const PlacementState& state, int first_resource,
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

  double before = 0.0;
  double after = 0.0;
  for (int edge_id : affected_edges) {
    const auto& edge = dfg_edges_[edge_id];
    before += EdgePlacementCost(edge, state.dfg_to_mrrg);
    after += ResourcePairPlacementCost(
        ResourceAfterSwap(edge.source, first_resource, second_resource, state),
        ResourceAfterSwap(edge.target, first_resource, second_resource, state));
  }
  return after - before;
}

int PlacementSearchEngine::PRISAWeakEdgeDeltaForSwap(
    const PlacementState& state, int first_resource,
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

  const int resource_count =
      static_cast<int>(Placement2DResourceOrder().size());
  int before = 0;
  int after = 0;
  for (int edge_id : affected_edges) {
    const auto& edge = dfg_edges_[edge_id];
    before += PRISAEdgeIsWeak(state.dfg_to_mrrg[edge.source],
                              state.dfg_to_mrrg[edge.target], resource_count)
                  ? 1
                  : 0;
    after += PRISAEdgeIsWeak(ResourceAfterSwap(edge.source, first_resource,
                                               second_resource, state),
                             ResourceAfterSwap(edge.target, first_resource,
                                               second_resource, state),
                             resource_count)
                 ? 1
                 : 0;
  }
  return after - before;
}

int PlacementSearchEngine::EdgeMeshHop(
    const DFGEdgeInfo& edge, const std::vector<int>& dfg_to_mrrg) const {
  const int source_resource = dfg_to_mrrg[edge.source];
  const int target_resource = dfg_to_mrrg[edge.target];
  if (source_resource < 0 || target_resource < 0) return kInfDistance;
  return SpatialStepDistance(source_resource, target_resource);
}

PlacementSearchEngine::CostAwarePRISAMetrics
PlacementSearchEngine::ComputeCostAwarePRISAMetrics(
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

double PlacementSearchEngine::CostAwarePRISAScore(
    const PlacementState& state) const {
  const int weak_edges = PRISAWeakEdgeCount(state);
  const CostAwarePRISAMetrics metrics = ComputeCostAwarePRISAMetrics(state);
  if (metrics.mesh_hop_sum >= kImpossibleCost ||
      metrics.max_mesh_hop >= kInfDistance ||
      metrics.mapped_lp_mesh_hop >= kInfDistance) {
    return kImpossibleCost;
  }
  // The strict PRISA WR test is intentionally coarse. This derived mapper
  // keeps WR as the first-stage objective, then optimizes the placement-only
  // paper metrics directly: mapped LP, max FIFO/hop, and total mesh hop.
  return weak_edges * 1000000.0 + metrics.mesh_hop_sum * 50.0 +
         metrics.mapped_lp_mesh_hop * 100.0 + metrics.max_mesh_hop * 200.0 -
         metrics.direct_edge_count * 80.0;
}

double PlacementSearchEngine::PRISAAcceptanceCost(
    const PlacementState& state) const {
  if (UsesCostAwarePRISA()) return CostAwarePRISAScore(state);
  return PlacementCost(state);
}

double PlacementSearchEngine::PRISABestCost(const PlacementState& state) const {
  if (UsesCostAwarePRISA()) return CostAwarePRISAScore(state);
  return PlacementCost(state);
}

int PlacementSearchEngine::CostAwarePRISACandidateSampleCount() const {
  if (options_.max_trials.has_value()) {
    return std::max(16, options_.max_trials.value());
  }
  return 512;
}

int PlacementSearchEngine::CostAwarePRISAFullEvaluationCount() const {
  return 8;
}

double PlacementSearchEngine::CostAwarePRISALocalSwapScore(
    const PlacementState& state, int first_resource,
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

  double mesh_delta = 0.0;
  int after_max_hop = 0;
  int direct_edge_gain = 0;
  for (int edge_id : affected_edges) {
    const auto& edge = dfg_edges_[edge_id];
    const int before = EdgeMeshHop(edge, state.dfg_to_mrrg);
    const int after = SpatialStepDistance(
        ResourceAfterSwap(edge.source, first_resource, second_resource, state),
        ResourceAfterSwap(edge.target, first_resource, second_resource, state));
    if (before >= kInfDistance || after >= kInfDistance) {
      return kImpossibleCost;
    }
    mesh_delta += after - before;
    after_max_hop = std::max(after_max_hop, after);
    if (before <= 1 && after > 1) direct_edge_gain--;
    if (before > 1 && after <= 1) direct_edge_gain++;
  }

  const int weak_delta =
      PRISAWeakEdgeDeltaForSwap(state, first_resource, second_resource);
  return weak_delta * 1000000.0 + mesh_delta * 100.0 + after_max_hop * 20.0 -
         direct_edge_gain * 80.0;
}

bool PlacementSearchEngine::ApplyCostAwarePRISAMove(PlacementState& state) {
  const auto& resources = Placement2DResourceOrder();
  if (resources.size() < 2) return false;

  struct Candidate {
    double score = std::numeric_limits<double>::infinity();
    int first_resource = -1;
    int second_resource = -1;
    int random_order = 0;
  };

  const int resource_count = static_cast<int>(resources.size());
  const int total_pairs = resource_count * (resource_count - 1) / 2;
  const int sample_count =
      std::min(total_pairs, CostAwarePRISACandidateSampleCount());

  auto normalize_pair = [](int lhs, int rhs) {
    if (lhs > rhs) std::swap(lhs, rhs);
    return std::pair<int, int>(lhs, rhs);
  };

  std::set<std::pair<int, int>> sampled_pairs;
  if (sample_count == total_pairs) {
    for (int i = 0; i < resource_count; i++) {
      for (int j = i + 1; j < resource_count; j++) {
        sampled_pairs.insert({i, j});
      }
    }
  } else {
    std::vector<std::pair<int, int>> edge_hops;
    edge_hops.reserve(dfg_edges_.size());
    for (const auto& edge : dfg_edges_) {
      edge_hops.push_back({EdgeMeshHop(edge, state.dfg_to_mrrg), edge.id});
    }
    std::sort(edge_hops.begin(), edge_hops.end(),
              std::greater<std::pair<int, int>>());
    const int focused_budget = std::max(1, sample_count / 2);
    const int focused_edge_count =
        std::min(4, static_cast<int>(edge_hops.size()));
    for (int idx = 0; idx < focused_edge_count; idx++) {
      const auto& edge = dfg_edges_[edge_hops[idx].second];
      const int endpoints[] = {state.dfg_to_mrrg[edge.source],
                               state.dfg_to_mrrg[edge.target]};
      for (int endpoint_resource : endpoints) {
        const int endpoint_index = ResourceOrderIndex()[endpoint_resource];
        if (endpoint_index < 0) continue;
        for (int other = 0;
             other < resource_count &&
             static_cast<int>(sampled_pairs.size()) < focused_budget;
             other++) {
          if (other == endpoint_index) continue;
          sampled_pairs.insert(normalize_pair(endpoint_index, other));
        }
      }
    }

    std::uniform_int_distribution<int> resource_dist(0, resource_count - 1);
    int attempts = 0;
    while (static_cast<int>(sampled_pairs.size()) < sample_count &&
           attempts < sample_count * 16) {
      attempts++;
      const int first = resource_dist(rng_);
      const int second = resource_dist(rng_);
      if (first == second) continue;
      sampled_pairs.insert(normalize_pair(first, second));
    }
  }

  std::vector<Candidate> candidates;
  candidates.reserve(sampled_pairs.size());
  int random_order = 0;
  for (const auto& [first_index, second_index] : sampled_pairs) {
    const int first_resource = resources[first_index];
    const int second_resource = resources[second_index];
    if (!CanSwapResources(state, first_resource, second_resource)) continue;
    candidates.push_back(Candidate{
        CostAwarePRISALocalSwapScore(state, first_resource, second_resource),
        first_resource, second_resource, random_order++});
  }
  if (candidates.empty()) return ApplyRandomResourceSwap(state, resources);

  std::shuffle(candidates.begin(), candidates.end(), rng_);
  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate& a, const Candidate& b) {
              if (a.score != b.score) return a.score < b.score;
              return a.random_order < b.random_order;
            });
  const int full_evaluation_count = std::min(
      CostAwarePRISAFullEvaluationCount(), static_cast<int>(candidates.size()));
  const double current_score = CostAwarePRISAScore(state);
  for (int i = 0; i < full_evaluation_count; i++) {
    PlacementState next = state;
    if (!ApplyResourceSwap(next, candidates[i].first_resource,
                           candidates[i].second_resource)) {
      candidates[i].score = kImpossibleCost;
      continue;
    }
    candidates[i].score = CostAwarePRISAScore(next);
  }
  candidates.resize(full_evaluation_count);
  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate& a, const Candidate& b) {
              if (a.score != b.score) return a.score < b.score;
              return a.random_order < b.random_order;
            });

  int selection_count = 1;
  if (candidates.front().score >= current_score) {
    selection_count = std::min(4, static_cast<int>(candidates.size()));
  } else {
    while (selection_count < static_cast<int>(candidates.size()) &&
           candidates[selection_count].score == candidates.front().score) {
      selection_count++;
    }
  }
  std::uniform_int_distribution<int> dist(0, selection_count - 1);
  const Candidate& chosen = candidates[dist(rng_)];
  return ApplyResourceSwap(state, chosen.first_resource,
                           chosen.second_resource);
}

bool PlacementSearchEngine::ApplyPRISAMove(PlacementState& state) {
  const auto& resources = Placement2DResourceOrder();
  if (resources.size() < 2) return false;
  if (UsesCostAwarePRISA()) {
    return ApplyCostAwarePRISAMove(state);
  }
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

}  // namespace mapper::detail::placement_search
