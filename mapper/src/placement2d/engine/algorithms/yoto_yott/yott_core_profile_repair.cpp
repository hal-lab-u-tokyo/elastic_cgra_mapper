#include "../../array_engine_internal.hpp"

namespace mapper::detail::placement2d {
namespace {

constexpr int kActiveNodeLimit = 16;
constexpr int kChainEmptyCellLimit = 6;

bool ContainsNode(const std::vector<std::pair<int, int>>& assignments,
                  int node) {
  return std::any_of(
      assignments.begin(), assignments.end(),
      [node](const auto& assignment) { return assignment.first == node; });
}

struct FifoProfileComparison {
  bool nonworse = true;
  bool strictly_better = false;
};

FifoProfileComparison CompareFifoProfiles(const std::vector<int>& candidate,
                                          const std::vector<int>& current) {
  const int max_fifo = std::max(static_cast<int>(candidate.size()),
                                static_cast<int>(current.size())) -
                       1;
  int candidate_tail_count = 0;
  int current_tail_count = 0;
  FifoProfileComparison result;
  for (int fifo = max_fifo; fifo >= 1; fifo--) {
    if (fifo < static_cast<int>(candidate.size())) {
      candidate_tail_count += candidate[fifo];
    }
    if (fifo < static_cast<int>(current.size())) {
      current_tail_count += current[fifo];
    }
    if (candidate_tail_count > current_tail_count) {
      result.nonworse = false;
      return result;
    }
    if (candidate_tail_count < current_tail_count) {
      result.strictly_better = true;
    }
  }
  return result;
}

bool IsBetterProfileRepairSummary(const ProfileRepairMetrics& candidate,
                                  const ProfileRepairMetrics& current) {
  if (candidate.non_direct_cost_edges != current.non_direct_cost_edges) {
    return candidate.non_direct_cost_edges < current.non_direct_cost_edges;
  }
  if (candidate.total_cost_fifo != current.total_cost_fifo) {
    return candidate.total_cost_fifo < current.total_cost_fifo;
  }
  if (candidate.max_cost_fifo != current.max_cost_fifo) {
    return candidate.max_cost_fifo < current.max_cost_fifo;
  }
  return false;
}

bool IsBetterBalancedRepairSummary(const ProfileRepairMetrics& candidate,
                                   const ProfileRepairMetrics& current) {
  if (candidate.max_cost_fifo != current.max_cost_fifo) {
    return candidate.max_cost_fifo < current.max_cost_fifo;
  }
  if (candidate.non_direct_cost_edges != current.non_direct_cost_edges) {
    return candidate.non_direct_cost_edges < current.non_direct_cost_edges;
  }
  return candidate.total_cost_fifo < current.total_cost_fifo;
}

bool SatisfiesBalancedRepairGuard(const ProfileRepairMetrics& candidate,
                                  const ProfileRepairContext& context,
                                  int edge_count) {
  return edge_count - candidate.non_direct_cost_edges >=
             context.minimum_direct_edges &&
         candidate.total_cost_fifo <= context.maximum_total_cost_fifo &&
         candidate.max_cost_fifo <= context.maximum_cost_fifo;
}

}  // namespace

// Sparse-QAP neighborhood descent over selected YOTTCore placements. Tail
// repair requires stochastic FIFO-profile improvement. Balanced repair instead
// minimizes maximal FIFO under direct-edge and total-FIFO baseline guards.

ProfileRepairContext Placement2DArrayEngine::BuildProfileRepairContext(
    const PlacementState& state) const {
  ProfileRepairContext context;
  context.edge_cost_fifo.resize(edges_.size(), 0);
  context.cost_fifo_histogram.resize(1, 0);

  for (int edge_id = 0; edge_id < static_cast<int>(edges_.size()); edge_id++) {
    const auto& edge = edges_[edge_id];
    const int source_cell = state.dfg_to_cell[edge.source];
    const int target_cell = state.dfg_to_cell[edge.target];
    const int cost_fifo =
        std::max(0, DistanceCost(source_cell, target_cell) - 1);
    context.edge_cost_fifo[edge_id] = cost_fifo;
    if (cost_fifo >= static_cast<int>(context.cost_fifo_histogram.size())) {
      context.cost_fifo_histogram.resize(cost_fifo + 1, 0);
    }
    context.cost_fifo_histogram[cost_fifo]++;
    context.metrics.total_cost_fifo += cost_fifo;
    context.metrics.non_direct_cost_edges += cost_fifo > 0 ? 1 : 0;
    context.metrics.max_cost_fifo =
        std::max(context.metrics.max_cost_fifo, cost_fifo);
  }
  return context;
}

std::vector<int> Placement2DArrayEngine::ProfileRepairActiveNodes(
    const ProfileRepairContext& context) const {
  std::vector<int> pressure(dfg_.GetNodeNum(), 0);
  for (int edge_id = 0; edge_id < static_cast<int>(edges_.size()); edge_id++) {
    const int pressure_delta = context.edge_cost_fifo[edge_id];
    if (pressure_delta <= 0) continue;
    pressure[edges_[edge_id].source] += pressure_delta;
    pressure[edges_[edge_id].target] += pressure_delta;
  }

  std::vector<int> nodes;
  for (int node = 0; node < dfg_.GetNodeNum(); node++) {
    if (pressure[node] > 0) nodes.push_back(node);
  }
  std::sort(nodes.begin(), nodes.end(), [&](int lhs, int rhs) {
    if (pressure[lhs] != pressure[rhs]) return pressure[lhs] > pressure[rhs];
    if (degree_[lhs] != degree_[rhs]) return degree_[lhs] > degree_[rhs];
    return lhs < rhs;
  });
  if (static_cast<int>(nodes.size()) > kActiveNodeLimit) {
    nodes.resize(kActiveNodeLimit);
  }
  return nodes;
}

std::vector<int> Placement2DArrayEngine::ProfileRepairEmptyCells(
    const PlacementState& state, int node) const {
  std::vector<int> cells;
  for (int cell : compatible_cells_[node]) {
    if (state.cell_to_dfg[cell] < 0 && IsCPUMappingCompatible(node, cell)) {
      cells.push_back(cell);
    }
  }
  return cells;
}

std::vector<int> Placement2DArrayEngine::ProfileRepairPreferredEmptyCells(
    const PlacementState& state, int node, int limit) const {
  std::vector<std::tuple<int, int, int>> ranked;
  for (int cell : ProfileRepairEmptyCells(state, node)) {
    int non_direct = 0;
    int total_fifo = 0;
    for (int edge_id : incident_edge_ids_[node]) {
      const auto& edge = edges_[edge_id];
      const int other = edge.source == node ? edge.target : edge.source;
      const int other_cell = state.dfg_to_cell[other];
      const int fifo = std::max(0, DistanceCost(cell, other_cell) - 1);
      non_direct += fifo > 0 ? 1 : 0;
      total_fifo += fifo;
    }
    ranked.push_back({non_direct, total_fifo, cell});
  }
  std::sort(ranked.begin(), ranked.end());
  if (static_cast<int>(ranked.size()) > limit) ranked.resize(limit);

  std::vector<int> result;
  result.reserve(ranked.size());
  for (const auto& [unused_non_direct, unused_fifo, cell] : ranked) {
    (void)unused_non_direct;
    (void)unused_fifo;
    result.push_back(cell);
  }
  return result;
}

std::vector<int> Placement2DArrayEngine::ProfileRepairSwapNodes(
    const PlacementState& state, int node) const {
  std::vector<char> seen(dfg_.GetNodeNum(), 0);
  std::vector<int> frontier = {node};
  seen[node] = 1;

  // Communication-graph distance two is the sparse-QAP neighborhood. It is
  // augmented by occupants of scarce one-hop cells around placed neighbors.
  for (int depth = 0; depth < 2; depth++) {
    std::vector<int> next;
    for (int current : frontier) {
      for (int edge_id : incident_edge_ids_[current]) {
        const auto& edge = edges_[edge_id];
        const int other = edge.source == current ? edge.target : edge.source;
        if (!seen[other]) {
          seen[other] = 1;
          next.push_back(other);
        }
      }
    }
    frontier = std::move(next);
  }

  for (int edge_id : incident_edge_ids_[node]) {
    const auto& edge = edges_[edge_id];
    const int neighbor = edge.source == node ? edge.target : edge.source;
    const int neighbor_cell = state.dfg_to_cell[neighbor];
    for (int cell : compatible_cells_[node]) {
      if (!IsCPUMappingCompatible(node, cell)) continue;
      const int occupant = state.cell_to_dfg[cell];
      if (occupant >= 0 && occupant != node &&
          DistanceCost(cell, neighbor_cell) <= 1) {
        seen[occupant] = 1;
      }
    }
  }

  std::vector<int> result;
  const int node_cell = state.dfg_to_cell[node];
  for (int other = 0; other < dfg_.GetNodeNum(); other++) {
    if (!seen[other] || other == node) continue;
    const int other_cell = state.dfg_to_cell[other];
    if (IsCPUMappingCompatible(node, other_cell) &&
        IsCPUMappingCompatible(other, node_cell)) {
      result.push_back(other);
    }
  }
  return result;
}

std::optional<ProfileRepairMove>
Placement2DArrayEngine::EvaluateProfileRepairMove(
    const PlacementState& state, const ProfileRepairContext& context,
    const std::vector<std::pair<int, int>>& assignments) const {
  if (assignments.empty()) return std::nullopt;

  std::vector<int> targets;
  targets.reserve(assignments.size());
  for (const auto& [node, target_cell] : assignments) {
    if (node < 0 || node >= dfg_.GetNodeNum() || target_cell < 0 ||
        target_cell >= rows_ * cols_ ||
        !IsCPUMappingCompatible(node, target_cell) ||
        state.dfg_to_cell[node] == target_cell) {
      return std::nullopt;
    }
    const int occupant = state.cell_to_dfg[target_cell];
    if (occupant >= 0 && !ContainsNode(assignments, occupant)) {
      return std::nullopt;
    }
    targets.push_back(target_cell);
  }
  std::sort(targets.begin(), targets.end());
  if (std::adjacent_find(targets.begin(), targets.end()) != targets.end()) {
    return std::nullopt;
  }
  for (int i = 0; i < static_cast<int>(assignments.size()); i++) {
    for (int j = i + 1; j < static_cast<int>(assignments.size()); j++) {
      if (assignments[i].first == assignments[j].first) return std::nullopt;
    }
  }

  std::vector<int> affected_edges;
  for (const auto& [node, unused_cell] : assignments) {
    (void)unused_cell;
    affected_edges.insert(affected_edges.end(),
                          incident_edge_ids_[node].begin(),
                          incident_edge_ids_[node].end());
  }
  std::sort(affected_edges.begin(), affected_edges.end());
  affected_edges.erase(
      std::unique(affected_edges.begin(), affected_edges.end()),
      affected_edges.end());
  if (affected_edges.empty()) return std::nullopt;

  auto cell_after_move = [&](int node) {
    for (const auto& [moved_node, target_cell] : assignments) {
      if (moved_node == node) return target_cell;
    }
    return state.dfg_to_cell[node];
  };

  ProfileRepairMetrics metrics = context.metrics;
  std::vector<int> cost_histogram = context.cost_fifo_histogram;
  for (int edge_id : affected_edges) {
    const int before_cost_fifo = context.edge_cost_fifo[edge_id];
    const auto& edge = edges_[edge_id];
    const int after_cost_fifo =
        std::max(0, DistanceCost(cell_after_move(edge.source),
                                 cell_after_move(edge.target)) -
                        1);
    if (after_cost_fifo >= static_cast<int>(cost_histogram.size())) {
      cost_histogram.resize(after_cost_fifo + 1, 0);
    }
    cost_histogram[before_cost_fifo]--;
    cost_histogram[after_cost_fifo]++;
    metrics.total_cost_fifo += after_cost_fifo - before_cost_fifo;
    metrics.non_direct_cost_edges +=
        (after_cost_fifo > 0 ? 1 : 0) - (before_cost_fifo > 0 ? 1 : 0);
  }
  metrics.max_cost_fifo = 0;
  for (int fifo = static_cast<int>(cost_histogram.size()) - 1; fifo >= 0;
       fifo--) {
    if (cost_histogram[fifo] > 0) {
      metrics.max_cost_fifo = fifo;
      break;
    }
  }
  RecordPlacementSwapAttempts();
  if (context.prioritize_max_fifo) {
    if (!SatisfiesBalancedRepairGuard(metrics, context, edges_.size()) ||
        !IsBetterBalancedRepairSummary(metrics, context.metrics)) {
      return std::nullopt;
    }
    return ProfileRepairMove{assignments, metrics};
  }

  const FifoProfileComparison cost_profile =
      CompareFifoProfiles(cost_histogram, context.cost_fifo_histogram);
  if (!cost_profile.nonworse || !cost_profile.strictly_better) {
    return std::nullopt;
  }
  return ProfileRepairMove{assignments, metrics};
}

void Placement2DArrayEngine::ApplyProfileRepairMove(
    PlacementState& state, const ProfileRepairMove& move) const {
  for (const auto& [node, unused_target] : move.assignments) {
    (void)unused_target;
    state.cell_to_dfg[state.dfg_to_cell[node]] = -1;
  }
  for (const auto& [node, target_cell] : move.assignments) {
    state.dfg_to_cell[node] = target_cell;
    state.cell_to_dfg[target_cell] = node;
  }
}

std::optional<ProfileRepairMove>
Placement2DArrayEngine::FindBestProfileRepairMove(
    const PlacementState& state, const ProfileRepairContext& context) const {
  std::optional<ProfileRepairMove> best;
  auto consider = [&](std::optional<ProfileRepairMove> candidate) {
    if (!candidate.has_value()) return;
    const bool better =
        !best.has_value() ||
        (context.prioritize_max_fifo
             ? IsBetterBalancedRepairSummary(candidate->metrics, best->metrics)
             : IsBetterProfileRepairSummary(candidate->metrics, best->metrics));
    if (better) {
      best = std::move(candidate);
    }
  };

  const std::vector<int> active_nodes = ProfileRepairActiveNodes(context);
  for (int node : active_nodes) {
    for (int empty_cell : ProfileRepairEmptyCells(state, node)) {
      consider(EvaluateProfileRepairMove(state, context, {{node, empty_cell}}));
    }
  }

  for (int node : active_nodes) {
    const int node_cell = state.dfg_to_cell[node];
    for (int other : ProfileRepairSwapNodes(state, node)) {
      const int other_cell = state.dfg_to_cell[other];
      consider(EvaluateProfileRepairMove(
          state, context, {{node, other_cell}, {other, node_cell}}));
    }
  }

  for (int node : active_nodes) {
    for (int other : ProfileRepairSwapNodes(state, node)) {
      const int other_cell = state.dfg_to_cell[other];
      for (int empty_cell : ProfileRepairPreferredEmptyCells(
               state, other, kChainEmptyCellLimit)) {
        consider(EvaluateProfileRepairMove(
            state, context, {{node, other_cell}, {other, empty_cell}}));
      }
    }
  }

  return best;
}

PlacementState Placement2DArrayEngine::PolishProfileRepairPlacement(
    PlacementState state, std::chrono::steady_clock::time_point start) {
  const int budget =
      std::min(MaxIterations(), std::max(8, static_cast<int>(edges_.size())));
  for (int iteration = 0; iteration < budget && !HasTimedOut(start, 0.03);
       iteration++) {
    const ProfileRepairContext context = BuildProfileRepairContext(state);
    auto move = FindBestProfileRepairMove(state, context);
    if (!move.has_value()) break;
    ApplyProfileRepairMove(state, *move);
  }
  return state;
}

PlacementState Placement2DArrayEngine::PolishBalancedCoreRepairPlacement(
    PlacementState state, const ProfileRepairMetrics& guard,
    std::chrono::steady_clock::time_point start) {
  const int budget =
      std::min(MaxIterations(), std::max(8, static_cast<int>(edges_.size())));
  for (int iteration = 0; iteration < budget && !HasTimedOut(start, 0.03);
       iteration++) {
    ProfileRepairContext context = BuildProfileRepairContext(state);
    context.prioritize_max_fifo = true;
    context.minimum_direct_edges =
        static_cast<int>(edges_.size()) - guard.non_direct_cost_edges;
    context.maximum_total_cost_fifo = guard.total_cost_fifo;
    context.maximum_cost_fifo = guard.max_cost_fifo;
    auto move = FindBestProfileRepairMove(state, context);
    if (!move.has_value()) break;
    ApplyProfileRepairMove(state, *move);
  }
  return state;
}

std::optional<PlacementState>
Placement2DArrayEngine::RunCoreTailRepairMultiStart(
    std::chrono::steady_clock::time_point start) {
  std::vector<std::pair<double, PlacementState>> elites;
  const int keep_count = std::max(1, ElitePlacementCount());
  int trials = 0;

  std::ofstream trace;
  if (TraceTrials()) {
    trace.open(TrialTracePath());
    if (trace) WriteTrialTraceHeader(trace);
  }

  for (int seed = 0; seed < SeedCount() && !HasTimedOut(start, 0.05); seed++) {
    ResetSeed(seed);
    for (int trial = 0; trial < MaxTrials() && !HasTimedOut(start, 0.05);
         trial++) {
      if (UsesPerTrialSeeds()) ResetTrialSeed(seed, trial);
      const std::vector<Step> plan = BuildYOTTCorePlan();
      auto placement = ConstructYOTTCorePlacement(plan);
      trials++;

      const double cost =
          placement.has_value() ? PlacementCost(*placement) : kImpossibleCost;
      bool selected = false;
      if (placement.has_value()) {
        elites.push_back({cost, *placement});
        std::stable_sort(elites.begin(), elites.end(),
                         [](const auto& lhs, const auto& rhs) {
                           return lhs.first < rhs.first;
                         });
        selected = static_cast<int>(elites.size()) <= keep_count ||
                   cost <= elites[keep_count - 1].first + 1.0e-9;
        if (static_cast<int>(elites.size()) > keep_count) elites.pop_back();
      }

      if (trace) {
        WriteTrialTraceRow(trace, "yott_core_tail", seed, trial,
                           placement.has_value(), selected, cost, 0.0,
                           placement, SecondsSince(start), PlanHash(plan));
      }
    }
  }

  if (elites.empty()) return std::nullopt;

  // The first elite is exactly the placement YOTTCore would return for the
  // same seeds and trials. A repaired elite may replace it only when the
  // cumulative FIFO tail is no worse at every threshold. This also prevents
  // regressions in direct-edge count, maximal FIFO, and every FIFO percentile.
  PlacementState best = elites.front().second;
  const ProfileRepairMetrics core_guard =
      BuildProfileRepairContext(best).metrics;
  const bool balanced = candidate_rank_policy_ == "core_balanced_repair";
  for (int elite_index = 0; elite_index < static_cast<int>(elites.size());
       elite_index++) {
    PlacementState repaired =
        balanced
            ? PolishBalancedCoreRepairPlacement(elites[elite_index].second,
                                                core_guard, start)
            : PolishProfileRepairPlacement(elites[elite_index].second, start);
    const ProfileRepairContext candidate = BuildProfileRepairContext(repaired);
    const ProfileRepairContext current = BuildProfileRepairContext(best);
    const FifoProfileComparison comparison = CompareFifoProfiles(
        candidate.cost_fifo_histogram, current.cost_fifo_histogram);
    const bool equal_profile =
        comparison.nonworse && !comparison.strictly_better &&
        CompareFifoProfiles(current.cost_fifo_histogram,
                            candidate.cost_fifo_histogram)
            .nonworse;
    const bool better =
        balanced
            ? (candidate.metrics.non_direct_cost_edges <=
                   core_guard.non_direct_cost_edges &&
               candidate.metrics.total_cost_fifo <=
                   core_guard.total_cost_fifo &&
               candidate.metrics.max_cost_fifo <= core_guard.max_cost_fifo &&
               IsBetterBalancedRepairSummary(candidate.metrics,
                                             current.metrics))
            : (comparison.nonworse &&
               (comparison.strictly_better ||
                (equal_profile &&
                 PlacementCost(repaired) + 1.0e-9 < PlacementCost(best))));
    if (trace) {
      WriteTrialTraceRow(trace, "yott_core_tail_polish", -1, elite_index, true,
                         better, PlacementCost(repaired), 0.0, repaired,
                         SecondsSince(start));
    }
    if (better) best = std::move(repaired);
  }

  Log("yott_core_tail_repair trials=" + std::to_string(trials) +
      " elites=" + std::to_string(elites.size()));
  return best;
}

}  // namespace mapper::detail::placement2d
