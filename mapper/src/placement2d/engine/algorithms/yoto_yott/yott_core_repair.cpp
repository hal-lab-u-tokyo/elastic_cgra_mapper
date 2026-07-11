#include "../../array_engine_internal.hpp"

namespace mapper::detail::placement2d {

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

bool Placement2DArrayEngine::CoreRepairUsesCutDemand() const {
  return !IsRepairNoCutPolicy(candidate_rank_policy_);
}

bool Placement2DArrayEngine::CoreRepairUsesRepair() const {
  return !IsRepairNoRepairPolicy(candidate_rank_policy_);
}

bool Placement2DArrayEngine::CoreRepairUsesTopM() const {
  return !IsRepairNoTopMPolicy(candidate_rank_policy_);
}

int Placement2DArrayEngine::CoreRepairEdgePoolSize() const { return 8; }

bool Placement2DArrayEngine::CoreRepairUsesFifoScore() const {
  return !IsRepairNoFifoPolicy(candidate_rank_policy_);
}

PlacementState Placement2DArrayEngine::PolishCoreRepairPlacement(
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
Placement2DArrayEngine::ConstructCoreRepairPlacement(
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

std::optional<PlacementState> Placement2DArrayEngine::RunCoreRepairMultiStart(
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

}  // namespace mapper::detail::placement2d
