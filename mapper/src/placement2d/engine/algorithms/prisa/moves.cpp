#include "../../array_engine_internal.hpp"

namespace mapper::detail::placement2d {

// PR/WR region accounting and local swap deltas.

bool Placement2DArrayEngine::PRISAIsWeak(int row, int column) const {
  const int resource_count = static_cast<int>(prisa_cell_order_.size());
  if (row == column || row < 0 || column < 0 || row >= resource_count ||
      column >= resource_count) {
    return false;
  }
  return prisa_weak_region_matrix_[row * resource_count + column] != 0;
}

bool Placement2DArrayEngine::PRISAIsPotential(int row, int column) const {
  return row != column && !PRISAIsWeak(row, column);
}

bool Placement2DArrayEngine::PRISAEdgeRegion(const PlacementState& state,
                                             int edge_id, bool* is_weak,
                                             bool* is_potential) const {
  const auto& edge = edges_[edge_id];
  const int source_cell = state.dfg_to_cell[edge.source];
  const int target_cell = state.dfg_to_cell[edge.target];
  if (source_cell < 0 || target_cell < 0) return false;
  const int row = prisa_order_index_[source_cell];
  const int column = prisa_order_index_[target_cell];
  if (row < 0 || column < 0 || row == column) return false;
  *is_weak = PRISAIsWeak(row, column);
  *is_potential = !*is_weak;
  return true;
}

void Placement2DArrayEngine::AddPRISAEdgeToStats(const PlacementState& state,
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
  if (!PRISAEdgeRegion(state, edge_id, &is_weak, &is_potential)) return;
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

PRISAStats Placement2DArrayEngine::InitializePRISAStats(
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
    AddPRISAEdgeToStats(state, edge_id, stats, 1);
  }
  return stats;
}

std::vector<int> Placement2DArrayEngine::AffectedEdgesForCellPair(
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

int Placement2DArrayEngine::CellAfterSwap(int cell, int first_cell,
                                          int second_cell) const {
  if (cell == first_cell) return second_cell;
  if (cell == second_cell) return first_cell;
  return cell;
}

int Placement2DArrayEngine::EdgeWeakAfterSwap(const PlacementState& state,
                                              int edge_id, int first_cell,
                                              int second_cell) const {
  const auto& edge = edges_[edge_id];
  const int source_cell =
      CellAfterSwap(state.dfg_to_cell[edge.source], first_cell, second_cell);
  const int target_cell =
      CellAfterSwap(state.dfg_to_cell[edge.target], first_cell, second_cell);
  const int row = prisa_order_index_[source_cell];
  const int column = prisa_order_index_[target_cell];
  return PRISAIsWeak(row, column) ? 1 : 0;
}

int Placement2DArrayEngine::PRISAWeakDeltaForCellSwap(
    const PlacementState& state, int first_cell, int second_cell,
    const std::vector<int>& affected_edges) const {
  int before = 0;
  int after = 0;
  for (int edge_id : affected_edges) {
    bool is_weak = false;
    bool is_potential = false;
    if (PRISAEdgeRegion(state, edge_id, &is_weak, &is_potential) && is_weak) {
      before++;
    }
    after += EdgeWeakAfterSwap(state, edge_id, first_cell, second_cell);
  }
  return after - before;
}

double Placement2DArrayEngine::PlacementCostDeltaForCellSwap(
    const PlacementState& state, int first_cell, int second_cell,
    const std::vector<int>& affected_edges) const {
  double before = 0.0;
  double after = 0.0;
  for (int edge_id : affected_edges) {
    const auto& edge = edges_[edge_id];
    const int source_cell = state.dfg_to_cell[edge.source];
    const int target_cell = state.dfg_to_cell[edge.target];
    before += DistanceCost(source_cell, target_cell);
    after += DistanceCost(CellAfterSwap(source_cell, first_cell, second_cell),
                          CellAfterSwap(target_cell, first_cell, second_cell));
  }
  return after - before;
}

int Placement2DArrayEngine::DirectEdgeDeltaForCellSwap(
    const PlacementState& state, int first_cell, int second_cell,
    const std::vector<int>& affected_edges) const {
  int before = 0;
  int after = 0;
  for (int edge_id : affected_edges) {
    const auto& edge = edges_[edge_id];
    const int source_cell = state.dfg_to_cell[edge.source];
    const int target_cell = state.dfg_to_cell[edge.target];
    before += DistanceCost(source_cell, target_cell) <= 1 ? 1 : 0;
    after +=
        DistanceCost(CellAfterSwap(source_cell, first_cell, second_cell),
                     CellAfterSwap(target_cell, first_cell, second_cell)) <= 1
            ? 1
            : 0;
  }
  return after - before;
}

int Placement2DArrayEngine::AffectedMaxDistanceAfterCellSwap(
    const PlacementState& state, int first_cell, int second_cell,
    const std::vector<int>& affected_edges) const {
  int result = 0;
  for (int edge_id : affected_edges) {
    const auto& edge = edges_[edge_id];
    const int source_cell = state.dfg_to_cell[edge.source];
    const int target_cell = state.dfg_to_cell[edge.target];
    result = std::max(
        result,
        DistanceCost(CellAfterSwap(source_cell, first_cell, second_cell),
                     CellAfterSwap(target_cell, first_cell, second_cell)));
  }
  return result;
}

bool Placement2DArrayEngine::CanSwapCells(const PlacementState& state,
                                          int first_cell,
                                          int second_cell) const {
  if (first_cell == second_cell || first_cell < 0 || second_cell < 0) {
    return false;
  }
  const int first_node = state.cell_to_dfg[first_cell];
  const int second_node = state.cell_to_dfg[second_cell];
  if (first_node < 0 && second_node < 0) return false;

  // Structural-I/O placement assigns source/sink nodes to logical I/O cells
  // and all other nodes to compute cells. IsCompatible() intentionally skips
  // that logical type check, so every move must restore it explicitly.
  const auto can_occupy = [this](int node, int cell) {
    if (node < 0) return true;
    if (UsesStructuralIOCellTypes()) {
      return IsCPUMappingCompatible(node, cell);
    }
    return IsCompatible(node, cell);
  };
  if (!can_occupy(first_node, second_cell)) {
    return false;
  }
  if (!can_occupy(second_node, first_cell)) {
    return false;
  }
  return true;
}

bool Placement2DArrayEngine::ApplyCellSwapWithStats(PlacementState& state,
                                                    PRISAStats& stats,
                                                    int first_cell,
                                                    int second_cell) const {
  if (!CanSwapCells(state, first_cell, second_cell)) return false;
  const auto affected_edges =
      AffectedEdgesForCellPair(state, first_cell, second_cell);
  for (int edge_id : affected_edges) {
    AddPRISAEdgeToStats(state, edge_id, stats, -1);
  }

  const int first_node = state.cell_to_dfg[first_cell];
  const int second_node = state.cell_to_dfg[second_cell];
  state.cell_to_dfg[first_cell] = second_node;
  state.cell_to_dfg[second_cell] = first_node;
  if (first_node >= 0) state.dfg_to_cell[first_node] = second_cell;
  if (second_node >= 0) state.dfg_to_cell[second_node] = first_cell;

  for (int edge_id : affected_edges) {
    AddPRISAEdgeToStats(state, edge_id, stats, 1);
  }
  return true;
}

bool Placement2DArrayEngine::ApplyCellSwap(PlacementState& state,
                                           int first_cell,
                                           int second_cell) const {
  if (!CanSwapCells(state, first_cell, second_cell)) return false;
  const int first_node = state.cell_to_dfg[first_cell];
  const int second_node = state.cell_to_dfg[second_cell];
  state.cell_to_dfg[first_cell] = second_node;
  state.cell_to_dfg[second_cell] = first_node;
  if (first_node >= 0) state.dfg_to_cell[first_node] = second_cell;
  if (second_node >= 0) state.dfg_to_cell[second_node] = first_cell;
  return true;
}

std::optional<Placement2DArrayEngine::PRISAProposal>
Placement2DArrayEngine::ProposeRandomPRISASwap(const PlacementState& state) {
  if (prisa_cell_order_.size() < 2) return std::nullopt;
  std::uniform_int_distribution<int> dist(
      0, static_cast<int>(prisa_cell_order_.size()) - 1);
  for (int attempt = 0; attempt < 32; attempt++) {
    const int first_cell = prisa_cell_order_[dist(rng_)];
    const int second_cell = prisa_cell_order_[dist(rng_)];
    if (!CanSwapCells(state, first_cell, second_cell)) continue;
    const auto affected_edges =
        AffectedEdgesForCellPair(state, first_cell, second_cell);
    return PRISAProposal{first_cell, second_cell,
                         PlacementCostDeltaForCellSwap(
                             state, first_cell, second_cell, affected_edges),
                         DirectEdgeDeltaForCellSwap(
                             state, first_cell, second_cell, affected_edges),
                         AffectedMaxDistanceAfterCellSwap(
                             state, first_cell, second_cell, affected_edges)};
  }
  return std::nullopt;
}

std::optional<Placement2DArrayEngine::PRISAProposal>
Placement2DArrayEngine::ProposePlacementCostSampledSwap(
    const PlacementState& state, int requested_sample_count) {
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
    if (UsesCostAwarePRISA()) {
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
    if (!CanSwapCells(state, first_cell, second_cell)) return;
    const auto affected_edges =
        AffectedEdgesForCellPair(state, first_cell, second_cell);
    const double cost_delta = PlacementCostDeltaForCellSwap(
        state, first_cell, second_cell, affected_edges);
    PRISAProposal candidate{
        first_cell, second_cell, cost_delta,
        DirectEdgeDeltaForCellSwap(state, first_cell, second_cell,
                                   affected_edges),
        AffectedMaxDistanceAfterCellSwap(state, first_cell, second_cell,
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
  return ProposeRandomPRISASwap(state);
}

// PRISA move proposal. The base variant tries to repair weak-region edges;
// cost-aware variants use additional direct-edge and hop-distance terms.

int Placement2DArrayEngine::PRISALightQualitySampleCount() const {
  const int resource_count = static_cast<int>(prisa_cell_order_.size());
  // SIS already gives PRISA a low-bandwidth starting point.  Keep the
  // post-WR quality refinement cheaper than no-SIS so the runtime trend
  // matches the paper without using the cost-aware derived mapper.
  if (UsesPRISASIS()) return std::max(8, resource_count / 3);
  return std::max(16, resource_count);
}

double Placement2DArrayEngine::CostAwarePRISALocalSwapScore(
    const PlacementState& state, int first_cell, int second_cell,
    const std::vector<int>& affected_edges) const {
  double mesh_delta = 0.0;
  int after_max_hop = 0;
  int direct_edge_gain = 0;
  for (int edge_id : affected_edges) {
    const auto& edge = edges_[edge_id];
    const int source_cell = state.dfg_to_cell[edge.source];
    const int target_cell = state.dfg_to_cell[edge.target];
    const int before = DistanceCost(source_cell, target_cell);
    const int after =
        DistanceCost(CellAfterSwap(source_cell, first_cell, second_cell),
                     CellAfterSwap(target_cell, first_cell, second_cell));
    mesh_delta += after - before;
    after_max_hop = std::max(after_max_hop, after);
    if (before <= 1 && after > 1) direct_edge_gain--;
    if (before > 1 && after <= 1) direct_edge_gain++;
  }

  const int weak_delta =
      PRISAWeakDeltaForCellSwap(state, first_cell, second_cell, affected_edges);
  return weak_delta * 1000000.0 + mesh_delta * 100.0 + after_max_hop * 20.0 -
         direct_edge_gain * 80.0;
}

int Placement2DArrayEngine::CostAwarePRISACandidateSampleCount() const {
  if (max_trials_.has_value()) return std::max(16, max_trials_.value());
  return 512;
}

int Placement2DArrayEngine::CostAwarePRISAFullEvaluationCount() const {
  return 8;
}

std::optional<Placement2DArrayEngine::PRISAProposal>
Placement2DArrayEngine::ProposeCostAwarePRISASwap(const PlacementState& state) {
  const int resource_count = static_cast<int>(prisa_cell_order_.size());
  if (resource_count < 2) return std::nullopt;

  const int total_pairs = resource_count * (resource_count - 1) / 2;
  const int sample_count =
      std::min(total_pairs, CostAwarePRISACandidateSampleCount());
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
      edge_hops.push_back({DistanceCost(source_cell, target_cell), edge_id});
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
    if (!CanSwapCells(state, first_cell, second_cell)) continue;
    const auto affected_edges =
        AffectedEdgesForCellPair(state, first_cell, second_cell);
    candidates.push_back({CostAwarePRISALocalSwapScore(
                              state, first_cell, second_cell, affected_edges),
                          first_cell, second_cell, random_order++});
  }
  if (candidates.empty()) return ProposeRandomPRISASwap(state);

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
    if (!ApplyCellSwap(next, candidates[i].first_cell,
                       candidates[i].second_cell)) {
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
  return PRISAProposal{chosen.first_cell, chosen.second_cell,
                       chosen.score - current_score, 0, 0};
}

std::vector<int> Placement2DArrayEngine::PRISAWeakCounterparts(
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

bool Placement2DArrayEngine::IsBetterPRISACandidate(
    const PRISACandidate& candidate, const PRISACandidate& best) const {
  if (UsesCostAwarePRISA()) {
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

std::optional<Placement2DArrayEngine::PRISACandidate>
Placement2DArrayEngine::BestPRISACandidateFor(const PlacementState& state,
                                              const PRISAStats& stats,
                                              int first_index, bool row_side) {
  const auto weak_counterparts =
      PRISAWeakCounterparts(state, stats, first_index, row_side);
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
    if (!CanSwapCells(state, first_cell, second_cell)) continue;

    int fixed_weak_count = 0;
    for (int counterpart : weak_counterparts) {
      const bool moves_to_pr =
          row_side ? PRISAIsPotential(second_index, counterpart)
                   : PRISAIsPotential(counterpart, second_index);
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
                          DistanceCost(first_cell, second_cell), offset});
    }
  }

  std::optional<PRISACandidate> best;
  for (const auto& key : filtered) {
    const int second_cell = prisa_cell_order_[key.second_index];
    const auto affected_edges =
        AffectedEdgesForCellPair(state, first_cell, second_cell);
    PRISACandidate candidate{
        first_index,
        key.second_index,
        key.fixed_weak_count,
        key.potential_count,
        PRISAWeakDeltaForCellSwap(state, first_cell, second_cell,
                                  affected_edges),
        PlacementCostDeltaForCellSwap(state, first_cell, second_cell,
                                      affected_edges),
        DirectEdgeDeltaForCellSwap(state, first_cell, second_cell,
                                   affected_edges),
        AffectedMaxDistanceAfterCellSwap(state, first_cell, second_cell,
                                         affected_edges),
        key.spatial_distance,
        key.random_order};
    if (!best.has_value() || IsBetterPRISACandidate(candidate, *best)) {
      best = candidate;
    }
  }
  return best;
}

int Placement2DArrayEngine::RandomArgmax(const std::vector<int>& values) {
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

std::optional<Placement2DArrayEngine::PRISAProposal>
Placement2DArrayEngine::ProposePRISAMove(const PlacementState& state,
                                         const PRISAStats& stats) {
  if (UsesCostAwarePRISA()) return ProposeCostAwarePRISASwap(state);
  if (stats.weak_edges <= 0) {
    // After all weak-region edges are repaired, PRISA should not fall into
    // the expensive cost-aware search used by the derived mapper.  A small
    // random sample keeps the paper-style random-swap refinement cheap while
    // still allowing annealing to improve placement quality.
    return ProposePlacementCostSampledSwap(state,
                                           PRISALightQualitySampleCount());
  }

  const int row_index = RandomArgmax(stats.row_weak);
  const int column_index = RandomArgmax(stats.column_weak);
  if (row_index < 0 || column_index < 0) {
    return ProposePlacementCostSampledSwap(state,
                                           PRISALightQualitySampleCount());
  }
  const int row_max = stats.row_weak[row_index];
  const int column_max = stats.column_weak[column_index];
  if (row_max <= 0 && column_max <= 0) {
    return ProposePlacementCostSampledSwap(state,
                                           PRISALightQualitySampleCount());
  }

  std::optional<PRISACandidate> candidate;
  const bool prefer_row = row_max > column_max ||
                          (row_max == column_max &&
                           std::uniform_int_distribution<int>(0, 1)(rng_) == 0);
  if (prefer_row) {
    candidate = BestPRISACandidateFor(state, stats, row_index, true);
    if (!candidate.has_value()) {
      candidate = BestPRISACandidateFor(state, stats, column_index, false);
    }
  } else {
    candidate = BestPRISACandidateFor(state, stats, column_index, false);
    if (!candidate.has_value()) {
      candidate = BestPRISACandidateFor(state, stats, row_index, true);
    }
  }
  if (!candidate.has_value()) {
    return ProposePlacementCostSampledSwap(state,
                                           PRISALightQualitySampleCount());
  }
  return PRISAProposal{prisa_cell_order_[candidate->first_index],
                       prisa_cell_order_[candidate->second_index],
                       candidate->cost_delta, candidate->direct_delta,
                       candidate->affected_max_after};
}

}  // namespace mapper::detail::placement2d
