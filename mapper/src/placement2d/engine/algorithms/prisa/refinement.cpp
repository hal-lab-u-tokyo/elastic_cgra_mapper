#include "../../array_engine_internal.hpp"

namespace mapper::detail::placement2d {

// Array PRISA pipeline.
//
//   1. ConstructPRISAInitialPlacement()
//      Build the initial placement. SIS uses LowBandwidthNodeOrder(); no-SIS
//      starts from RandomPlacement().
//   2. InitializePRISAStats()
//      Count weak-region and potential-region edges in the PRISA distance
//      matrix.
//   3. ProposePRISAMove()
//      Pick a swap that repairs weak-region edges. Derived cost-aware variants
//      can instead score sampled swaps by placement quality.
//   4. RunPRISA()
//      Apply proposals under a small annealing schedule and keep the best
//      placement seen in that trial.
//   5. RunPRISAMultiSeed()
//      Repeat seeds and keep the best PlacementQuality().
//
// Ablation handles:
//   - SIS order: LowBandwidthNodeOrder()
//   - PR/WR matrix use: PRISAIsWeak(), PRISAEdgeRegion()
//   - move choice: BestPRISACandidateFor(), ProposePRISAMove()
//   - derived cost-aware scoring: ProposeCostAwarePRISASwap()
//   - final cleanup: PolishPRISAQuality(), PolishCostAwarePRISADirectEdges()

int Placement2DArrayEngine::PRISAMaxIterations() const {
  if (max_iterations_.has_value()) return std::max(1, max_iterations_.value());
  return std::max(1, dfg_.GetNodeNum() * 10);
}

int Placement2DArrayEngine::PRISAPolishPasses() const {
  if (!UsesCostAwarePRISA()) return 0;
  return std::max(4, std::min(16, PRISAMaxIterations() / 80));
}

// Derived cost-aware cleanup passes. Base PRISA and no-SIS PRISA skip these.

PlacementState Placement2DArrayEngine::PolishPRISAQuality(
    PlacementState state, std::chrono::steady_clock::time_point start) {
  PlacementQuality current = ComputePlacementQuality(state);
  const int resource_count = static_cast<int>(prisa_cell_order_.size());
  if (resource_count < 2) return state;

  for (int pass = 0; pass < PRISAPolishPasses() && !HasTimedOut(start, 0.05);
       pass++) {
    bool improved = false;
    PlacementState best_state = state;
    PlacementQuality best_quality = current;

    for (int first_index = 0; first_index < resource_count; first_index++) {
      for (int second_index = first_index + 1; second_index < resource_count;
           second_index++) {
        const int first_cell = prisa_cell_order_[first_index];
        const int second_cell = prisa_cell_order_[second_index];
        if (!CanSwapCells(state, first_cell, second_cell)) continue;
        const auto affected_edges =
            AffectedEdgesForCellPair(state, first_cell, second_cell);
        const int direct_delta = DirectEdgeDeltaForCellSwap(
            state, first_cell, second_cell, affected_edges);
        const double cost_delta = PlacementCostDeltaForCellSwap(
            state, first_cell, second_cell, affected_edges);
        if (direct_delta < 0) continue;
        if (cost_delta > 1.0e-9) continue;
        if (direct_delta == 0 && cost_delta >= -1.0e-9) continue;

        PlacementState next = state;
        if (!ApplyCellSwap(next, first_cell, second_cell)) continue;
        const PlacementQuality next_quality = ComputePlacementQuality(next);
        if (IsBetterQuality(next_quality, best_quality)) {
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

int Placement2DArrayEngine::DirectEdgeGainForCellSwap(
    const PlacementState& state, int first_cell, int second_cell,
    const std::vector<int>& affected_edges) const {
  int gain = 0;
  for (int edge_id : affected_edges) {
    const auto& edge = edges_[edge_id];
    const int source_cell = state.dfg_to_cell[edge.source];
    const int target_cell = state.dfg_to_cell[edge.target];
    const int before = DistanceCost(source_cell, target_cell);
    const int after =
        DistanceCost(CellAfterSwap(source_cell, first_cell, second_cell),
                     CellAfterSwap(target_cell, first_cell, second_cell));
    if (before <= 1 && after > 1) gain--;
    if (before > 1 && after <= 1) gain++;
  }
  return gain;
}

PlacementState Placement2DArrayEngine::PolishCostAwarePRISADirectEdges(
    PlacementState state, std::chrono::steady_clock::time_point start) {
  const int resource_count = static_cast<int>(prisa_cell_order_.size());
  if (resource_count < 2) return state;

  struct Candidate {
    int direct_gain = 0;
    double local_score = std::numeric_limits<double>::infinity();
    int first_cell = -1;
    int second_cell = -1;
  };

  CostAwarePRISAMetrics current = ComputeCostAwarePRISAMetrics(state);
  for (int pass = 0; pass < PRISAPolishPasses() && !HasTimedOut(start, 0.05);
       pass++) {
    std::vector<Candidate> candidates;
    candidates.reserve(resource_count * resource_count / 2);
    for (int first_index = 0; first_index < resource_count; first_index++) {
      for (int second_index = first_index + 1; second_index < resource_count;
           second_index++) {
        const int first_cell = prisa_cell_order_[first_index];
        const int second_cell = prisa_cell_order_[second_index];
        if (!CanSwapCells(state, first_cell, second_cell)) continue;
        const auto affected_edges =
            AffectedEdgesForCellPair(state, first_cell, second_cell);
        const int direct_gain = DirectEdgeGainForCellSwap(
            state, first_cell, second_cell, affected_edges);
        if (direct_gain <= 0) continue;
        candidates.push_back(
            {direct_gain,
             CostAwarePRISALocalSwapScore(state, first_cell, second_cell,
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
    CostAwarePRISAMetrics best_metrics = current;
    for (const auto& candidate : candidates) {
      PlacementState next = state;
      if (!ApplyCellSwap(next, candidate.first_cell, candidate.second_cell)) {
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

bool Placement2DArrayEngine::IsCostAwareParetoImprovement(
    const CostAwarePRISAMetrics& candidate,
    const CostAwarePRISAMetrics& current) const {
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

bool Placement2DArrayEngine::IsBetterCostAwareParetoCandidate(
    const CostAwarePRISAMetrics& candidate,
    const CostAwarePRISAMetrics& best) const {
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

PlacementState Placement2DArrayEngine::PolishCostAwarePRISAPareto(
    PlacementState state, std::chrono::steady_clock::time_point start) {
  const int resource_count = static_cast<int>(prisa_cell_order_.size());
  if (resource_count < 2) return state;

  CostAwarePRISAMetrics current = ComputeCostAwarePRISAMetrics(state);
  const int passes = std::min(4, PRISAPolishPasses());
  for (int pass = 0; pass < passes && !HasTimedOut(start, 0.05); pass++) {
    bool improved = false;
    PlacementState best_state = state;
    CostAwarePRISAMetrics best_metrics = current;
    const int current_weak_edges = PRISAWeakEdgeCount(state);

    for (int first_index = 0; first_index < resource_count; first_index++) {
      for (int second_index = first_index + 1;
           second_index < resource_count && !HasTimedOut(start, 0.05);
           second_index++) {
        const int first_cell = prisa_cell_order_[first_index];
        const int second_cell = prisa_cell_order_[second_index];
        if (!CanSwapCells(state, first_cell, second_cell)) continue;

        PlacementState next = state;
        if (!ApplyCellSwap(next, first_cell, second_cell)) continue;
        if (PRISAWeakEdgeCount(next) > current_weak_edges) continue;
        const CostAwarePRISAMetrics next_metrics =
            ComputeCostAwarePRISAMetrics(next);
        if (!IsCostAwareParetoImprovement(next_metrics, current)) continue;
        if (!improved ||
            IsBetterCostAwareParetoCandidate(next_metrics, best_metrics)) {
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

std::optional<PlacementState> Placement2DArrayEngine::RunPRISA(
    std::chrono::steady_clock::time_point start) {
  auto current = ConstructPRISAInitialPlacement();
  if (!current.has_value()) return std::nullopt;
  PRISAStats stats = InitializePRISAStats(*current);

  PlacementState best = *current;
  double current_cost = PRISAAcceptanceScore(*current);
  double best_cost = current_cost;
  PlacementQuality best_quality = ComputePlacementQuality(best);
  const double initial_cost = PlacementCost(*current);
  const double initial_acceptance_cost = current_cost;
  const int initial_weak_edges = stats.weak_edges;
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
    auto proposal = ProposePRISAMove(*current, stats);
    if (!proposal.has_value()) continue;
    generated_moves++;
    const double next_cost = current_cost + proposal->cost_delta;
    const double delta = next_cost - current_cost;
    const bool accept =
        delta <= 0.0 || unit(rng_) < std::exp(-delta / temperature);
    if (accept) {
      if (!ApplyCellSwapWithStats(*current, stats, proposal->first_cell,
                                  proposal->second_cell)) {
        continue;
      }
      current_cost = next_cost;
      accepted_moves++;
      const bool better_cost = current_cost + 1.0e-9 < best_cost;
      bool better_same_cost = false;
      if (!UsesCostAwarePRISA() &&
          std::abs(current_cost - best_cost) <= 1.0e-9) {
        const PlacementQuality current_quality =
            ComputePlacementQuality(*current);
        better_same_cost = IsBetterQuality(current_quality, best_quality);
        if (better_same_cost) best_quality = current_quality;
      }
      if (better_cost || better_same_cost) {
        best = *current;
        best_cost = current_cost;
        if (better_cost) best_quality = ComputePlacementQuality(best);
        improving_moves++;
      }
    }
    temperature = std::max(final_temperature, temperature * cooling);
  }

  if (UsesCostAwarePRISA()) {
    const double before_polish_cost = best_cost;
    PlacementState polished = PolishPRISAQuality(best, start);
    polished = PolishCostAwarePRISADirectEdges(polished, start);
    polished = PolishCostAwarePRISAPareto(polished, start);
    const double polished_cost = PRISAAcceptanceScore(polished);
    const CostAwarePRISAMetrics polished_metrics =
        ComputeCostAwarePRISAMetrics(polished);
    const CostAwarePRISAMetrics best_metrics =
        ComputeCostAwarePRISAMetrics(best);
    if (polished_cost + 1.0e-9 < before_polish_cost ||
        (polished_metrics.direct_edge_count > best_metrics.direct_edge_count &&
         polished_metrics.max_mesh_hop <= best_metrics.max_mesh_hop &&
         polished_metrics.mapped_lp_mesh_hop <=
             best_metrics.mapped_lp_mesh_hop)) {
      best = std::move(polished);
      best_cost = PRISAAcceptanceScore(best);
      improving_moves++;
    }
  }

  Log("array_prisa_result sis=" + std::to_string(UsesPRISASIS() ? 1 : 0) +
      " iterations=" + std::to_string(completed_iterations) +
      " generated_moves=" + std::to_string(generated_moves) +
      " accepted_moves=" + std::to_string(accepted_moves) +
      " improving_moves=" + std::to_string(improving_moves) +
      " initial_cost=" + std::to_string(initial_cost) +
      " initial_acceptance_cost=" + std::to_string(initial_acceptance_cost) +
      " best_cost=" + std::to_string(best_cost) +
      " best_placement_cost=" + std::to_string(PlacementCost(best)) +
      " initial_weak_edges=" + std::to_string(initial_weak_edges) +
      " best_weak_edges=" + std::to_string(stats.weak_edges));
  return best;
}

std::optional<PlacementState> Placement2DArrayEngine::RunPRISAMultiSeed(
    std::chrono::steady_clock::time_point start) {
  std::optional<PlacementState> best;
  double best_cost = std::numeric_limits<double>::infinity();
  PlacementQuality best_quality;
  int seeds = 0;
  for (int seed = 0; seed < SeedCount() && !HasTimedOut(start, 0.05); seed++) {
    ResetSeed(seed);
    auto placement = RunPRISA(start);
    seeds++;
    if (!placement.has_value()) continue;
    const double cost = PRISAAcceptanceScore(*placement);
    const PlacementQuality quality = ComputePlacementQuality(*placement);
    const bool better_cost = cost + 1.0e-9 < best_cost;
    const bool better_same_cost = !UsesCostAwarePRISA() &&
                                  std::abs(cost - best_cost) <= 1.0e-9 &&
                                  IsBetterQuality(quality, best_quality);
    if (better_cost || better_same_cost) {
      best = placement;
      best_cost = cost;
      best_quality = quality;
    }
  }
  Log("array_prisa_search seeds=" + std::to_string(seeds) +
      " sis=" + std::to_string(UsesPRISASIS() ? 1 : 0));
  return best;
}

}  // namespace mapper::detail::placement2d
