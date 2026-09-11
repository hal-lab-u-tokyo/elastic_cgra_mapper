#include "../engine_internal.hpp"

namespace mapper::detail::placement_search {

// Initial placement, candidate ranking, and traversal execution.

std::optional<int> PlacementSearchEngine::PlaceInitialNode(
    int dfg_node_id, PlacementState& state) {
  std::vector<int> candidates = CompatibleResources(dfg_node_id, state);
  if (candidates.empty()) return std::nullopt;
  std::shuffle(candidates.begin(), candidates.end(), rng_);
  int chosen = candidates.front();
  if (UsesPhysicalPlacementStage()) {
    std::sort(candidates.begin(), candidates.end(), [&](int a, int b) {
      const double sa = Placement2DInitialPlacementScore(dfg_node_id, a);
      const double sb = Placement2DInitialPlacementScore(dfg_node_id, b);
      if (sa != sb) return sa < sb;
      return a < b;
    });
    const double best_score =
        Placement2DInitialPlacementScore(dfg_node_id, candidates.front());
    std::vector<int> best_candidates;
    for (int candidate : candidates) {
      if (Placement2DInitialPlacementScore(dfg_node_id, candidate) <=
          best_score + 1.0e-9) {
        best_candidates.push_back(candidate);
      }
    }
    std::uniform_int_distribution<int> dist(
        0, static_cast<int>(best_candidates.size()) - 1);
    chosen = best_candidates[dist(rng_)];
  } else if (IsIONode(dfg_node_id)) {
    std::sort(candidates.begin(), candidates.end(), [&](int a, int b) {
      const double sa = IOPlacementScore(dfg_node_id, a);
      const double sb = IOPlacementScore(dfg_node_id, b);
      if (sa != sb) return sa < sb;
      return a < b;
    });
    const double best_score = IOPlacementScore(dfg_node_id, candidates.front());
    std::vector<int> best_candidates;
    for (int candidate : candidates) {
      if (IOPlacementScore(dfg_node_id, candidate) <= best_score + 1.0) {
        best_candidates.push_back(candidate);
      }
    }
    std::uniform_int_distribution<int> dist(
        0, static_cast<int>(best_candidates.size()) - 1);
    chosen = best_candidates[dist(rng_)];
  }
  state.dfg_to_mrrg[dfg_node_id] = chosen;
  state.mrrg_to_dfg[chosen] = dfg_node_id;
  return chosen;
}

double PlacementSearchEngine::DegreeMatchScore(
    int dfg_node_id, int mrrg_node_id, const PlacementState& state) const {
  const int target_degree = annotation_.degree[dfg_node_id];
  const int freedom = ResourceFreedom(mrrg_node_id, state);
  if (freedom >= target_degree) return freedom - target_degree;
  return 4.0 * (target_degree - freedom);
}

int PlacementSearchEngine::FreeResourcesAtDistance(
    int anchor_mrrg_node, int target_distance,
    const PlacementState& state) const {
  int result = 0;
  for (int r = 0; r < mrrg_.GetNodeNum(); r++) {
    if (UsesPhysicalPeExclusivePlacement() &&
        IsPhysicalPositionOccupied(r, state)) {
      continue;
    }
    if (!UsesPhysicalPeExclusivePlacement() && state.mrrg_to_dfg[r] != -1) {
      continue;
    }
    if (!SupportsAnyDFGOperation(r)) continue;
    const int distance = UsesPhysicalPlacementStage()
                             ? PhysicalDistance(anchor_mrrg_node, r)
                             : ResourceDistance(anchor_mrrg_node, r);
    if (distance == target_distance) result++;
  }
  return result;
}

double PlacementSearchEngine::AnnotationScore(const TraversalStep& step,
                                              int candidate_mrrg_node,
                                              const PlacementState& state,
                                              SequenceAnnotationKind kind,
                                              bool lookahead_only) const {
  double best = kImpossibleCost;
  for (const auto& annotation : step.annotations) {
    if (annotation.kind != kind) continue;
    if (lookahead_only && annotation.distance < 2) continue;
    if (!lookahead_only && kind == SequenceAnnotationKind::kReconvergence &&
        annotation.distance >= 2) {
      continue;
    }

    double score = 0.0;
    const int anchor_mrrg_node = state.dfg_to_mrrg[annotation.anchor_node];
    if (kind == SequenceAnnotationKind::kIO) {
      score += IOPlacementScore(step.target_node, candidate_mrrg_node);
      if (anchor_mrrg_node >= 0) {
        const int distance =
            UsesPhysicalPlacementStage()
                ? PhysicalDistance(candidate_mrrg_node, anchor_mrrg_node)
                : ResourceDistance(candidate_mrrg_node, anchor_mrrg_node);
        score += 2.0 * std::abs(distance - annotation.distance);
      }
    } else if (anchor_mrrg_node >= 0) {
      const int distance =
          UsesPhysicalPlacementStage()
              ? PhysicalDistance(candidate_mrrg_node, anchor_mrrg_node)
              : ResourceDistance(candidate_mrrg_node, anchor_mrrg_node);
      score += 3.0 * std::abs(distance - annotation.distance);
      if (lookahead_only) {
        score -= 0.25 * FreeResourcesAtDistance(anchor_mrrg_node,
                                                annotation.distance - 1, state);
      }
    } else {
      continue;
    }
    best = std::min(best, score);
  }
  return best;
}

bool PlacementSearchEngine::HasAnnotationKind(const TraversalStep& step,
                                              SequenceAnnotationKind kind,
                                              bool lookahead_only) const {
  for (const auto& annotation : step.annotations) {
    if (annotation.kind != kind) continue;
    if (lookahead_only && annotation.distance < 2) continue;
    if (!lookahead_only && kind == SequenceAnnotationKind::kReconvergence &&
        annotation.distance >= 2) {
      continue;
    }
    return true;
  }
  return false;
}

CandidateRank PlacementSearchEngine::TraversalPlacementRank(
    const TraversalStep& step, int candidate_mrrg_node,
    const PlacementState& state, bool use_annotations, int random_order) const {
  const int placed_mrrg_node = state.dfg_to_mrrg[step.placed_node];
  const int locality_distance = DirectedDistanceCost(
      UsesPhysicalPlacementStage()
          ? PhysicalDistance(placed_mrrg_node, candidate_mrrg_node)
          : ResourceDistance(placed_mrrg_node, candidate_mrrg_node));
  CandidateRank rank;
  rank.annotation_priority = 3;
  rank.annotation_cost = 0.0;
  rank.locality_distance = locality_distance;
  rank.degree_cost =
      use_annotations
          ? DegreeMatchScore(step.target_node, candidate_mrrg_node, state)
          : 0.0;
  rank.random_order = random_order;

  if (use_annotations &&
      HasAnnotationKind(step, SequenceAnnotationKind::kIO, false)) {
    rank.annotation_priority = 0;
    rank.annotation_cost = AnnotationScore(step, candidate_mrrg_node, state,
                                           SequenceAnnotationKind::kIO, false);
    return rank;
  }
  if (use_annotations &&
      HasAnnotationKind(step, SequenceAnnotationKind::kReconvergence, true)) {
    rank.annotation_priority = 1;
    rank.annotation_cost =
        AnnotationScore(step, candidate_mrrg_node, state,
                        SequenceAnnotationKind::kReconvergence, true);
    return rank;
  }
  if (use_annotations &&
      HasAnnotationKind(step, SequenceAnnotationKind::kReconvergence, false)) {
    rank.annotation_priority = 2;
    rank.annotation_cost =
        AnnotationScore(step, candidate_mrrg_node, state,
                        SequenceAnnotationKind::kReconvergence, false);
    return rank;
  }

  return rank;
}

bool PlacementSearchEngine::PlaceTraversalStep(const TraversalStep& step,
                                               PlacementState& state,
                                               bool use_annotations) {
  if (state.dfg_to_mrrg[step.target_node] >= 0) return true;
  if (state.dfg_to_mrrg[step.placed_node] < 0) {
    if (!PlaceInitialNode(step.placed_node, state).has_value()) return false;
  }
  std::vector<int> candidates = ClosestCompatibleResources(
      step.target_node, state.dfg_to_mrrg[step.placed_node], state);
  if (candidates.empty()) return false;
  if (search_kind_ == PlacementSearchKind::kPlacement2DYOTO ||
      search_kind_ == PlacementSearchKind::kModuloPhysicalYOTO) {
    std::sort(candidates.begin(), candidates.end());
  } else {
    std::shuffle(candidates.begin(), candidates.end(), rng_);
  }
  std::vector<std::pair<CandidateRank, int>> scored_candidates;
  scored_candidates.reserve(candidates.size());
  for (int order = 0; order < static_cast<int>(candidates.size()); order++) {
    int candidate = candidates[order];
    scored_candidates.push_back(
        {TraversalPlacementRank(step, candidate, state, use_annotations, order),
         candidate});
  }
  std::sort(scored_candidates.begin(), scored_candidates.end(),
            [](const auto& a, const auto& b) {
              if (a.first.annotation_priority != b.first.annotation_priority) {
                return a.first.annotation_priority <
                       b.first.annotation_priority;
              }
              if (a.first.annotation_cost != b.first.annotation_cost) {
                return a.first.annotation_cost < b.first.annotation_cost;
              }
              if (a.first.locality_distance != b.first.locality_distance) {
                return a.first.locality_distance < b.first.locality_distance;
              }
              if (a.first.degree_cost != b.first.degree_cost) {
                return a.first.degree_cost < b.first.degree_cost;
              }
              return a.first.random_order < b.first.random_order;
            });
  const int chosen = scored_candidates.front().second;
  state.dfg_to_mrrg[step.target_node] = chosen;
  state.mrrg_to_dfg[chosen] = step.target_node;
  return true;
}

std::optional<PlacementState>
PlacementSearchEngine::ConstructTraversalPlacement(
    bool use_paper_traversal_plan, bool use_annotations) {
  TraversalPlan plan = use_paper_traversal_plan
                           ? BuildPlacement2DTraversalPlan(use_annotations)
                           : BuildTraversalPlan(use_annotations);
  PlacementState state;
  state.dfg_to_mrrg.assign(dfg_.GetNodeNum(), -1);
  state.mrrg_to_dfg.assign(mrrg_.GetNodeNum(), -1);

  for (const auto& step : plan.sequence) {
    if (!PlaceTraversalStep(step, state, use_annotations)) return std::nullopt;
  }
  for (int node_id = 0; node_id < dfg_.GetNodeNum(); node_id++) {
    if (state.dfg_to_mrrg[node_id] < 0) {
      if (!PlaceInitialNode(node_id, state).has_value()) return std::nullopt;
    }
  }
  return state;
}

}  // namespace mapper::detail::placement_search
