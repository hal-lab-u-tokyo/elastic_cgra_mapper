#include <mapper/prisa/engine/prisa_engine.hpp>

namespace mapper::prisa {

std::optional<PlacementState> PrisaEngine::RunPRISAMultiSeed(
    std::chrono::steady_clock::time_point start) {
  std::optional<PlacementState> best_routeable;
  double best_routeable_cost = std::numeric_limits<double>::infinity();
  std::optional<PlacementState> best_unrouted;
  double best_unrouted_cost = std::numeric_limits<double>::infinity();
  int attempted_seeds = 0;

  for (int seed_index = 0; seed_index < SeedCount() && !HasTimedOut(start);
       seed_index++) {
    ResetSeed(seed_index);
    attempted_seeds++;
    auto placement = RunPRISA(start);
    if (!placement.has_value()) continue;
    const double cost = PlacementCost(*placement);
    if (cost < best_unrouted_cost) {
      best_unrouted = placement;
      best_unrouted_cost = cost;
    }
    if (options_.placement_only) continue;
    if (TryRoutePlacement(*placement).has_value() &&
        cost < best_routeable_cost) {
      best_routeable = placement;
      best_routeable_cost = cost;
    }
  }
  Log("prisa_search seeds=" + std::to_string(attempted_seeds) +
      " sis=" + (UsesPRISASIS() ? "1" : "0") +
      " routeable=" + (best_routeable.has_value() ? "1" : "0"));
  if (options_.placement_only) return best_unrouted;
  if (best_routeable.has_value()) return best_routeable;
  return best_unrouted;
}

std::optional<PlacementState> PrisaEngine::RunPRISA(
    std::chrono::steady_clock::time_point start) {
  std::optional<PlacementState> current = ConstructPRISAInitialPlacement();
  if (!current.has_value()) current = RandomLegalPlacement();
  if (!current.has_value()) return std::nullopt;

  PlacementState best = *current;
  double current_cost = PRISAAcceptanceCost(*current);
  double best_cost = PRISABestCost(*current);
  const double initial_cost = PlacementCost(*current);
  const double initial_acceptance_cost = current_cost;
  const double initial_best_cost = best_cost;
  const int initial_weak_edges = PRISAWeakEdgeCount(*current);
  std::vector<PlacementState> elite;
  elite.push_back(best);

  const int max_iterations = PRISAMaxIterations();
  const double start_probability = UsesPRISASIS() ? 0.20 : 0.40;
  const double end_probability = 0.01;
  double temperature = -1.0 / std::log(start_probability);
  const double final_temperature = -1.0 / std::log(end_probability);
  const double cooling = std::pow(final_temperature / temperature,
                                  1.0 / std::max(1, max_iterations));
  std::uniform_real_distribution<double> unit(0.0, 1.0);
  int generated_moves = 0;
  int accepted_moves = 0;
  int improving_moves = 0;
  int completed_iterations = 0;

  for (int iteration = 0;
       iteration < max_iterations && !HasTimedOut(start, 0.05); iteration++) {
    completed_iterations = iteration + 1;
    PlacementState next = *current;
    if (!ApplyPRISAMove(next)) continue;
    generated_moves++;
    const double next_cost = PRISAAcceptanceCost(next);
    const double delta = next_cost - current_cost;
    const bool accept =
        delta <= 0.0 || unit(rng_) < std::exp(-delta / temperature);
    if (accept) {
      *current = next;
      current_cost = next_cost;
      accepted_moves++;
    }
    const double next_best_cost = PRISABestCost(next);
    if (next_best_cost < best_cost) {
      best = next;
      best_cost = next_best_cost;
      improving_moves++;
      elite.push_back(best);
      if (elite.size() > 16) elite.erase(elite.begin());
    }
    temperature = std::max(final_temperature, temperature * cooling);
  }

  if (UsesCostAwarePRISA()) {
    const double before_polish_cost = best_cost;
    best = PolishCostAwarePRISA(best, start);
    best = PolishCostAwarePRISADirectEdges(best, start);
    best_cost = PRISABestCost(best);
    if (best_cost + 1.0e-9 < before_polish_cost) {
      improving_moves++;
    }
  }

  Log("prisa_result sis=" + std::to_string(UsesPRISASIS() ? 1 : 0) +
      " iterations=" + std::to_string(completed_iterations) +
      " generated_moves=" + std::to_string(generated_moves) +
      " accepted_moves=" + std::to_string(accepted_moves) +
      " improving_moves=" + std::to_string(improving_moves) +
      " initial_cost=" + std::to_string(initial_cost) +
      " best_cost=" + std::to_string(PlacementCost(best)) +
      " initial_acceptance_cost=" + std::to_string(initial_acceptance_cost) +
      " initial_best_cost=" + std::to_string(initial_best_cost) +
      " best_acceptance_cost=" + std::to_string(PRISAAcceptanceCost(best)) +
      " best_selection_cost=" + std::to_string(best_cost) +
      " initial_weak_edges=" + std::to_string(initial_weak_edges) +
      " best_weak_edges=" + std::to_string(PRISAWeakEdgeCount(best)));

  std::sort(elite.begin(), elite.end(),
            [&](const PlacementState& a, const PlacementState& b) {
              return PRISAAcceptanceCost(a) < PRISAAcceptanceCost(b);
            });
  if (options_.placement_only) return best;
  for (const auto& candidate : elite) {
    auto finalized = FinalizePhysicalPlacementIfNeeded(candidate);
    if (!finalized.has_value()) continue;
    if (TryRoutePlacement(*finalized).has_value()) return *finalized;
    if (HasTimedOut(start, 0.01)) break;
  }
  return FinalizePhysicalPlacementIfNeeded(best);
}

// PRISA resource ordering, weak regions, and initial placement.

const std::vector<int>& PrisaEngine::Placement2DResourceOrder() const {
  if (IsPRISALike() && !prisa_resources_.empty()) return prisa_resources_;
  return placement2d_resources_;
}

const std::vector<int>& PrisaEngine::ResourceOrderIndex() const {
  if (IsPRISALike() && !prisa_resource_order_index_.empty()) {
    return prisa_resource_order_index_;
  }
  return placement2d_resource_order_index_;
}

std::optional<PlacementState>
PrisaEngine::ConstructPRISAInitialPlacementFromOrder(
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

std::optional<PlacementState> PrisaEngine::ConstructPRISAInitialPlacement() {
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

// PRISA swap feasibility, deltas, and move proposals.

int PrisaEngine::EdgeMeshHop(const DFGEdgeInfo& edge,
                             const std::vector<int>& dfg_to_mrrg) const {
  const int source_resource = dfg_to_mrrg[edge.source];
  const int target_resource = dfg_to_mrrg[edge.target];
  if (source_resource < 0 || target_resource < 0) return kInfDistance;
  return SpatialStepDistance(source_resource, target_resource);
}

PrisaEngine::CostAwarePRISAMetrics PrisaEngine::ComputeCostAwarePRISAMetrics(
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

double PrisaEngine::PRISAAcceptanceCost(const PlacementState& state) const {
  if (UsesCostAwarePRISA()) return CostAwarePRISAScore(state);
  return PlacementCost(state);
}

double PrisaEngine::PRISABestCost(const PlacementState& state) const {
  if (UsesCostAwarePRISA()) return CostAwarePRISAScore(state);
  return PlacementCost(state);
}

bool PrisaEngine::ApplyPRISAMove(PlacementState& state) {
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

// DFG traversal roots, order, and YOTT annotations.

int PrisaEngine::OtherEndpoint(const DFGEdgeInfo& edge, int node_id) const {
  if (edge.source == node_id) return edge.target;
  if (edge.target == node_id) return edge.source;
  return -1;
}

// Physical-placement to modulo-context assignment.

std::optional<int> PrisaEngine::FindSamePhysicalResourceAtContext(
    int base_resource, int dfg_node_id, int desired_context,
    const PlacementState& assigned) const {
  const int context_size = std::max(1, mrrg_.GetMRRGConfig().context_size);
  const auto base_position = mrrg_.GetNodeProperty(base_resource).position_id;
  for (int delta = 0; delta < context_size; delta++) {
    const int context_id = (desired_context + delta) % context_size;
    for (int resource = 0; resource < mrrg_.GetNodeNum(); resource++) {
      if (assigned.mrrg_to_dfg[resource] != -1) continue;
      const auto property = mrrg_.GetNodeProperty(resource);
      if (property.position_id != base_position) continue;
      if (property.context_id != context_id) continue;
      if (!IsCompatible(dfg_node_id, resource)) continue;
      return resource;
    }
  }
  return std::nullopt;
}

std::optional<PlacementState> PrisaEngine::AssignContextsToPhysicalPlacement(
    const PlacementState& physical_placement) const {
  if (!IsPhysicalThenContextLike()) return physical_placement;

  const int context_size = std::max(1, mrrg_.GetMRRGConfig().context_size);
  if (context_size == 1) return physical_placement;

  const std::vector<int> levels = BuildASAPContextLevels();
  PlacementState assigned;
  assigned.dfg_to_mrrg.assign(dfg_.GetNodeNum(), -1);
  assigned.mrrg_to_dfg.assign(mrrg_.GetNodeNum(), -1);

  for (int dfg_node_id = 0; dfg_node_id < dfg_.GetNodeNum(); dfg_node_id++) {
    const int base_resource = physical_placement.dfg_to_mrrg[dfg_node_id];
    if (base_resource < 0) return std::nullopt;
    const int desired_context = levels[dfg_node_id] % context_size;
    auto resource = FindSamePhysicalResourceAtContext(
        base_resource, dfg_node_id, desired_context, assigned);
    if (!resource.has_value()) return std::nullopt;
    assigned.dfg_to_mrrg[dfg_node_id] = resource.value();
    assigned.mrrg_to_dfg[resource.value()] = dfg_node_id;
  }

  return assigned;
}

std::optional<PlacementState> PrisaEngine::FinalizePhysicalPlacementIfNeeded(
    const PlacementState& placement) const {
  if (!IsPhysicalThenContextLike()) return placement;
  return AssignContextsToPhysicalPlacement(placement);
}

double PrisaEngine::EdgePlacementCost(
    const DFGEdgeInfo& edge, const std::vector<int>& dfg_to_mrrg) const {
  return ResourcePairPlacementCost(dfg_to_mrrg[edge.source],
                                   dfg_to_mrrg[edge.target]);
}

std::optional<PlacementState> PrisaEngine::RandomLegalPlacement() {
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

int PrisaEngine::CostAwarePRISACandidateSampleCount() const {
  if (options_.max_trials.has_value()) {
    return std::max(16, options_.max_trials.value());
  }
  return 512;
}

int PrisaEngine::CostAwarePRISAFullEvaluationCount() const { return 8; }

double PrisaEngine::CostAwarePRISALocalSwapScore(const PlacementState& state,
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

bool PrisaEngine::ApplyCostAwarePRISAMove(PlacementState& state) {
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

// Cost-aware PRISA cleanup passes.

int PrisaEngine::CostAwarePRISAPolishPasses() const {
  return std::max(4, std::min(12, PRISAMaxIterations() / 125));
}

PlacementState PrisaEngine::PolishCostAwarePRISA(
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

int PrisaEngine::DirectEdgeGainForSwap(const PlacementState& state,
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

PlacementState PrisaEngine::PolishCostAwarePRISADirectEdges(
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

// Initial placement and SIS order.

std::optional<PlacementState> PrisaEngine::GridRandomPlacement() {
  PlacementState state;
  state.dfg_to_cell.assign(dfg_.GetNodeNum(), -1);
  state.cell_to_dfg.assign(rows_ * cols_, -1);
  std::vector<int> nodes(dfg_.GetNodeNum());
  std::iota(nodes.begin(), nodes.end(), 0);
  std::shuffle(nodes.begin(), nodes.end(), rng_);
  std::stable_sort(nodes.begin(), nodes.end(), [&](int a, int b) {
    if (GridIsIONode(a) != GridIsIONode(b))
      return GridIsIONode(a) > GridIsIONode(b);
    return degree_[a] > degree_[b];
  });
  for (int node : nodes) {
    auto cells = GridCompatibleCells(node, state);
    if (cells.empty()) return std::nullopt;
    std::uniform_int_distribution<int> dist(0,
                                            static_cast<int>(cells.size()) - 1);
    GridPlaceNode(node, cells[dist(rng_)], state);
  }
  return state;
}

int PrisaEngine::GridOtherEndpoint(int edge_id, int node) const {
  const auto& edge = edges_[edge_id];
  if (edge.source == node) return edge.target;
  if (edge.target == node) return edge.source;
  return -1;
}

std::vector<int> PrisaEngine::GridLowBandwidthNodeOrder() {
  const int n = dfg_.GetNodeNum();
  if (n == 0) return {};

  auto bfs_distance = [&](int root,
                          const std::vector<char>* allowed = nullptr) {
    std::vector<int> distance(n, -1);
    std::queue<int> q;
    distance[root] = 0;
    q.push(root);
    while (!q.empty()) {
      const int u = q.front();
      q.pop();
      for (int edge_id : incident_edge_ids_[u]) {
        const int v = GridOtherEndpoint(edge_id, u);
        if (v < 0 || distance[v] >= 0) continue;
        if (allowed != nullptr && !(*allowed)[v]) continue;
        distance[v] = distance[u] + 1;
        q.push(v);
      }
    }
    return distance;
  };

  std::vector<char> visited(n, 0);
  std::vector<std::vector<int>> components;
  for (int root = 0; root < n; root++) {
    if (visited[root]) continue;
    std::vector<int> component;
    std::queue<int> q;
    visited[root] = 1;
    q.push(root);
    while (!q.empty()) {
      const int u = q.front();
      q.pop();
      component.push_back(u);
      for (int edge_id : incident_edge_ids_[u]) {
        const int v = GridOtherEndpoint(edge_id, u);
        if (v < 0 || visited[v]) continue;
        visited[v] = 1;
        q.push(v);
      }
    }
    components.push_back(std::move(component));
  }

  std::sort(components.begin(), components.end(),
            [&](const std::vector<int>& a, const std::vector<int>& b) {
              if (a.size() != b.size()) return a.size() > b.size();
              const auto max_degree = [&](const std::vector<int>& component) {
                int result = -1;
                for (int node : component) {
                  result = std::max(result, degree_[node]);
                }
                return result;
              };
              const int a_degree = max_degree(a);
              const int b_degree = max_degree(b);
              if (a_degree != b_degree) return a_degree > b_degree;
              return *std::min_element(a.begin(), a.end()) <
                     *std::min_element(b.begin(), b.end());
            });

  std::vector<int> order;
  order.reserve(n);
  for (const auto& component : components) {
    std::vector<char> in_component(n, 0);
    for (int node : component) in_component[node] = 1;

    int highest_degree = component.front();
    for (int node : component) {
      if (degree_[node] > degree_[highest_degree] ||
          (degree_[node] == degree_[highest_degree] && node < highest_degree)) {
        highest_degree = node;
      }
    }

    const auto distance_from_high_degree =
        bfs_distance(highest_degree, &in_component);
    int max_distance = 0;
    std::vector<int> farthest_nodes;
    for (int node : component) {
      if (distance_from_high_degree[node] < 0) continue;
      if (distance_from_high_degree[node] > max_distance) {
        max_distance = distance_from_high_degree[node];
        farthest_nodes.clear();
      }
      if (distance_from_high_degree[node] == max_distance) {
        farthest_nodes.push_back(node);
      }
    }
    if (farthest_nodes.empty()) farthest_nodes.push_back(highest_degree);
    const int start = *std::min_element(
        farthest_nodes.begin(), farthest_nodes.end(), [&](int a, int b) {
          if (degree_[a] != degree_[b]) return degree_[a] < degree_[b];
          return a < b;
        });

    std::vector<char> labeled(n, 0);
    std::queue<int> q;
    labeled[start] = 1;
    q.push(start);
    while (!q.empty()) {
      const int u = q.front();
      q.pop();
      order.push_back(u);

      std::vector<int> neighbors;
      for (int edge_id : incident_edge_ids_[u]) {
        const int v = GridOtherEndpoint(edge_id, u);
        if (v < 0 || !in_component[v] || labeled[v]) continue;
        labeled[v] = 1;
        neighbors.push_back(v);
      }
      std::sort(neighbors.begin(), neighbors.end(), [&](int a, int b) {
        if (degree_[a] != degree_[b]) return degree_[a] < degree_[b];
        return a < b;
      });
      for (int neighbor : neighbors) q.push(neighbor);
    }

    for (int node : component) {
      if (!labeled[node]) order.push_back(node);
    }
  }
  return order;
}

std::optional<PlacementState>
PrisaEngine::GridConstructPRISAInitialPlacementFromOrder(
    const std::vector<int>& node_order) {
  PlacementState state;
  state.dfg_to_cell.assign(dfg_.GetNodeNum(), -1);
  state.cell_to_dfg.assign(rows_ * cols_, -1);
  for (int label = 0; label < static_cast<int>(node_order.size()); label++) {
    const int node = node_order[label];
    int best_cell = -1;
    int best_distance = std::numeric_limits<int>::max();
    for (int order_index = 0;
         order_index < static_cast<int>(prisa_cell_order_.size());
         order_index++) {
      const int cell = prisa_cell_order_[order_index];
      if (!GridCanPlace(node, cell, state)) continue;
      const int label_distance = std::abs(order_index - label);
      if (label_distance < best_distance) {
        best_distance = label_distance;
        best_cell = cell;
      }
    }
    if (best_cell < 0) return std::nullopt;
    GridPlaceNode(node, best_cell, state);
  }
  return state;
}

std::optional<PlacementState>
PrisaEngine::GridConstructPRISAInitialPlacement() {
  if (!GridUsesPRISASIS()) return GridRandomPlacement();
  return GridConstructPRISAInitialPlacementFromOrder(
      GridLowBandwidthNodeOrder());
}

// PR/WR region accounting and local swap deltas.

bool PrisaEngine::GridPRISAIsWeak(int row, int column) const {
  const int resource_count = static_cast<int>(prisa_cell_order_.size());
  if (row == column || row < 0 || column < 0 || row >= resource_count ||
      column >= resource_count) {
    return false;
  }
  return prisa_weak_region_matrix_[row * resource_count + column] != 0;
}

bool PrisaEngine::GridPRISAIsPotential(int row, int column) const {
  return row != column && !GridPRISAIsWeak(row, column);
}

bool PrisaEngine::GridPRISAEdgeRegion(const PlacementState& state, int edge_id,
                                      bool* is_weak, bool* is_potential) const {
  const auto& edge = edges_[edge_id];
  const int source_cell = state.dfg_to_cell[edge.source];
  const int target_cell = state.dfg_to_cell[edge.target];
  if (source_cell < 0 || target_cell < 0) return false;
  const int row = prisa_order_index_[source_cell];
  const int column = prisa_order_index_[target_cell];
  if (row < 0 || column < 0 || row == column) return false;
  *is_weak = GridPRISAIsWeak(row, column);
  *is_potential = !*is_weak;
  return true;
}

void PrisaEngine::GridAddPRISAEdgeToStats(const PlacementState& state,
                                          int edge_id, PRISAStats& stats,
                                          int sign) const {
  const auto& edge = edges_[edge_id];
  const int source_cell = state.dfg_to_cell[edge.source];
  const int target_cell = state.dfg_to_cell[edge.target];
  if (source_cell < 0 || target_cell < 0) return;
  const int row = prisa_order_index_[source_cell];
  const int column = prisa_order_index_[target_cell];
  if (row < 0 || column < 0 || row == column) return;
  bool is_weak = false;
  bool is_potential = false;
  if (!GridPRISAEdgeRegion(state, edge_id, &is_weak, &is_potential)) return;
  if (is_weak) {
    stats.row_weak[row] += sign;
    stats.column_weak[column] += sign;
    stats.weak_edges += sign;
    stats.edge_weak[edge_id] = sign > 0;
    stats.edge_potential[edge_id] = 0;
  } else if (is_potential) {
    stats.row_potential[row] += sign;
    stats.column_potential[column] += sign;
    stats.edge_potential[edge_id] = sign > 0;
    stats.edge_weak[edge_id] = 0;
  }
}

PRISAStats PrisaEngine::GridInitializePRISAStats(
    const PlacementState& state) const {
  const int resource_count = static_cast<int>(prisa_cell_order_.size());
  PRISAStats stats;
  stats.row_weak.assign(resource_count, 0);
  stats.column_weak.assign(resource_count, 0);
  stats.row_potential.assign(resource_count, 0);
  stats.column_potential.assign(resource_count, 0);
  stats.edge_weak.assign(edges_.size(), 0);
  stats.edge_potential.assign(edges_.size(), 0);
  stats.weak_edges = 0;
  for (int edge_id = 0; edge_id < static_cast<int>(edges_.size()); edge_id++) {
    GridAddPRISAEdgeToStats(state, edge_id, stats, 1);
  }
  return stats;
}

std::vector<int> PrisaEngine::GridAffectedEdgesForCellPair(
    const PlacementState& state, int cell_a, int cell_b) const {
  std::vector<int> result;
  const int node_a = state.cell_to_dfg[cell_a];
  const int node_b = state.cell_to_dfg[cell_b];
  if (node_a >= 0) {
    result.insert(result.end(), incident_edge_ids_[node_a].begin(),
                  incident_edge_ids_[node_a].end());
  }
  if (node_b >= 0) {
    result.insert(result.end(), incident_edge_ids_[node_b].begin(),
                  incident_edge_ids_[node_b].end());
  }
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}

int PrisaEngine::GridCellAfterSwap(int cell, int first_cell,
                                   int second_cell) const {
  if (cell == first_cell) return second_cell;
  if (cell == second_cell) return first_cell;
  return cell;
}

int PrisaEngine::GridEdgeWeakAfterSwap(const PlacementState& state, int edge_id,
                                       int first_cell, int second_cell) const {
  const auto& edge = edges_[edge_id];
  const int source_cell = GridCellAfterSwap(state.dfg_to_cell[edge.source],
                                            first_cell, second_cell);
  const int target_cell = GridCellAfterSwap(state.dfg_to_cell[edge.target],
                                            first_cell, second_cell);
  const int row = prisa_order_index_[source_cell];
  const int column = prisa_order_index_[target_cell];
  return GridPRISAIsWeak(row, column) ? 1 : 0;
}

int PrisaEngine::GridPRISAWeakDeltaForCellSwap(
    const PlacementState& state, int first_cell, int second_cell,
    const std::vector<int>& affected_edges) const {
  int before = 0;
  int after = 0;
  for (int edge_id : affected_edges) {
    bool is_weak = false;
    bool is_potential = false;
    if (GridPRISAEdgeRegion(state, edge_id, &is_weak, &is_potential) &&
        is_weak) {
      before++;
    }
    after += GridEdgeWeakAfterSwap(state, edge_id, first_cell, second_cell);
  }
  return after - before;
}

double PrisaEngine::GridPlacementCostDeltaForCellSwap(
    const PlacementState& state, int first_cell, int second_cell,
    const std::vector<int>& affected_edges) const {
  double before = 0.0;
  double after = 0.0;
  for (int edge_id : affected_edges) {
    const auto& edge = edges_[edge_id];
    const int source_cell = state.dfg_to_cell[edge.source];
    const int target_cell = state.dfg_to_cell[edge.target];
    before += GridDistanceCost(source_cell, target_cell);
    after += GridDistanceCost(
        GridCellAfterSwap(source_cell, first_cell, second_cell),
        GridCellAfterSwap(target_cell, first_cell, second_cell));
  }
  return after - before;
}

int PrisaEngine::GridDirectEdgeDeltaForCellSwap(
    const PlacementState& state, int first_cell, int second_cell,
    const std::vector<int>& affected_edges) const {
  int before = 0;
  int after = 0;
  for (int edge_id : affected_edges) {
    const auto& edge = edges_[edge_id];
    const int source_cell = state.dfg_to_cell[edge.source];
    const int target_cell = state.dfg_to_cell[edge.target];
    before += GridDistanceCost(source_cell, target_cell) <= 1 ? 1 : 0;
    after += GridDistanceCost(
                 GridCellAfterSwap(source_cell, first_cell, second_cell),
                 GridCellAfterSwap(target_cell, first_cell, second_cell)) <= 1
                 ? 1
                 : 0;
  }
  return after - before;
}

int PrisaEngine::GridAffectedMaxDistanceAfterCellSwap(
    const PlacementState& state, int first_cell, int second_cell,
    const std::vector<int>& affected_edges) const {
  int result = 0;
  for (int edge_id : affected_edges) {
    const auto& edge = edges_[edge_id];
    const int source_cell = state.dfg_to_cell[edge.source];
    const int target_cell = state.dfg_to_cell[edge.target];
    result = std::max(
        result, GridDistanceCost(
                    GridCellAfterSwap(source_cell, first_cell, second_cell),
                    GridCellAfterSwap(target_cell, first_cell, second_cell)));
  }
  return result;
}

bool PrisaEngine::GridCanSwapCells(const PlacementState& state, int first_cell,
                                   int second_cell) const {
  if (first_cell == second_cell || first_cell < 0 || second_cell < 0) {
    return false;
  }
  const int first_node = state.cell_to_dfg[first_cell];
  const int second_node = state.cell_to_dfg[second_cell];
  if (first_node < 0 && second_node < 0) return false;

  // Structural-I/O placement assigns source/sink nodes to logical I/O cells
  // and all other nodes to compute cells. GridIsCompatible() intentionally
  // skips that logical type check, so every move must restore it explicitly.
  const auto can_occupy = [this](int node, int cell) {
    if (node < 0) return true;
    if (GridUsesStructuralIOCellTypes()) {
      return GridIsCPUMappingCompatible(node, cell);
    }
    return GridIsCompatible(node, cell);
  };
  if (!can_occupy(first_node, second_cell)) {
    return false;
  }
  if (!can_occupy(second_node, first_cell)) {
    return false;
  }
  return true;
}

bool PrisaEngine::GridApplyCellSwapWithStats(PlacementState& state,
                                             PRISAStats& stats, int first_cell,
                                             int second_cell) const {
  if (!GridCanSwapCells(state, first_cell, second_cell)) return false;
  const auto affected_edges =
      GridAffectedEdgesForCellPair(state, first_cell, second_cell);
  for (int edge_id : affected_edges) {
    GridAddPRISAEdgeToStats(state, edge_id, stats, -1);
  }

  const int first_node = state.cell_to_dfg[first_cell];
  const int second_node = state.cell_to_dfg[second_cell];
  state.cell_to_dfg[first_cell] = second_node;
  state.cell_to_dfg[second_cell] = first_node;
  if (first_node >= 0) state.dfg_to_cell[first_node] = second_cell;
  if (second_node >= 0) state.dfg_to_cell[second_node] = first_cell;

  for (int edge_id : affected_edges) {
    GridAddPRISAEdgeToStats(state, edge_id, stats, 1);
  }
  return true;
}

bool PrisaEngine::GridApplyCellSwap(PlacementState& state, int first_cell,
                                    int second_cell) const {
  if (!GridCanSwapCells(state, first_cell, second_cell)) return false;
  const int first_node = state.cell_to_dfg[first_cell];
  const int second_node = state.cell_to_dfg[second_cell];
  state.cell_to_dfg[first_cell] = second_node;
  state.cell_to_dfg[second_cell] = first_node;
  if (first_node >= 0) state.dfg_to_cell[first_node] = second_cell;
  if (second_node >= 0) state.dfg_to_cell[second_node] = first_cell;
  return true;
}

std::optional<PrisaEngine::PRISAProposal>
PrisaEngine::GridProposeRandomPRISASwap(const PlacementState& state) {
  if (prisa_cell_order_.size() < 2) return std::nullopt;
  std::uniform_int_distribution<int> dist(
      0, static_cast<int>(prisa_cell_order_.size()) - 1);
  for (int attempt = 0; attempt < 32; attempt++) {
    const int first_cell = prisa_cell_order_[dist(rng_)];
    const int second_cell = prisa_cell_order_[dist(rng_)];
    if (!GridCanSwapCells(state, first_cell, second_cell)) continue;
    const auto affected_edges =
        GridAffectedEdgesForCellPair(state, first_cell, second_cell);
    return PRISAProposal{first_cell, second_cell,
                         GridPlacementCostDeltaForCellSwap(
                             state, first_cell, second_cell, affected_edges),
                         GridDirectEdgeDeltaForCellSwap(
                             state, first_cell, second_cell, affected_edges),
                         GridAffectedMaxDistanceAfterCellSwap(
                             state, first_cell, second_cell, affected_edges)};
  }
  return std::nullopt;
}

std::optional<PrisaEngine::PRISAProposal>
PrisaEngine::GridProposePlacementCostSampledSwap(const PlacementState& state,
                                                 int requested_sample_count) {
  const int resource_count = static_cast<int>(prisa_cell_order_.size());
  if (resource_count < 2) return std::nullopt;
  const int total_pairs = resource_count * (resource_count - 1) / 2;
  const int default_sample_count = std::max(16, resource_count * 4);
  const int sample_count =
      std::min(total_pairs, requested_sample_count > 0 ? requested_sample_count
                                                       : default_sample_count);
  std::optional<PRISAProposal> best;

  auto is_better_fallback = [&](const PRISAProposal& candidate,
                                const PRISAProposal& current_best) {
    if (candidate.cost_delta + 1.0e-9 < current_best.cost_delta) {
      return true;
    }
    if (candidate.cost_delta > current_best.cost_delta + 1.0e-9) {
      return false;
    }
    if (GridUsesCostAwarePRISA()) {
      if (candidate.direct_delta != current_best.direct_delta) {
        return candidate.direct_delta > current_best.direct_delta;
      }
      if (candidate.affected_max_after != current_best.affected_max_after) {
        return candidate.affected_max_after < current_best.affected_max_after;
      }
    }
    return false;
  };

  auto consider_pair = [&](int first_index, int second_index) {
    const int first_cell = prisa_cell_order_[first_index];
    const int second_cell = prisa_cell_order_[second_index];
    if (!GridCanSwapCells(state, first_cell, second_cell)) return;
    const auto affected_edges =
        GridAffectedEdgesForCellPair(state, first_cell, second_cell);
    const double cost_delta = GridPlacementCostDeltaForCellSwap(
        state, first_cell, second_cell, affected_edges);
    PRISAProposal candidate{
        first_cell, second_cell, cost_delta,
        GridDirectEdgeDeltaForCellSwap(state, first_cell, second_cell,
                                       affected_edges),
        GridAffectedMaxDistanceAfterCellSwap(state, first_cell, second_cell,
                                             affected_edges)};
    if (!best.has_value() || is_better_fallback(candidate, *best)) {
      best = candidate;
    }
  };

  if (sample_count == total_pairs) {
    for (int first = 0; first < resource_count; first++) {
      for (int second = first + 1; second < resource_count; second++) {
        consider_pair(first, second);
      }
    }
  } else {
    std::uniform_int_distribution<int> dist(0, resource_count - 1);
    for (int attempt = 0; attempt < sample_count * 4; attempt++) {
      const int first = dist(rng_);
      const int second = dist(rng_);
      if (first == second) continue;
      consider_pair(std::min(first, second), std::max(first, second));
    }
  }
  if (best.has_value()) return best;
  return GridProposeRandomPRISASwap(state);
}

// PRISA move proposal. The base variant tries to repair weak-region edges;
// cost-aware variants use additional direct-edge and hop-distance terms.

int PrisaEngine::GridPRISALightQualitySampleCount() const {
  const int resource_count = static_cast<int>(prisa_cell_order_.size());
  // SIS already gives PRISA a low-bandwidth starting point.  Keep the
  // post-WR quality refinement cheaper than no-SIS so the runtime trend
  // matches the paper without using the cost-aware derived mapper.
  if (GridUsesPRISASIS()) return std::max(8, resource_count / 3);
  return std::max(16, resource_count);
}

double PrisaEngine::GridCostAwarePRISALocalSwapScore(
    const PlacementState& state, int first_cell, int second_cell,
    const std::vector<int>& affected_edges) const {
  double mesh_delta = 0.0;
  int after_max_hop = 0;
  int direct_edge_gain = 0;
  for (int edge_id : affected_edges) {
    const auto& edge = edges_[edge_id];
    const int source_cell = state.dfg_to_cell[edge.source];
    const int target_cell = state.dfg_to_cell[edge.target];
    const int before = GridDistanceCost(source_cell, target_cell);
    const int after = GridDistanceCost(
        GridCellAfterSwap(source_cell, first_cell, second_cell),
        GridCellAfterSwap(target_cell, first_cell, second_cell));
    mesh_delta += after - before;
    after_max_hop = std::max(after_max_hop, after);
    if (before <= 1 && after > 1) direct_edge_gain--;
    if (before > 1 && after <= 1) direct_edge_gain++;
  }

  const int weak_delta = GridPRISAWeakDeltaForCellSwap(
      state, first_cell, second_cell, affected_edges);
  return weak_delta * 1000000.0 + mesh_delta * 100.0 + after_max_hop * 20.0 -
         direct_edge_gain * 80.0;
}

int PrisaEngine::GridCostAwarePRISACandidateSampleCount() const {
  if (max_trials_.has_value()) return std::max(16, max_trials_.value());
  return 512;
}

int PrisaEngine::GridCostAwarePRISAFullEvaluationCount() const { return 8; }

std::optional<PrisaEngine::PRISAProposal>
PrisaEngine::GridProposeCostAwarePRISASwap(const PlacementState& state) {
  const int resource_count = static_cast<int>(prisa_cell_order_.size());
  if (resource_count < 2) return std::nullopt;

  const int total_pairs = resource_count * (resource_count - 1) / 2;
  const int sample_count =
      std::min(total_pairs, GridCostAwarePRISACandidateSampleCount());
  auto normalize_pair = [](int lhs, int rhs) {
    if (lhs > rhs) std::swap(lhs, rhs);
    return std::pair<int, int>(lhs, rhs);
  };

  std::set<std::pair<int, int>> sampled_pairs;
  if (sample_count == total_pairs) {
    for (int first = 0; first < resource_count; first++) {
      for (int second = first + 1; second < resource_count; second++) {
        sampled_pairs.insert({first, second});
      }
    }
  } else {
    std::vector<std::pair<int, int>> edge_hops;
    edge_hops.reserve(edges_.size());
    for (int edge_id = 0; edge_id < static_cast<int>(edges_.size());
         edge_id++) {
      const auto& edge = edges_[edge_id];
      const int source_cell = state.dfg_to_cell[edge.source];
      const int target_cell = state.dfg_to_cell[edge.target];
      if (source_cell < 0 || target_cell < 0) continue;
      edge_hops.push_back(
          {GridDistanceCost(source_cell, target_cell), edge_id});
    }
    std::sort(edge_hops.begin(), edge_hops.end(),
              std::greater<std::pair<int, int>>());

    const int focused_budget = std::max(1, sample_count / 2);
    const int focused_edge_count =
        std::min(4, static_cast<int>(edge_hops.size()));
    for (int index = 0; index < focused_edge_count &&
                        static_cast<int>(sampled_pairs.size()) < focused_budget;
         index++) {
      const auto& edge = edges_[edge_hops[index].second];
      const int endpoint_cells[] = {state.dfg_to_cell[edge.source],
                                    state.dfg_to_cell[edge.target]};
      for (int endpoint_cell : endpoint_cells) {
        const int endpoint_index = prisa_order_index_[endpoint_cell];
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

    std::uniform_int_distribution<int> dist(0, resource_count - 1);
    int attempts = 0;
    while (static_cast<int>(sampled_pairs.size()) < sample_count &&
           attempts < sample_count * 16) {
      attempts++;
      const int first = dist(rng_);
      const int second = dist(rng_);
      if (first == second) continue;
      sampled_pairs.insert(normalize_pair(first, second));
    }
  }

  struct Candidate {
    double score = std::numeric_limits<double>::infinity();
    int first_cell = -1;
    int second_cell = -1;
    int random_order = 0;
  };

  std::vector<Candidate> candidates;
  candidates.reserve(sampled_pairs.size());
  int random_order = 0;
  for (const auto& [first_index, second_index] : sampled_pairs) {
    const int first_cell = prisa_cell_order_[first_index];
    const int second_cell = prisa_cell_order_[second_index];
    if (!GridCanSwapCells(state, first_cell, second_cell)) continue;
    const auto affected_edges =
        GridAffectedEdgesForCellPair(state, first_cell, second_cell);
    candidates.push_back({GridCostAwarePRISALocalSwapScore(
                              state, first_cell, second_cell, affected_edges),
                          first_cell, second_cell, random_order++});
  }
  if (candidates.empty()) return GridProposeRandomPRISASwap(state);

  std::shuffle(candidates.begin(), candidates.end(), rng_);
  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate& a, const Candidate& b) {
              if (a.score != b.score) return a.score < b.score;
              return a.random_order < b.random_order;
            });

  const int full_evaluation_count =
      std::min(GridCostAwarePRISAFullEvaluationCount(),
               static_cast<int>(candidates.size()));
  const double current_score = GridCostAwarePRISAScore(state);
  for (int i = 0; i < full_evaluation_count; i++) {
    PlacementState next = state;
    if (!GridApplyCellSwap(next, candidates[i].first_cell,
                           candidates[i].second_cell)) {
      candidates[i].score = kGridImpossibleCost;
      continue;
    }
    candidates[i].score = GridCostAwarePRISAScore(next);
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
  return PRISAProposal{chosen.first_cell, chosen.second_cell,
                       chosen.score - current_score, 0, 0};
}

std::vector<int> PrisaEngine::GridPRISAWeakCounterparts(
    const PlacementState& state, const PRISAStats& stats, int first_index,
    bool row_side) const {
  std::vector<int> counterparts;
  counterparts.reserve(row_side ? stats.row_weak[first_index]
                                : stats.column_weak[first_index]);
  for (int edge_id = 0; edge_id < static_cast<int>(edges_.size()); edge_id++) {
    if (!stats.edge_weak[edge_id]) continue;
    const auto& edge = edges_[edge_id];
    const int source_cell = state.dfg_to_cell[edge.source];
    const int target_cell = state.dfg_to_cell[edge.target];
    const int row = prisa_order_index_[source_cell];
    const int column = prisa_order_index_[target_cell];
    if (row_side) {
      if (row == first_index) counterparts.push_back(column);
    } else {
      if (column == first_index) counterparts.push_back(row);
    }
  }
  return counterparts;
}

bool PrisaEngine::GridIsBetterPRISACandidate(const PRISACandidate& candidate,
                                             const PRISACandidate& best) const {
  if (GridUsesCostAwarePRISA()) {
    if (candidate.potential_count != best.potential_count) {
      return candidate.potential_count < best.potential_count;
    }
    if (candidate.fixed_weak_count != best.fixed_weak_count) {
      return candidate.fixed_weak_count > best.fixed_weak_count;
    }
    if (candidate.weak_delta != best.weak_delta) {
      return candidate.weak_delta < best.weak_delta;
    }
    if (candidate.cost_delta != best.cost_delta) {
      return candidate.cost_delta < best.cost_delta;
    }
    if (candidate.affected_max_after != best.affected_max_after) {
      return candidate.affected_max_after < best.affected_max_after;
    }
    if (candidate.direct_delta != best.direct_delta) {
      return candidate.direct_delta > best.direct_delta;
    }
    return candidate.random_order < best.random_order;
  }
  if (candidate.potential_count != best.potential_count) {
    return candidate.potential_count < best.potential_count;
  }
  if (candidate.fixed_weak_count != best.fixed_weak_count) {
    return candidate.fixed_weak_count > best.fixed_weak_count;
  }
  if (candidate.weak_delta != best.weak_delta) {
    return candidate.weak_delta < best.weak_delta;
  }
  if (candidate.cost_delta != best.cost_delta) {
    return candidate.cost_delta < best.cost_delta;
  }
  if (candidate.direct_delta != best.direct_delta) {
    return candidate.direct_delta > best.direct_delta;
  }
  if (candidate.affected_max_after != best.affected_max_after) {
    return candidate.affected_max_after < best.affected_max_after;
  }
  if (candidate.spatial_distance != best.spatial_distance) {
    return candidate.spatial_distance < best.spatial_distance;
  }
  return candidate.random_order < best.random_order;
}

std::optional<PrisaEngine::PRISACandidate>
PrisaEngine::GridBestPRISACandidateFor(const PlacementState& state,
                                       const PRISAStats& stats, int first_index,
                                       bool row_side) {
  const auto weak_counterparts =
      GridPRISAWeakCounterparts(state, stats, first_index, row_side);
  if (weak_counterparts.empty()) return std::nullopt;

  const int resource_count = static_cast<int>(prisa_cell_order_.size());
  const int first_cell = prisa_cell_order_[first_index];
  std::uniform_int_distribution<int> start_dist(0, resource_count - 1);
  const int start = start_dist(rng_);

  struct CandidateKey {
    int second_index = -1;
    int fixed_weak_count = 0;
    int potential_count = 0;
    int spatial_distance = 0;
    int random_order = 0;
  };

  std::vector<CandidateKey> filtered;
  int best_potential_count = std::numeric_limits<int>::max();
  int best_fixed_weak_count = std::numeric_limits<int>::min();

  for (int offset = 0; offset < resource_count; offset++) {
    const int second_index = (start + offset) % resource_count;
    if (second_index == first_index) continue;
    const int second_cell = prisa_cell_order_[second_index];
    if (!GridCanSwapCells(state, first_cell, second_cell)) continue;

    int fixed_weak_count = 0;
    for (int counterpart : weak_counterparts) {
      const bool moves_to_pr =
          row_side ? GridPRISAIsPotential(second_index, counterpart)
                   : GridPRISAIsPotential(counterpart, second_index);
      if (moves_to_pr) fixed_weak_count++;
    }
    if (fixed_weak_count == 0) continue;

    const int potential_count = row_side ? stats.row_potential[second_index]
                                         : stats.column_potential[second_index];
    if (potential_count < best_potential_count ||
        (potential_count == best_potential_count &&
         fixed_weak_count > best_fixed_weak_count)) {
      filtered.clear();
      best_potential_count = potential_count;
      best_fixed_weak_count = fixed_weak_count;
    }
    if (potential_count == best_potential_count &&
        fixed_weak_count == best_fixed_weak_count) {
      filtered.push_back({second_index, fixed_weak_count, potential_count,
                          GridDistanceCost(first_cell, second_cell), offset});
    }
  }

  std::optional<PRISACandidate> best;
  for (const auto& key : filtered) {
    const int second_cell = prisa_cell_order_[key.second_index];
    const auto affected_edges =
        GridAffectedEdgesForCellPair(state, first_cell, second_cell);
    PRISACandidate candidate{
        first_index,
        key.second_index,
        key.fixed_weak_count,
        key.potential_count,
        GridPRISAWeakDeltaForCellSwap(state, first_cell, second_cell,
                                      affected_edges),
        GridPlacementCostDeltaForCellSwap(state, first_cell, second_cell,
                                          affected_edges),
        GridDirectEdgeDeltaForCellSwap(state, first_cell, second_cell,
                                       affected_edges),
        GridAffectedMaxDistanceAfterCellSwap(state, first_cell, second_cell,
                                             affected_edges),
        key.spatial_distance,
        key.random_order};
    if (!best.has_value() || GridIsBetterPRISACandidate(candidate, *best)) {
      best = candidate;
    }
  }
  return best;
}

int PrisaEngine::GridRandomArgmax(const std::vector<int>& values) {
  int best = std::numeric_limits<int>::min();
  int best_index = -1;
  int equal_count = 0;
  for (int i = 0; i < static_cast<int>(values.size()); i++) {
    if (values[i] > best) {
      best = values[i];
      best_index = i;
      equal_count = 1;
    } else if (values[i] == best) {
      equal_count++;
      std::uniform_int_distribution<int> dist(0, equal_count - 1);
      if (dist(rng_) == 0) best_index = i;
    }
  }
  return best_index;
}

std::optional<PrisaEngine::PRISAProposal> PrisaEngine::GridProposePRISAMove(
    const PlacementState& state, const PRISAStats& stats) {
  if (GridUsesCostAwarePRISA()) return GridProposeCostAwarePRISASwap(state);
  if (stats.weak_edges <= 0) {
    // After all weak-region edges are repaired, PRISA should not fall into
    // the expensive cost-aware search used by the derived mapper.  A small
    // random sample keeps the paper-style random-swap refinement cheap while
    // still allowing annealing to improve placement quality.
    return GridProposePlacementCostSampledSwap(
        state, GridPRISALightQualitySampleCount());
  }

  const int row_index = GridRandomArgmax(stats.row_weak);
  const int column_index = GridRandomArgmax(stats.column_weak);
  if (row_index < 0 || column_index < 0) {
    return GridProposePlacementCostSampledSwap(
        state, GridPRISALightQualitySampleCount());
  }
  const int row_max = stats.row_weak[row_index];
  const int column_max = stats.column_weak[column_index];
  if (row_max <= 0 && column_max <= 0) {
    return GridProposePlacementCostSampledSwap(
        state, GridPRISALightQualitySampleCount());
  }

  std::optional<PRISACandidate> candidate;
  const bool prefer_row = row_max > column_max ||
                          (row_max == column_max &&
                           std::uniform_int_distribution<int>(0, 1)(rng_) == 0);
  if (prefer_row) {
    candidate = GridBestPRISACandidateFor(state, stats, row_index, true);
    if (!candidate.has_value()) {
      candidate = GridBestPRISACandidateFor(state, stats, column_index, false);
    }
  } else {
    candidate = GridBestPRISACandidateFor(state, stats, column_index, false);
    if (!candidate.has_value()) {
      candidate = GridBestPRISACandidateFor(state, stats, row_index, true);
    }
  }
  if (!candidate.has_value()) {
    return GridProposePlacementCostSampledSwap(
        state, GridPRISALightQualitySampleCount());
  }
  return PRISAProposal{prisa_cell_order_[candidate->first_index],
                       prisa_cell_order_[candidate->second_index],
                       candidate->cost_delta, candidate->direct_delta,
                       candidate->affected_max_after};
}

// Array PRISA pipeline.
//
//   1. GridConstructPRISAInitialPlacement()
//      Build the initial placement. SIS uses GridLowBandwidthNodeOrder();
//      no-SIS starts from GridRandomPlacement().
//   2. GridInitializePRISAStats()
//      Count weak-region and potential-region edges in the PRISA distance
//      matrix.
//   3. GridProposePRISAMove()
//      Pick a swap that repairs weak-region edges. Derived cost-aware variants
//      can instead score sampled swaps by placement quality.
//   4. GridRunPRISA()
//      Apply proposals under a small annealing schedule and keep the best
//      placement seen in that trial.
//   5. GridRunPRISAMultiSeed()
//      Repeat seeds and keep the best PlacementQuality().
//
// Ablation handles:
//   - SIS order: GridLowBandwidthNodeOrder()
//   - PR/WR matrix use: GridPRISAIsWeak(), GridPRISAEdgeRegion()
//   - move choice: GridBestPRISACandidateFor(), GridProposePRISAMove()
//   - derived cost-aware scoring: GridProposeCostAwarePRISASwap()
//   - final cleanup: GridPolishPRISAQuality(),
//   GridPolishCostAwarePRISADirectEdges()

int PrisaEngine::GridPRISAMaxIterations() const {
  if (max_iterations_.has_value()) return std::max(1, max_iterations_.value());
  return std::max(1, dfg_.GetNodeNum() * 10);
}

int PrisaEngine::GridPRISAPolishPasses() const {
  if (!GridUsesCostAwarePRISA()) return 0;
  return std::max(4, std::min(16, GridPRISAMaxIterations() / 80));
}

// Derived cost-aware cleanup passes. Base PRISA and no-SIS PRISA skip these.

PlacementState PrisaEngine::GridPolishPRISAQuality(
    PlacementState state, std::chrono::steady_clock::time_point start) {
  PlacementQuality current = GridComputePlacementQuality(state);
  const int resource_count = static_cast<int>(prisa_cell_order_.size());
  if (resource_count < 2) return state;

  for (int pass = 0;
       pass < GridPRISAPolishPasses() && !HasTimedOut(start, 0.05); pass++) {
    bool improved = false;
    PlacementState best_state = state;
    PlacementQuality best_quality = current;

    for (int first_index = 0; first_index < resource_count; first_index++) {
      for (int second_index = first_index + 1; second_index < resource_count;
           second_index++) {
        const int first_cell = prisa_cell_order_[first_index];
        const int second_cell = prisa_cell_order_[second_index];
        if (!GridCanSwapCells(state, first_cell, second_cell)) continue;
        const auto affected_edges =
            GridAffectedEdgesForCellPair(state, first_cell, second_cell);
        const int direct_delta = GridDirectEdgeDeltaForCellSwap(
            state, first_cell, second_cell, affected_edges);
        const double cost_delta = GridPlacementCostDeltaForCellSwap(
            state, first_cell, second_cell, affected_edges);
        if (direct_delta < 0) continue;
        if (cost_delta > 1.0e-9) continue;
        if (direct_delta == 0 && cost_delta >= -1.0e-9) continue;

        PlacementState next = state;
        if (!GridApplyCellSwap(next, first_cell, second_cell)) continue;
        const PlacementQuality next_quality = GridComputePlacementQuality(next);
        if (GridIsBetterQuality(next_quality, best_quality)) {
          best_quality = next_quality;
          best_state = std::move(next);
          improved = true;
        }
      }
    }

    if (!improved) break;
    state = std::move(best_state);
    current = best_quality;
  }
  return state;
}

int PrisaEngine::GridDirectEdgeGainForCellSwap(
    const PlacementState& state, int first_cell, int second_cell,
    const std::vector<int>& affected_edges) const {
  int gain = 0;
  for (int edge_id : affected_edges) {
    const auto& edge = edges_[edge_id];
    const int source_cell = state.dfg_to_cell[edge.source];
    const int target_cell = state.dfg_to_cell[edge.target];
    const int before = GridDistanceCost(source_cell, target_cell);
    const int after = GridDistanceCost(
        GridCellAfterSwap(source_cell, first_cell, second_cell),
        GridCellAfterSwap(target_cell, first_cell, second_cell));
    if (before <= 1 && after > 1) gain--;
    if (before > 1 && after <= 1) gain++;
  }
  return gain;
}

PlacementState PrisaEngine::GridPolishCostAwarePRISADirectEdges(
    PlacementState state, std::chrono::steady_clock::time_point start) {
  const int resource_count = static_cast<int>(prisa_cell_order_.size());
  if (resource_count < 2) return state;

  struct Candidate {
    int direct_gain = 0;
    double local_score = std::numeric_limits<double>::infinity();
    int first_cell = -1;
    int second_cell = -1;
  };

  GridCostAwarePRISAMetrics current = GridComputeCostAwarePRISAMetrics(state);
  for (int pass = 0;
       pass < GridPRISAPolishPasses() && !HasTimedOut(start, 0.05); pass++) {
    std::vector<Candidate> candidates;
    candidates.reserve(resource_count * resource_count / 2);
    for (int first_index = 0; first_index < resource_count; first_index++) {
      for (int second_index = first_index + 1; second_index < resource_count;
           second_index++) {
        const int first_cell = prisa_cell_order_[first_index];
        const int second_cell = prisa_cell_order_[second_index];
        if (!GridCanSwapCells(state, first_cell, second_cell)) continue;
        const auto affected_edges =
            GridAffectedEdgesForCellPair(state, first_cell, second_cell);
        const int direct_gain = GridDirectEdgeGainForCellSwap(
            state, first_cell, second_cell, affected_edges);
        if (direct_gain <= 0) continue;
        candidates.push_back(
            {direct_gain,
             GridCostAwarePRISALocalSwapScore(state, first_cell, second_cell,
                                              affected_edges),
             first_cell, second_cell});
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
    GridCostAwarePRISAMetrics best_metrics = current;
    for (const auto& candidate : candidates) {
      PlacementState next = state;
      if (!GridApplyCellSwap(next, candidate.first_cell,
                             candidate.second_cell)) {
        continue;
      }
      const GridCostAwarePRISAMetrics next_metrics =
          GridComputeCostAwarePRISAMetrics(next);
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

bool PrisaEngine::GridIsCostAwareParetoImprovement(
    const GridCostAwarePRISAMetrics& candidate,
    const GridCostAwarePRISAMetrics& current) const {
  if (candidate.mesh_hop_sum > current.mesh_hop_sum + 1.0e-9) return false;
  if (candidate.max_mesh_hop > current.max_mesh_hop) return false;
  if (candidate.mapped_lp_mesh_hop > current.mapped_lp_mesh_hop) {
    return false;
  }
  return candidate.direct_edge_count > current.direct_edge_count ||
         candidate.mesh_hop_sum + 1.0e-9 < current.mesh_hop_sum ||
         candidate.max_mesh_hop < current.max_mesh_hop ||
         candidate.mapped_lp_mesh_hop < current.mapped_lp_mesh_hop;
}

bool PrisaEngine::GridIsBetterCostAwareParetoCandidate(
    const GridCostAwarePRISAMetrics& candidate,
    const GridCostAwarePRISAMetrics& best) const {
  if (candidate.direct_edge_count != best.direct_edge_count) {
    return candidate.direct_edge_count > best.direct_edge_count;
  }
  if (candidate.max_mesh_hop != best.max_mesh_hop) {
    return candidate.max_mesh_hop < best.max_mesh_hop;
  }
  if (candidate.mapped_lp_mesh_hop != best.mapped_lp_mesh_hop) {
    return candidate.mapped_lp_mesh_hop < best.mapped_lp_mesh_hop;
  }
  return candidate.mesh_hop_sum + 1.0e-9 < best.mesh_hop_sum;
}

PlacementState PrisaEngine::GridPolishCostAwarePRISAPareto(
    PlacementState state, std::chrono::steady_clock::time_point start) {
  const int resource_count = static_cast<int>(prisa_cell_order_.size());
  if (resource_count < 2) return state;

  GridCostAwarePRISAMetrics current = GridComputeCostAwarePRISAMetrics(state);
  const int passes = std::min(4, GridPRISAPolishPasses());
  for (int pass = 0; pass < passes && !HasTimedOut(start, 0.05); pass++) {
    bool improved = false;
    PlacementState best_state = state;
    GridCostAwarePRISAMetrics best_metrics = current;
    const int current_weak_edges = GridPRISAWeakEdgeCount(state);

    for (int first_index = 0; first_index < resource_count; first_index++) {
      for (int second_index = first_index + 1;
           second_index < resource_count && !HasTimedOut(start, 0.05);
           second_index++) {
        const int first_cell = prisa_cell_order_[first_index];
        const int second_cell = prisa_cell_order_[second_index];
        if (!GridCanSwapCells(state, first_cell, second_cell)) continue;

        PlacementState next = state;
        if (!GridApplyCellSwap(next, first_cell, second_cell)) continue;
        if (GridPRISAWeakEdgeCount(next) > current_weak_edges) continue;
        const GridCostAwarePRISAMetrics next_metrics =
            GridComputeCostAwarePRISAMetrics(next);
        if (!GridIsCostAwareParetoImprovement(next_metrics, current)) continue;
        if (!improved ||
            GridIsBetterCostAwareParetoCandidate(next_metrics, best_metrics)) {
          best_state = std::move(next);
          best_metrics = next_metrics;
          improved = true;
        }
      }
    }

    if (!improved) break;
    state = std::move(best_state);
    current = best_metrics;
  }
  return state;
}

// Trial loop and multi-seed selection.

std::optional<PlacementState> PrisaEngine::GridRunPRISA(
    std::chrono::steady_clock::time_point start) {
  auto current = GridConstructPRISAInitialPlacement();
  if (!current.has_value()) return std::nullopt;
  PRISAStats stats = GridInitializePRISAStats(*current);

  PlacementState best = *current;
  double current_cost = GridPRISAAcceptanceScore(*current);
  double best_cost = current_cost;
  PlacementQuality best_quality = GridComputePlacementQuality(best);
  const double initial_cost = GridPlacementCost(*current);
  const double initial_acceptance_cost = current_cost;
  const int initial_weak_edges = stats.weak_edges;
  const int max_iterations = GridPRISAMaxIterations();
  const double start_probability = GridUsesPRISASIS() ? 0.20 : 0.40;
  const double end_probability = 0.01;
  double temperature = -1.0 / std::log(start_probability);
  const double final_temperature = -1.0 / std::log(end_probability);
  const double cooling = std::pow(final_temperature / temperature,
                                  1.0 / std::max(1, max_iterations));
  std::uniform_real_distribution<double> unit(0.0, 1.0);
  int generated_moves = 0;
  int accepted_moves = 0;
  int improving_moves = 0;
  int completed_iterations = 0;

  for (int iteration = 0;
       iteration < max_iterations && !HasTimedOut(start, 0.05); iteration++) {
    completed_iterations = iteration + 1;
    auto proposal = GridProposePRISAMove(*current, stats);
    if (!proposal.has_value()) continue;
    generated_moves++;
    const double next_cost = current_cost + proposal->cost_delta;
    const double delta = next_cost - current_cost;
    const bool accept =
        delta <= 0.0 || unit(rng_) < std::exp(-delta / temperature);
    if (accept) {
      if (!GridApplyCellSwapWithStats(*current, stats, proposal->first_cell,
                                      proposal->second_cell)) {
        continue;
      }
      current_cost = next_cost;
      accepted_moves++;
      const bool better_cost = current_cost + 1.0e-9 < best_cost;
      bool better_same_cost = false;
      if (!GridUsesCostAwarePRISA() &&
          std::abs(current_cost - best_cost) <= 1.0e-9) {
        const PlacementQuality current_quality =
            GridComputePlacementQuality(*current);
        better_same_cost = GridIsBetterQuality(current_quality, best_quality);
        if (better_same_cost) best_quality = current_quality;
      }
      if (better_cost || better_same_cost) {
        best = *current;
        best_cost = current_cost;
        if (better_cost) best_quality = GridComputePlacementQuality(best);
        improving_moves++;
      }
    }
    temperature = std::max(final_temperature, temperature * cooling);
  }

  if (GridUsesCostAwarePRISA()) {
    const double before_polish_cost = best_cost;
    PlacementState polished = GridPolishPRISAQuality(best, start);
    polished = GridPolishCostAwarePRISADirectEdges(polished, start);
    polished = GridPolishCostAwarePRISAPareto(polished, start);
    const double polished_cost = GridPRISAAcceptanceScore(polished);
    const GridCostAwarePRISAMetrics polished_metrics =
        GridComputeCostAwarePRISAMetrics(polished);
    const GridCostAwarePRISAMetrics best_metrics =
        GridComputeCostAwarePRISAMetrics(best);
    if (polished_cost + 1.0e-9 < before_polish_cost ||
        (polished_metrics.direct_edge_count > best_metrics.direct_edge_count &&
         polished_metrics.max_mesh_hop <= best_metrics.max_mesh_hop &&
         polished_metrics.mapped_lp_mesh_hop <=
             best_metrics.mapped_lp_mesh_hop)) {
      best = std::move(polished);
      best_cost = GridPRISAAcceptanceScore(best);
      improving_moves++;
    }
  }

  Log("array_prisa_result sis=" + std::to_string(GridUsesPRISASIS() ? 1 : 0) +
      " iterations=" + std::to_string(completed_iterations) +
      " generated_moves=" + std::to_string(generated_moves) +
      " accepted_moves=" + std::to_string(accepted_moves) +
      " improving_moves=" + std::to_string(improving_moves) +
      " initial_cost=" + std::to_string(initial_cost) +
      " initial_acceptance_cost=" + std::to_string(initial_acceptance_cost) +
      " best_cost=" + std::to_string(best_cost) +
      " best_placement_cost=" + std::to_string(GridPlacementCost(best)) +
      " initial_weak_edges=" + std::to_string(initial_weak_edges) +
      " best_weak_edges=" + std::to_string(stats.weak_edges));
  return best;
}

std::optional<PlacementState> PrisaEngine::GridRunPRISAMultiSeed(
    std::chrono::steady_clock::time_point start) {
  std::optional<PlacementState> best;
  double best_cost = std::numeric_limits<double>::infinity();
  PlacementQuality best_quality;
  int seeds = 0;
  for (int seed = 0; seed < SeedCount() && !HasTimedOut(start, 0.05); seed++) {
    ResetSeed(seed);
    auto placement = GridRunPRISA(start);
    seeds++;
    if (!placement.has_value()) continue;
    const double cost = GridPRISAAcceptanceScore(*placement);
    const PlacementQuality quality = GridComputePlacementQuality(*placement);
    const bool better_cost = cost + 1.0e-9 < best_cost;
    const bool better_same_cost = !GridUsesCostAwarePRISA() &&
                                  std::abs(cost - best_cost) <= 1.0e-9 &&
                                  GridIsBetterQuality(quality, best_quality);
    if (better_cost || better_same_cost) {
      best = placement;
      best_cost = cost;
      best_quality = quality;
    }
  }
  Log("array_prisa_search seeds=" + std::to_string(seeds) +
      " sis=" + std::to_string(GridUsesPRISASIS() ? 1 : 0));
  return best;
}

double PrisaEngine::GridPlacementCost(const PlacementState& state) const {
  double cost = 0.0;
  for (const auto& edge : edges_) {
    const int from = state.dfg_to_cell[edge.source];
    const int to = state.dfg_to_cell[edge.target];
    if (from < 0 || to < 0) return kGridImpossibleCost;
    cost += GridDistanceCost(from, to);
  }
  return cost;
}

PlacementQuality PrisaEngine::GridComputePlacementQuality(
    const PlacementState& state) const {
  PlacementQuality quality;
  quality.cost = 0.0;
  quality.direct_edges = 0;
  quality.max_distance = 0;
  for (const auto& edge : edges_) {
    const int from = state.dfg_to_cell[edge.source];
    const int to = state.dfg_to_cell[edge.target];
    if (from < 0 || to < 0) {
      quality.cost = kGridImpossibleCost;
      return quality;
    }
    const int distance = GridDistanceCost(from, to);
    quality.cost += distance;
    if (distance <= 1) quality.direct_edges++;
    quality.max_distance = std::max(quality.max_distance, distance);
  }
  return quality;
}

bool PrisaEngine::GridIsBetterQuality(const PlacementQuality& candidate,
                                      const PlacementQuality& best) const {
  if (candidate.cost + 1.0e-9 < best.cost) return true;
  if (candidate.cost > best.cost + 1.0e-9) return false;
  if (candidate.direct_edges != best.direct_edges &&
      candidate.max_distance <= best.max_distance) {
    return candidate.direct_edges > best.direct_edges;
  }
  if (candidate.max_distance != best.max_distance) {
    return candidate.max_distance < best.max_distance;
  }
  return candidate.direct_edges > best.direct_edges;
}

int PrisaEngine::GridPRISAWeakEdgeCount(const PlacementState& state) const {
  int result = 0;
  for (int edge_id = 0; edge_id < static_cast<int>(edges_.size()); edge_id++) {
    bool is_weak = false;
    bool is_potential = false;
    if (GridPRISAEdgeRegion(state, edge_id, &is_weak, &is_potential) &&
        is_weak) {
      result++;
    }
  }
  return result;
}

GridCostAwarePRISAMetrics PrisaEngine::GridComputeCostAwarePRISAMetrics(
    const PlacementState& state) const {
  GridCostAwarePRISAMetrics metrics;
  const int node_count = dfg_.GetNodeNum();
  std::vector<int> indegree(node_count, 0);
  std::vector<std::vector<std::pair<int, int>>> weighted_successors(node_count);

  for (const auto& edge : edges_) {
    const int from = state.dfg_to_cell[edge.source];
    const int to = state.dfg_to_cell[edge.target];
    if (from < 0 || to < 0) {
      metrics.mesh_hop_sum = kGridImpossibleCost;
      metrics.max_mesh_hop = std::numeric_limits<int>::max();
      metrics.mapped_lp_mesh_hop = std::numeric_limits<int>::max();
      return metrics;
    }
    const int hop = GridDistanceCost(from, to);
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

double PrisaEngine::GridCostAwarePRISAScore(const PlacementState& state) const {
  const int weak_edges = GridPRISAWeakEdgeCount(state);
  const GridCostAwarePRISAMetrics metrics =
      GridComputeCostAwarePRISAMetrics(state);
  if (metrics.mesh_hop_sum >= kGridImpossibleCost ||
      metrics.max_mesh_hop == std::numeric_limits<int>::max() ||
      metrics.mapped_lp_mesh_hop == std::numeric_limits<int>::max()) {
    return kGridImpossibleCost;
  }
  return weak_edges * 1000000.0 + metrics.mesh_hop_sum * 50.0 +
         metrics.mapped_lp_mesh_hop * 100.0 + metrics.max_mesh_hop * 200.0 -
         metrics.direct_edge_count * 80.0;
}

double PrisaEngine::GridPRISAAcceptanceScore(
    const PlacementState& state) const {
  if (GridUsesCostAwarePRISA()) return GridCostAwarePRISAScore(state);
  return GridPlacementCost(state);
}

std::vector<int> PrisaEngine::GridCompatibleCells(
    int dfg_node, const PlacementState& state) const {
  std::vector<int> cells;
  cells.reserve(compatible_cells_[dfg_node].size());
  for (int cell : compatible_cells_[dfg_node]) {
    if (GridCanPlace(dfg_node, cell, state)) cells.push_back(cell);
  }
  return cells;
}

void PrisaEngine::GridPlaceNode(int dfg_node, int cell,
                                PlacementState& state) const {
  state.dfg_to_cell[dfg_node] = cell;
  state.cell_to_dfg[cell] = dfg_node;
}

// Shared traversal graph utilities used by paper-guided and cpu_mapping
// YOTO/YOTT variants. Method-specific files decide how these scores are used.

std::vector<int> PrisaEngine::GridBuildCriticalPathScore() const {
  const int n = dfg_.GetNodeNum();
  std::vector<int> memo(n, -1);
  std::vector<char> visiting(n, false);
  std::function<int(int)> dfs = [&](int node) {
    if (memo[node] >= 0) return memo[node];
    if (visiting[node]) return 0;
    visiting[node] = true;
    int best = 0;
    for (int successor : successors_[node]) {
      if (successor == node) continue;
      best = std::max(best, 1 + dfs(successor));
    }
    visiting[node] = false;
    memo[node] = best;
    return best;
  };
  for (int node = 0; node < n; node++) dfs(node);
  return memo;
}

}  // namespace mapper::prisa
