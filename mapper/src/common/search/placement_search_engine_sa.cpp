#include "placement_search_engine_internal.hpp"

namespace mapper::detail::placement_search {

std::optional<PlacementState> PlacementSearchEngine::RunSAMultiSeed(
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

std::optional<PlacementState> PlacementSearchEngine::RunSA(
    std::chrono::steady_clock::time_point start) {
  if (search_kind_ == PlacementSearchKind::kPlacement2DSA) {
    return RunPlacement2DSA(start);
  }

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
    const bool accept = delta <= 0.0 || unit(rng_) < std::exp(-delta / temperature);
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

  std::sort(elite.begin(), elite.end(), [&](const PlacementState& a,
                                            const PlacementState& b) {
    return PlacementCost(a) < PlacementCost(b);
  });
  if (options_.placement_only) return best;
  for (const auto& candidate : elite) {
    if (TryRoutePlacement(candidate).has_value()) return candidate;
    if (HasTimedOut(start, 0.01)) break;
  }
  return best;
}

std::optional<PlacementState> PlacementSearchEngine::RunPlacement2DSA(
    std::chrono::steady_clock::time_point start) {
  std::optional<PlacementState> current = RandomLegalPlacement();
  if (!current.has_value()) return std::nullopt;

  PlacementState best = *current;
  double current_cost = PlacementCost(*current);
  double best_cost = current_cost;
  std::vector<PlacementState> elite;
  elite.push_back(best);

  std::uniform_real_distribution<double> unit(0.0, 1.0);
  double temperature = 100.0;
  int iteration = 0;
  const int max_iterations = MaxIterations();
  std::vector<int> node_order(dfg_.GetNodeNum());
  std::iota(node_order.begin(), node_order.end(), 0);

  auto try_candidate = [&](const PlacementState& next) {
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
  };

  while (iteration < max_iterations && !HasTimedOut(start, 0.05) &&
         temperature > 1.0e-5) {
    std::shuffle(node_order.begin(), node_order.end(), rng_);
    for (int i = 0; i < static_cast<int>(node_order.size()); i++) {
      for (int j = i + 1; j < static_cast<int>(node_order.size()); j++) {
        PlacementState next = *current;
        if (!ApplySwapMove(next, node_order[i], node_order[j])) continue;
        try_candidate(next);
        iteration++;
        if (iteration >= max_iterations || HasTimedOut(start, 0.05)) break;
      }
      if (iteration >= max_iterations || HasTimedOut(start, 0.05)) break;
    }
    const int empty_cell_trials = std::max(1, dfg_.GetNodeNum());
    for (int i = 0; i < empty_cell_trials && iteration < max_iterations &&
                    !HasTimedOut(start, 0.05);
         i++) {
      PlacementState next = *current;
      if (!ApplyMoveToFreeResource(next)) continue;
      try_candidate(next);
      iteration++;
    }
    temperature *= 0.999;
  }

  std::sort(elite.begin(), elite.end(), [&](const PlacementState& a,
                                            const PlacementState& b) {
    return PlacementCost(a) < PlacementCost(b);
  });
  if (options_.placement_only) return best;
  for (const auto& candidate : elite) {
    if (TryRoutePlacement(candidate).has_value()) return candidate;
    if (HasTimedOut(start, 0.01)) break;
  }
  return best;
}

bool PlacementSearchEngine::ApplyRandomMove(PlacementState& state) {
  const int node_num = dfg_.GetNodeNum();
  if (node_num <= 0) return false;
  std::uniform_int_distribution<int> node_dist(0, node_num - 1);
  std::uniform_real_distribution<double> unit(0.0, 1.0);

  if (unit(rng_) < 0.55) {
    // Swap two DFG nodes when both operations are legal at the other's PE.
    int a = node_dist(rng_);
    int b = node_dist(rng_);
    return ApplySwapMove(state, a, b);
  }

  return ApplyMoveToFreeResource(state);
}

bool PlacementSearchEngine::ApplySwapMove(PlacementState& state, int a, int b) const {
  if (a == b) return false;
  int ra = state.dfg_to_mrrg[a];
  int rb = state.dfg_to_mrrg[b];
  if (ra < 0 || rb < 0) return false;
  if (!IsCompatible(a, rb) || !IsCompatible(b, ra)) return false;
  std::swap(state.dfg_to_mrrg[a], state.dfg_to_mrrg[b]);
  state.mrrg_to_dfg[ra] = b;
  state.mrrg_to_dfg[rb] = a;
  return true;
}

bool PlacementSearchEngine::ApplyMoveToFreeResource(PlacementState& state) {
  const int node_num = dfg_.GetNodeNum();
  if (node_num <= 0) return false;
  std::uniform_int_distribution<int> node_dist(0, node_num - 1);
  int a = node_dist(rng_);
  std::vector<int> free_candidates;
  for (int r = 0; r < mrrg_.GetNodeNum(); r++) {
    if (CanOccupyResource(a, r, state)) {
      free_candidates.push_back(r);
    }
  }
  if (free_candidates.empty()) return false;
  std::uniform_int_distribution<int> resource_dist(
      0, static_cast<int>(free_candidates.size()) - 1);
  int old_r = state.dfg_to_mrrg[a];
  int new_r = free_candidates[resource_dist(rng_)];
  state.dfg_to_mrrg[a] = new_r;
  state.mrrg_to_dfg[old_r] = -1;
  state.mrrg_to_dfg[new_r] = a;
  return true;
}

}  // namespace mapper::detail::placement_search
