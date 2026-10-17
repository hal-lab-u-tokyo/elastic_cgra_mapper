#include <mapper/sa/engine/context/sa_context_engine.hpp>

namespace mapper::sa_context {

std::optional<PlacementState> ModuloSAEngine::RunSAMultiSeed(
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
    auto placement = RunSA(start);
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
  Log("sa_search seeds=" + std::to_string(attempted_seeds) +
      " routeable=" + (best_routeable.has_value() ? "1" : "0"));
  if (options_.placement_only) return best_unrouted;
  if (best_routeable.has_value()) return best_routeable;
  return best_unrouted;
}

std::optional<PlacementState> ModuloSAEngine::RunSA(
    std::chrono::steady_clock::time_point start) {
  {}

  std::optional<PlacementState> current = ConstructGreedyPlacement();
  if (!current.has_value()) current = RandomLegalPlacement();
  if (!current.has_value()) return std::nullopt;

  PlacementState best = *current;
  double current_cost = PlacementCost(*current);
  double best_cost = current_cost;
  std::vector<PlacementState> elite;
  elite.push_back(best);

  const int node_num = dfg_.GetNodeNum();
  double temperature = std::max(1.0, current_cost / std::max(1, node_num));
  std::uniform_real_distribution<double> unit(0.0, 1.0);
  int iteration = 0;
  const int max_iterations = MaxIterations();

  while (iteration < max_iterations && !HasTimedOut(start, 0.05)) {
    iteration++;
    PlacementState next = *current;
    if (!ApplyRandomMove(next)) continue;
    const double next_cost = PlacementCost(next);
    const double delta = next_cost - current_cost;
    const bool accept =
        delta <= 0.0 || unit(rng_) < std::exp(-delta / temperature);
    if (accept) {
      *current = next;
      current_cost = next_cost;
    }
    if (next_cost < best_cost) {
      best = next;
      best_cost = next_cost;
      elite.push_back(best);
      if (elite.size() > 16) elite.erase(elite.begin());
    }
    temperature *= 0.995;
    if (temperature < 1.0e-4) temperature = std::max(1.0, best_cost / 10.0);
  }

  std::sort(elite.begin(), elite.end(),
            [&](const PlacementState& a, const PlacementState& b) {
              return PlacementCost(a) < PlacementCost(b);
            });
  if (options_.placement_only) return best;
  for (const auto& candidate : elite) {
    if (TryRoutePlacement(candidate).has_value()) return candidate;
    if (HasTimedOut(start, 0.01)) break;
  }
  return best;
}

}  // namespace mapper::sa_context
