#include <mapper/yott_core_repair/engine/yott_core_repair_engine.hpp>

namespace mapper::yott_core_repair {
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

ProfileRepairContext YottCoreRepairEngine::BuildProfileRepairContext(
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

std::vector<int> YottCoreRepairEngine::ProfileRepairActiveNodes(
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

std::vector<int> YottCoreRepairEngine::ProfileRepairEmptyCells(
    const PlacementState& state, int node) const {
  std::vector<int> cells;
  for (int cell : compatible_cells_[node]) {
    if (state.cell_to_dfg[cell] < 0 && IsCPUMappingCompatible(node, cell)) {
      cells.push_back(cell);
    }
  }
  return cells;
}

std::vector<int> YottCoreRepairEngine::ProfileRepairPreferredEmptyCells(
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

std::vector<int> YottCoreRepairEngine::ProfileRepairSwapNodes(
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
YottCoreRepairEngine::EvaluateProfileRepairMove(
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

void YottCoreRepairEngine::ApplyProfileRepairMove(
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
YottCoreRepairEngine::FindBestProfileRepairMove(
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

PlacementState YottCoreRepairEngine::PolishProfileRepairPlacement(
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

PlacementState YottCoreRepairEngine::PolishBalancedCoreRepairPlacement(
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

std::optional<PlacementState> YottCoreRepairEngine::RunCoreTailRepairMultiStart(
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

// Pipeline:
// 1. Build the compact YOTTCore traversal plan.
// 2. Place nodes with local candidate pools and bottleneck-aware ranking.
// 3. Keep the best few constructed placements, then run bounded local swaps on
//    the currently worst edges.
namespace {

constexpr const char* kRepairNoFifoPolicy = "repair_no_fifo";
constexpr const char* kRepairNoCutPolicy = "repair_no_cut";
constexpr const char* kRepairNoRepairPolicy = "repair_no_repair";
constexpr const char* kRepairNoTopMPolicy = "repair_no_topm";

bool IsRepairNoFifoPolicy(const std::string& policy) {
  return policy == kRepairNoFifoPolicy;
}

bool IsRepairNoCutPolicy(const std::string& policy) {
  return policy == kRepairNoCutPolicy;
}

bool IsRepairNoRepairPolicy(const std::string& policy) {
  return policy == kRepairNoRepairPolicy;
}

bool IsRepairNoTopMPolicy(const std::string& policy) {
  return policy == kRepairNoTopMPolicy;
}

}  // namespace

bool YottCoreRepairEngine::CoreRepairUsesCutDemand() const {
  return !IsRepairNoCutPolicy(candidate_rank_policy_);
}

bool YottCoreRepairEngine::CoreRepairUsesRepair() const {
  return !IsRepairNoRepairPolicy(candidate_rank_policy_);
}

bool YottCoreRepairEngine::CoreRepairUsesTopM() const {
  return !IsRepairNoTopMPolicy(candidate_rank_policy_);
}

int YottCoreRepairEngine::CoreRepairEdgePoolSize() const { return 8; }

bool YottCoreRepairEngine::CoreRepairUsesFifoScore() const {
  return !IsRepairNoFifoPolicy(candidate_rank_policy_);
}

PlacementState YottCoreRepairEngine::PolishCoreRepairPlacement(
    PlacementState state, std::chrono::steady_clock::time_point start) {
  if (!CoreRepairUsesRepair()) return state;

  int budget = max_iterations_.value_or(
      std::max(32, static_cast<int>(edges_.size()) * 4));
  budget = std::min(budget, std::max(24, static_cast<int>(edges_.size()) * 10));
  double best_score = CoreRepairScore(state);

  for (int iter = 0; iter < budget && !HasTimedOut(start, 0.03); iter++) {
    const std::vector<int> repair_edges = CoreRepairEdgePool(state);
    if (repair_edges.empty()) break;

    std::optional<PlacementState> best_candidate;
    double best_candidate_score = best_score;
    std::vector<std::pair<double, PlacementState>> delta_gate_shortlist;
    auto push_delta_gate_candidate = [&](double local_delta,
                                         PlacementState candidate) {
      constexpr int kShortlistSize = 4;
      delta_gate_shortlist.push_back({local_delta, std::move(candidate)});
      std::sort(delta_gate_shortlist.begin(), delta_gate_shortlist.end(),
                [](const auto& lhs, const auto& rhs) {
                  if (std::abs(lhs.first - rhs.first) > 1.0e-12) {
                    return lhs.first < rhs.first;
                  }
                  return false;
                });
      if (static_cast<int>(delta_gate_shortlist.size()) > kShortlistSize) {
        delta_gate_shortlist.resize(kShortlistSize);
      }
    };

    for (int repair_edge_id : repair_edges) {
      const auto& repair_edge = edges_[repair_edge_id];
      const std::array<std::pair<int, int>, 2> endpoints = {
          std::make_pair(repair_edge.source, repair_edge.target),
          std::make_pair(repair_edge.target, repair_edge.source)};

      for (const auto& [moving_node, fixed_node] : endpoints) {
        const int moving_cell = state.dfg_to_cell[moving_node];
        const int fixed_cell = state.dfg_to_cell[fixed_node];
        if (moving_cell < 0 || fixed_cell < 0) continue;

        std::vector<int> targets;
        constexpr int fixed_radius = 4;
        constexpr int neighbor_radius = 3;
        for (int radius = 1; radius <= fixed_radius; radius++) {
          for (int cell : CoreRepairCells(fixed_cell, radius)) {
            targets.push_back(cell);
          }
        }
        for (int edge_id : incident_edge_ids_[moving_node]) {
          const auto& edge = edges_[edge_id];
          const int other =
              edge.source == moving_node ? edge.target : edge.source;
          const int other_cell = state.dfg_to_cell[other];
          if (other_cell < 0) continue;
          for (int radius = 1; radius <= neighbor_radius; radius++) {
            for (int cell : CoreRepairCells(other_cell, radius)) {
              targets.push_back(cell);
            }
          }
        }
        std::sort(targets.begin(), targets.end());
        targets.erase(std::unique(targets.begin(), targets.end()),
                      targets.end());

        for (int target_cell : targets) {
          if (!CanSwapCells(state, moving_cell, target_cell)) continue;
          const auto affected_edges =
              AffectedEdgesForCellPair(state, moving_cell, target_cell);
          const double candidate_delta = CoreRepairLocalSwapDelta(
              state, moving_cell, target_cell, affected_edges);
          if (candidate_delta >= 0.0) continue;
          PlacementState candidate = state;
          if (!ApplyCellSwap(candidate, moving_cell, target_cell)) continue;
          push_delta_gate_candidate(candidate_delta, std::move(candidate));
        }
      }
    }

    // Delta-gated repair keeps the PRISA-like cheap local filter, but asks the
    // global edge score to approve the few locally promising swaps.
    for (const auto& [unused_local_delta, candidate] : delta_gate_shortlist) {
      (void)unused_local_delta;
      const double candidate_score = CoreRepairScore(candidate);
      if (candidate_score + 1.0e-9 < best_candidate_score) {
        best_candidate_score = candidate_score;
        best_candidate = candidate;
      }
    }

    if (!best_candidate.has_value()) break;
    state = std::move(*best_candidate);
    best_score = best_candidate_score;
  }
  return state;
}

std::optional<PlacementState>
YottCoreRepairEngine::ConstructCoreRepairPlacement(
    const std::vector<Step>& plan) {
  CPUMappingPlacementContext context = MakeCPUMappingContext();

  for (const auto& step : plan) {
    const bool anchor_placed = context.state.dfg_to_cell[step.anchor] >= 0;
    const bool target_placed = context.state.dfg_to_cell[step.target] >= 0;
    if (!anchor_placed && !target_placed) {
      if (!PlaceCPUMappingInitialNode(step.anchor, context)) {
        return std::nullopt;
      }
      PlaceCoreRepairStep(step, context);
    } else if (anchor_placed && !target_placed) {
      PlaceCoreRepairStep(step, context);
    } else if (!anchor_placed && target_placed) {
      Step reversed = step;
      reversed.anchor = step.target;
      reversed.target = step.anchor;
      PlaceCoreRepairStep(reversed, context);
    }
  }

  for (int node = 0; node < dfg_.GetNodeNum(); node++) {
    if (!PlaceCPUMappingInitialNode(node, context)) {
      return std::nullopt;
    }
  }
  return context.state;
}

std::optional<PlacementState> YottCoreRepairEngine::RunCoreRepairMultiStart(
    std::chrono::steady_clock::time_point start) {
  if (candidate_rank_policy_ == "core_tail_repair" ||
      candidate_rank_policy_ == "core_balanced_repair") {
    return RunCoreTailRepairMultiStart(start);
  }

  std::optional<PlacementState> best;
  std::vector<std::pair<double, PlacementState>> top_candidates;
  const int keep_count = CoreRepairUsesTopM() ? ElitePlacementCount() : 1;
  double best_score = std::numeric_limits<double>::infinity();

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
      auto placement = ConstructCoreRepairPlacement(plan);
      trials++;
      if (!placement.has_value()) {
        if (trace) {
          WriteTrialTraceRow(trace, "yott_core_repair", seed, trial, false,
                             false, kImpossibleCost, kImpossibleCost,
                             std::nullopt, SecondsSince(start), PlanHash(plan));
        }
        continue;
      }

      if (CoreRepairUsesRepair() && !CoreRepairUsesTopM()) {
        placement = PolishCoreRepairPlacement(*placement, start);
      }

      const double score = CoreRepairScore(*placement);
      bool selected = false;
      if (CoreRepairUsesTopM()) {
        top_candidates.push_back({score, *placement});
        std::sort(top_candidates.begin(), top_candidates.end(),
                  [](const auto& lhs, const auto& rhs) {
                    return lhs.first < rhs.first;
                  });
        selected = static_cast<int>(top_candidates.size()) <= keep_count ||
                   score <= top_candidates[keep_count - 1].first + 1.0e-9;
        if (static_cast<int>(top_candidates.size()) > keep_count) {
          top_candidates.pop_back();
        }
      } else {
        selected = !best.has_value() || score + 1.0e-9 < best_score;
        if (selected) {
          best = placement;
          best_score = score;
        }
      }
      if (trace) {
        WriteTrialTraceRow(trace, "yott_core_repair", seed, trial, true,
                           selected, score, PlacementCost(*placement),
                           placement, SecondsSince(start), PlanHash(plan));
      }
    }
  }

  if (CoreRepairUsesTopM()) {
    for (int elite_index = 0;
         elite_index < static_cast<int>(top_candidates.size()); elite_index++) {
      auto& candidate = top_candidates[elite_index];
      PlacementState polished =
          PolishCoreRepairPlacement(candidate.second, start);
      const double score = CoreRepairScore(polished);
      const bool better = !best.has_value() || score + 1.0e-9 < best_score;
      if (trace) {
        WriteTrialTraceRow(trace, "yott_core_repair_polish", -1, elite_index,
                           true, better, score, PlacementCost(polished),
                           polished, SecondsSince(start));
      }
      if (better) {
        best = std::move(polished);
        best_score = score;
      }
    }
  }

  Log("yott_core_repair trials=" + std::to_string(trials) +
      " policy=" + candidate_rank_policy_);
  if (best.has_value() && candidate_rank_policy_ == "profile_repair") {
    best = PolishProfileRepairPlacement(std::move(*best), start);
  }
  return best;
}

namespace {

void PushUniqueCell(std::vector<std::pair<int, int>>& cells, int freedom,
                    int cell) {
  for (const auto& existing : cells) {
    if (existing.second == cell) return;
  }
  cells.push_back({freedom, cell});
}

}  // namespace

// Candidate generation and ranking for the constructive pass.

std::vector<std::pair<int, int>> YottCoreRepairEngine::CoreRepairCandidateCells(
    const Step& step, int dfg_node, int anchor_cell,
    const CPUMappingPlacementContext& context) {
  std::vector<std::pair<int, int>> candidates;

  if (anchor_cell >= 0) {
    for (const auto& cell :
         CPUMappingTipCells(anchor_cell, dfg_node, context)) {
      PushUniqueCell(candidates, cell.first, cell.second);
    }
  }

  for (const auto& annotation : step.annotations) {
    if (annotation.kind != StepAnnotationKind::kReconvergence) continue;
    if (annotation.distance > 2) continue;
    const int annotated_cell =
        context.state.dfg_to_cell[annotation.anchor_node];
    if (annotated_cell < 0) continue;
    auto cells = CPUMappingCellsWithinAnnotatedDistance(
        annotated_cell, annotation.distance, context);
    for (const auto& cell : cells) {
      PushUniqueCell(candidates, cell.first, cell.second);
    }
  }

  int neighbor_count = 0;
  int row_sum = 0;
  int col_sum = 0;
  for (int edge_id : incident_edge_ids_[dfg_node]) {
    const auto& edge = edges_[edge_id];
    const int other = edge.source == dfg_node ? edge.target : edge.source;
    const int other_cell = context.state.dfg_to_cell[other];
    if (other_cell < 0) continue;
    neighbor_count++;
    row_sum += Row(other_cell);
    col_sum += Col(other_cell);
  }

  if (neighbor_count > 0) {
    const int center_row = std::clamp(
        static_cast<int>(std::lround(static_cast<double>(row_sum) /
                                     static_cast<double>(neighbor_count))),
        0, rows_ - 1);
    const int center_col = std::clamp(
        static_cast<int>(std::lround(static_cast<double>(col_sum) /
                                     static_cast<double>(neighbor_count))),
        0, cols_ - 1);
    const int center_cell = Cell(center_row, center_col);

    std::vector<std::pair<int, int>> nearest;
    for (int cell : CPUMappingCompatibleCells(dfg_node, context.state)) {
      nearest.push_back({DistanceCost(center_cell, cell), cell});
    }
    std::sort(nearest.begin(), nearest.end());
    const int limit = std::min(12, static_cast<int>(nearest.size()));
    for (int i = 0; i < limit; i++) {
      const int cell = nearest[i].second;
      PushUniqueCell(candidates, context.freedom[cell], cell);
    }
  }

  if (candidates.empty()) {
    for (const auto& cell :
         CPUMappingAllCompatibleCellPairs(dfg_node, context)) {
      PushUniqueCell(candidates, cell.first, cell.second);
    }
  }

  return candidates;
}

double YottCoreRepairEngine::CoreRepairCandidateScore(
    const Step& step, int dfg_node, int candidate_cell,
    const CPUMappingPlacementContext& context) const {
  double score = 0.0;
  int placed_edges = 0;
  int direct_edges = 0;

  for (int edge_id : incident_edge_ids_[dfg_node]) {
    const auto& edge = edges_[edge_id];
    const int other = edge.source == dfg_node ? edge.target : edge.source;
    const int other_cell = context.state.dfg_to_cell[other];
    if (other_cell < 0) continue;
    placed_edges++;
    score += CoreRepairEdgeCost(edge_id, candidate_cell, other_cell);
    if (DistanceCost(candidate_cell, other_cell) <= 1) direct_edges++;

    if (CoreRepairUsesCutDemand()) {
      score += 0.12 * static_cast<double>(
                          std::abs(Row(candidate_cell) - Row(other_cell)) +
                          std::abs(Col(candidate_cell) - Col(other_cell)));
    }
    if (CoreRepairUsesFifoScore()) {
      const int tail =
          std::max(0, DistanceCost(candidate_cell, other_cell) - 1);
      score += 2.4 * static_cast<double>(tail) +
               1.2 * static_cast<double>(tail * tail);
    }
  }

  for (const auto& annotation : step.annotations) {
    if (annotation.kind != StepAnnotationKind::kReconvergence) continue;
    const int anchor_cell = context.state.dfg_to_cell[annotation.anchor_node];
    if (anchor_cell < 0) continue;
    const int distance = DistanceCost(candidate_cell, anchor_cell);
    score += 0.75 * std::abs(distance - annotation.distance);
  }

  const double freedom_bonus =
      candidate_cell >= 0 &&
              candidate_cell < static_cast<int>(context.freedom.size())
          ? static_cast<double>(context.freedom[candidate_cell])
          : 0.0;
  score -= 0.2 * static_cast<double>(direct_edges);
  score -= 0.035 * freedom_bonus;
  score -= 0.06 * static_cast<double>(placed_edges);
  score += static_cast<double>(candidate_cell % 997) * 1.0e-6;
  return score;
}

int YottCoreRepairEngine::ChooseCoreRepairCell(
    const std::vector<std::pair<int, int>>& cells, const Step& step,
    int dfg_node, const CPUMappingPlacementContext& context) {
  std::vector<int> valid_cells;
  for (const auto& [unused_freedom, cell] : cells) {
    (void)unused_freedom;
    if (CanPlaceCPUMapping(dfg_node, cell, context.state)) {
      valid_cells.push_back(cell);
    }
  }
  if (valid_cells.empty()) return -1;
  RecordPlacementSwapAttempts(static_cast<long long>(valid_cells.size()));

  int best_cell = -1;
  double best_score = kImpossibleCost;
  for (int cell : valid_cells) {
    const double score =
        CoreRepairCandidateScore(step, dfg_node, cell, context);
    if (score < best_score) {
      best_score = score;
      best_cell = cell;
    }
  }
  return best_cell;
}

bool YottCoreRepairEngine::PlaceCoreRepairStep(
    const Step& step, CPUMappingPlacementContext& context) {
  const int dfg_node = step.target;
  if (context.state.dfg_to_cell[dfg_node] >= 0) return true;

  const int anchor_cell = context.state.dfg_to_cell[step.anchor];
  if (anchor_cell < 0) {
    return PlaceCPUMappingInitialNode(dfg_node, context);
  }

  auto candidates =
      CoreRepairCandidateCells(step, dfg_node, anchor_cell, context);
  const int cell = ChooseCoreRepairCell(candidates, step, dfg_node, context);
  if (cell >= 0) {
    PlaceCPUMappingNode(dfg_node, cell, context);
    return true;
  }
  return TryCPUMappingAdjacency(dfg_node, anchor_cell, context, true);
}

// Edge, FIFO, cut-demand, and local-swap objectives used by construction and
// bounded repair.

double YottCoreRepairEngine::CoreRepairEdgeWeight(int edge_id) const {
  if (edge_id < 0 || edge_id >= static_cast<int>(edges_.size())) {
    return 1.0;
  }
  const auto& edge = edges_[edge_id];
  const int max_critical = std::max(1, critical_path_max_);
  const double source_critical =
      static_cast<double>(critical_path_score_[edge.source]) /
      static_cast<double>(max_critical);
  const double target_critical =
      static_cast<double>(critical_path_score_[edge.target]) /
      static_cast<double>(max_critical);
  const double critical = std::max(source_critical, target_critical);
  const int fanout =
      std::max(0, static_cast<int>(successors_[edge.source].size()) - 1);
  return 1.0 + 1.6 * critical + 0.18 * static_cast<double>(fanout);
}

double YottCoreRepairEngine::CoreRepairEdgeCost(int source_cell,
                                                int target_cell) const {
  return CoreRepairEdgeCost(-1, source_cell, target_cell);
}

double YottCoreRepairEngine::CoreRepairEdgeCost(int edge_id, int source_cell,
                                                int target_cell) const {
  const int distance = DistanceCost(source_cell, target_cell);
  double score = static_cast<double>(distance);
  const int tail = std::max(0, distance - 1);
  score += 2.6 * static_cast<double>(tail * tail);
  if (distance > 1) score += 1.6;
  return CoreRepairEdgeWeight(edge_id) * score;
}

double YottCoreRepairEngine::CoreRepairCutScore(
    const PlacementState& state) const {
  if (!CoreRepairUsesCutDemand()) return 0.0;

  const auto [row_cut, col_cut] = CoreRepairCutDemand(state);
  if (row_cut.empty() && rows_ > 1) return kImpossibleCost;
  if (col_cut.empty() && cols_ > 1) return kImpossibleCost;

  double score = 0.0;
  constexpr double kSoftCapacity = 2.0;
  for (double demand : row_cut) {
    const double overflow = std::max(0.0, demand - kSoftCapacity);
    score += overflow * overflow;
  }
  for (double demand : col_cut) {
    const double overflow = std::max(0.0, demand - kSoftCapacity);
    score += overflow * overflow;
  }
  return score;
}

double YottCoreRepairEngine::CoreRepairFifoScore(
    const PlacementState& state) const {
  if (!CoreRepairUsesFifoScore()) return 0.0;

  double tail_sum = 0.0;
  double tail_square_sum = 0.0;
  int max_tail = 0;
  for (const auto& edge : edges_) {
    const int source_cell = state.dfg_to_cell[edge.source];
    const int target_cell = state.dfg_to_cell[edge.target];
    if (source_cell < 0 || target_cell < 0) return kImpossibleCost;
    const int tail = std::max(0, DistanceCost(source_cell, target_cell) - 1);
    tail_sum += static_cast<double>(tail);
    tail_square_sum += static_cast<double>(tail * tail);
    max_tail = std::max(max_tail, tail);
  }

  // Same tail definition as the paper-style FIFO metric in the reports:
  // direct edges cost zero; each extra segment adds one FIFO slot.
  return 1.6 * tail_sum + 1.2 * tail_square_sum +
         12.0 * static_cast<double>(max_tail);
}

std::pair<std::vector<double>, std::vector<double>>
YottCoreRepairEngine::CoreRepairCutDemand(const PlacementState& state) const {
  std::vector<double> row_cut(std::max(0, rows_ - 1), 0.0);
  std::vector<double> col_cut(std::max(0, cols_ - 1), 0.0);
  for (const auto& edge : edges_) {
    const int source_cell = state.dfg_to_cell[edge.source];
    const int target_cell = state.dfg_to_cell[edge.target];
    if (source_cell < 0 || target_cell < 0) return {{}, {}};

    const int r0 = Row(source_cell);
    const int r1 = Row(target_cell);
    const int c0 = Col(source_cell);
    const int c1 = Col(target_cell);
    for (int r = std::min(r0, r1); r < std::max(r0, r1); r++) {
      if (r >= 0 && r < static_cast<int>(row_cut.size())) row_cut[r] += 1.0;
    }
    for (int c = std::min(c0, c1); c < std::max(c0, c1); c++) {
      if (c >= 0 && c < static_cast<int>(col_cut.size())) col_cut[c] += 1.0;
    }
  }
  return {row_cut, col_cut};
}

double YottCoreRepairEngine::CoreRepairEdgePriority(
    const PlacementState& state, int edge_id,
    const std::vector<double>& row_cut,
    const std::vector<double>& col_cut) const {
  if (edge_id < 0 || edge_id >= static_cast<int>(edges_.size())) {
    return -1.0;
  }
  const auto& edge = edges_[edge_id];
  const int source_cell = state.dfg_to_cell[edge.source];
  const int target_cell = state.dfg_to_cell[edge.target];
  if (source_cell < 0 || target_cell < 0) return -1.0;

  const int distance = DistanceCost(source_cell, target_cell);
  double cut_demand = 0.0;
  const int r0 = Row(source_cell);
  const int r1 = Row(target_cell);
  const int c0 = Col(source_cell);
  const int c1 = Col(target_cell);
  for (int r = std::min(r0, r1); r < std::max(r0, r1); r++) {
    if (r >= 0 && r < static_cast<int>(row_cut.size())) {
      cut_demand += row_cut[r];
    }
  }
  for (int c = std::min(c0, c1); c < std::max(c0, c1); c++) {
    if (c >= 0 && c < static_cast<int>(col_cut.size())) {
      cut_demand += col_cut[c];
    }
  }

  const int max_critical = std::max(1, critical_path_max_);
  const double critical =
      static_cast<double>(std::max(critical_path_score_[edge.source],
                                   critical_path_score_[edge.target])) /
      static_cast<double>(max_critical);
  const int fanout =
      std::max(0, static_cast<int>(successors_[edge.source].size()) - 1);

  double priority = CoreRepairEdgeCost(edge_id, source_cell, target_cell);
  const int tail = std::max(0, distance - 1);
  priority += 80.0 * static_cast<double>(tail * tail);
  priority += 24.0 * static_cast<double>(tail);
  priority += 0.22 * cut_demand * static_cast<double>(std::max(1, distance));
  priority += 1.10 * critical;
  priority += 0.18 * static_cast<double>(fanout);
  return priority;
}

std::vector<int> YottCoreRepairEngine::CoreRepairEdgePool(
    const PlacementState& state) const {
  const auto [row_cut, col_cut] = CoreRepairCutDemand(state);
  std::vector<std::pair<double, int>> scored_edges;
  for (int edge_id = 0; edge_id < static_cast<int>(edges_.size()); edge_id++) {
    const double score =
        CoreRepairEdgePriority(state, edge_id, row_cut, col_cut);
    if (score >= 0.0) {
      scored_edges.push_back({score, edge_id});
    }
  }
  std::sort(scored_edges.begin(), scored_edges.end(),
            [](const auto& lhs, const auto& rhs) {
              if (std::abs(lhs.first - rhs.first) > 1.0e-12) {
                return lhs.first > rhs.first;
              }
              return lhs.second < rhs.second;
            });

  const int pool_size = CoreRepairEdgePoolSize();
  std::vector<int> edge_pool;
  for (int i = 0;
       i < std::min(pool_size, static_cast<int>(scored_edges.size())); i++) {
    edge_pool.push_back(scored_edges[i].second);
  }
  return edge_pool;
}

std::vector<int> YottCoreRepairEngine::CoreRepairCells(int center_cell,
                                                       int radius) const {
  std::vector<int> cells;
  if (center_cell < 0 || center_cell >= rows_ * cols_ || radius < 0) {
    return cells;
  }

  const int center_row = Row(center_cell);
  const int center_col = Col(center_cell);
  for (int dr = -radius; dr <= radius; dr++) {
    const int dc_abs = radius - std::abs(dr);
    const std::array<int, 2> dcs = {-dc_abs, dc_abs};
    for (int dc : dcs) {
      const int row = center_row + dr;
      const int col = center_col + dc;
      if (row < 0 || row >= rows_ || col < 0 || col >= cols_) continue;
      cells.push_back(Cell(row, col));
      if (dc_abs == 0) break;
    }
  }
  return cells;
}

double YottCoreRepairEngine::CoreRepairLocalSwapDelta(
    const PlacementState& state, int first_cell, int second_cell,
    const std::vector<int>& affected_edges) const {
  if (affected_edges.empty()) return kImpossibleCost;

  double edge_delta = 0.0;
  double fifo_delta = 0.0;
  int direct_delta = 0;
  int before_max_tail = 0;
  int after_max_tail = 0;
  for (int edge_id : affected_edges) {
    const auto& edge = edges_[edge_id];
    const int source_cell = state.dfg_to_cell[edge.source];
    const int target_cell = state.dfg_to_cell[edge.target];
    const int next_source = CellAfterSwap(source_cell, first_cell, second_cell);
    const int next_target = CellAfterSwap(target_cell, first_cell, second_cell);

    const int before = DistanceCost(source_cell, target_cell);
    const int after = DistanceCost(next_source, next_target);
    edge_delta += CoreRepairEdgeCost(edge_id, next_source, next_target) -
                  CoreRepairEdgeCost(edge_id, source_cell, target_cell);

    const int before_tail = std::max(0, before - 1);
    const int after_tail = std::max(0, after - 1);
    before_max_tail = std::max(before_max_tail, before_tail);
    after_max_tail = std::max(after_max_tail, after_tail);
    if (CoreRepairUsesFifoScore()) {
      fifo_delta += 1.6 * static_cast<double>(after_tail - before_tail);
      fifo_delta += 1.2 * static_cast<double>(after_tail * after_tail -
                                              before_tail * before_tail);
    }
    if (before <= 1 && after > 1) direct_delta--;
    if (before > 1 && after <= 1) direct_delta++;
  }

  // PRISA-style local acceptance: only edges incident to the swapped cells
  // decide the move. This keeps repair cheap and avoids a global rescore per
  // candidate, while preserving direct edges unless the local tail gain is
  // large.
  double delta = edge_delta + fifo_delta;
  delta += 18.0 * static_cast<double>(after_max_tail - before_max_tail);
  delta -= 4.0 * static_cast<double>(direct_delta);
  if (direct_delta < 0) delta += 24.0 * static_cast<double>(-direct_delta);
  return delta;
}

double YottCoreRepairEngine::CoreRepairScore(
    const PlacementState& state) const {
  double score = 0.0;
  for (int edge_id = 0; edge_id < static_cast<int>(edges_.size()); edge_id++) {
    const auto& edge = edges_[edge_id];
    const int source_cell = state.dfg_to_cell[edge.source];
    const int target_cell = state.dfg_to_cell[edge.target];
    if (source_cell < 0 || target_cell < 0) return kImpossibleCost;
    score += CoreRepairEdgeCost(edge_id, source_cell, target_cell);
  }

  if (CoreRepairUsesCutDemand()) {
    const CostAwarePRISAMetrics metrics = ComputeCostAwarePRISAMetrics(state);
    if (metrics.mesh_hop_sum >= kImpossibleCost) return kImpossibleCost;
    score += 0.18 * CoreRepairCutScore(state);
    score += 0.28 * static_cast<double>(metrics.mapped_lp_mesh_hop);
    score += 1.25 * static_cast<double>(metrics.max_mesh_hop);
  }
  score += CoreRepairFifoScore(state);
  return score;
}

}  // namespace mapper::yott_core_repair
