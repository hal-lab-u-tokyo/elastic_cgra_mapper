#include "../engine_internal.hpp"

namespace mapper::detail::placement_search {

std::optional<PlacementState>
PlacementSearchEngine::ConstructGreedyPlacement() {
  return ConstructTraversalPlacement(UsesPaperTraversalPlan(), IsYOTTLike());
}

std::vector<PlacementState>
PlacementSearchEngine::ConstructGreedyPlacementCandidates() {
  std::vector<PlacementState> result;
  auto primary =
      ConstructTraversalPlacement(UsesPaperTraversalPlan(), IsYOTTLike());
  PushPlacementCandidate(result, primary);

  // Fallback variants intentionally mix the paper-style traversal with older
  // routing-aware placements. The pure YOTO/YOTT mappers do not enter this
  // block, so comparison results remain attributable to one strategy.
  if (UsesModuloFallbackCandidates()) {
    auto fallback = ConstructTraversalPlacement(false, IsYOTTLike());
    PushPlacementCandidate(result, fallback);
    if (IsYOTTLike()) {
      auto yoto_style_primary = ConstructTraversalPlacement(true, false);
      PushPlacementCandidate(result, yoto_style_primary);
      auto yoto_style_fallback = ConstructTraversalPlacement(false, false);
      PushPlacementCandidate(result, yoto_style_fallback);
    }
  }
  return result;
}

std::optional<PlacementState> PlacementSearchEngine::RunGreedyMultiStart(
    std::chrono::steady_clock::time_point start) {
  const int max_trials = MaxTrials();
  const int seed_count = SeedCount();
  std::optional<PlacementState> best_unrouted;
  double best_unrouted_cost = std::numeric_limits<double>::infinity();
  std::vector<std::pair<double, PlacementState>> elite;
  const bool route_all_placement_trials =
      search_kind_ == PlacementSearchKind::kYOTO ||
      search_kind_ == PlacementSearchKind::kYOTT ||
      search_kind_ == PlacementSearchKind::kYOTOWithFallback ||
      search_kind_ == PlacementSearchKind::kYOTTWithFallback ||
      IsPlacement2DTraversalLike() || IsPhysicalThenContextLike();
  const int placement_variants_per_trial =
      UsesModuloFallbackCandidates() ? (IsYOTTLike() ? 4 : 2) : 1;
  const int elite_limit =
      route_all_placement_trials
          ? std::max(8, max_trials * seed_count * placement_variants_per_trial)
          : std::max(8, RoutingRetryCount() * 2);
  int total_trials = 0;

  for (int seed_index = 0; seed_index < seed_count && !HasTimedOut(start, 0.1);
       seed_index++) {
    ResetSeed(seed_index);
    for (int trial = 0; trial < max_trials && !HasTimedOut(start, 0.1);
         trial++, total_trials++) {
      auto placements = ConstructGreedyPlacementCandidates();
      for (const auto& placement : placements) {
        const double cost = PlacementCost(placement);
        if (cost < best_unrouted_cost) {
          best_unrouted = placement;
          best_unrouted_cost = cost;
        }
        elite.push_back({cost, placement});
        std::sort(elite.begin(), elite.end(), [](const auto& a, const auto& b) {
          return a.first < b.first;
        });
        if (static_cast<int>(elite.size()) > elite_limit) {
          elite.pop_back();
        }
      }
    }
  }

  std::optional<PlacementState> best_routeable;
  double best_routeable_cost = std::numeric_limits<double>::infinity();
  if (options_.placement_only) {
    Log("greedy_search trials=" + std::to_string(total_trials) +
        " elite=" + std::to_string(elite.size()) + " placement_only=1");
    return best_unrouted;
  }
  for (const auto& candidate : elite) {
    if (HasTimedOut(start, 0.02)) break;
    auto route_usage = TryRoutePlacement(candidate.second);
    if (route_usage.has_value() && candidate.first < best_routeable_cost) {
      best_routeable = candidate.second;
      best_routeable_cost = candidate.first;
    }
  }

  Log("greedy_search trials=" + std::to_string(total_trials) +
      " elite=" + std::to_string(elite.size()) +
      " routeable=" + (best_routeable.has_value() ? "1" : "0"));
  if (best_routeable.has_value()) return best_routeable;
  return best_unrouted;
}

}  // namespace mapper::detail::placement_search
