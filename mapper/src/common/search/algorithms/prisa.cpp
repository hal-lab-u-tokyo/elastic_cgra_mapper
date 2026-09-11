#include "../engine_internal.hpp"

namespace mapper::detail::placement_search {

std::optional<PlacementState> PlacementSearchEngine::RunPRISAMultiSeed(
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

std::optional<PlacementState> PlacementSearchEngine::RunPRISA(
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

}  // namespace mapper::detail::placement_search
