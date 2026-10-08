#include "../../array_engine_internal.hpp"

namespace mapper::detail::placement2d {

// Paper-guided YOTO/YOTT pipeline.
//
//   1. BuildPaperGuidedTraversalPlan()
//      Build an edge-local traversal plan. YOTT additionally records soft
//      annotations for I/O and reconvergent edges.
//   2. ConstructPaperGuidedTraversalPlacement()
//      Walk the plan. Place each target near its anchor.
//   3. PlacePaperGuidedStep()
//      Generate closest legal cells, rank them, and place one node.
//   4. RunPaperGuidedTraversalMultiStart()
//      Repeat trials/seeds and keep the lowest PlacementCost().
//
// Ablation handles:
//   - traversal roots/order: RawTraversalRoots(),
//   BuildPaperGuidedTraversalPlan()
//   - neighbor choice: ChoosePaperGuidedNeighbor()
//   - YOTT annotations: BackpropagatePaperGuidedAnnotation()
//   - candidate ranking: PaperGuidedTraversalRank()

// Paper-guided initial placement, edge order, and YOTT annotations.

double Placement2DArrayEngine::PaperGuidedIOPlacementScore(int dfg_node,
                                                           int cell) const {
  double score = BorderDistance(cell);
  if (IsIONode(dfg_node) || IsOutputNode(dfg_node)) {
    score += cell_memory_accessible_[cell] ? -8.0 : 40.0;
  } else {
    score += cell_memory_accessible_[cell] ? -1.0 : 0.0;
  }
  return score;
}

double Placement2DArrayEngine::PaperGuidedInitialPlacementScore(
    int dfg_node, int cell) const {
  if (IsIONode(dfg_node)) {
    double score = PaperGuidedIOPlacementScore(dfg_node, cell);
    if (IsCornerCell(cell)) score += 4.0;
    return score;
  }
  double score = 0.0;
  if (BorderDistance(cell) == 0) score += 50.0;
  if (separate_io_cells_ && cell_memory_accessible_[cell]) score += 100.0;
  return score;
}

void Placement2DArrayEngine::BackpropagatePaperGuidedAnnotation(
    std::vector<Step>& plan, int from_node, int anchor_node,
    StepAnnotationKind kind, int max_depth, int initial_distance) const {
  int current = from_node;
  int distance = initial_distance;
  while (distance <= max_depth) {
    const int step_index = FindLastStepTargeting(plan, current);
    if (step_index < 0) return;
    plan[step_index].annotations.push_back(
        StepAnnotation{kind, anchor_node, distance});
    current = plan[step_index].anchor;
    distance++;
  }
}

int Placement2DArrayEngine::ChoosePaperGuidedNeighbor(
    const std::vector<int>& candidates, bool choose_from_fanout, int mode,
    bool zigzag_take_back, const std::vector<int>& critical_path,
    const std::vector<double>& betweenness) {
  if (candidates.empty()) return -1;
  if (candidates.size() == 1) return candidates.front();

  if (mode == 0) {
    return *std::max_element(
        candidates.begin(), candidates.end(), [&](int a, int b) {
          const int da = choose_from_fanout
                             ? static_cast<int>(successors_[a].size())
                             : static_cast<int>(predecessors_[a].size());
          const int db = choose_from_fanout
                             ? static_cast<int>(successors_[b].size())
                             : static_cast<int>(predecessors_[b].size());
          if (da != db) return da < db;
          return degree_[a] < degree_[b];
        });
  }
  if (mode == 1) {
    return *std::max_element(candidates.begin(), candidates.end(),
                             [&](int a, int b) {
                               if (betweenness[a] != betweenness[b]) {
                                 return betweenness[a] < betweenness[b];
                               }
                               return degree_[a] < degree_[b];
                             });
  }
  if (mode == 2) {
    return *std::max_element(candidates.begin(), candidates.end(),
                             [&](int a, int b) {
                               if (critical_path[a] != critical_path[b]) {
                                 return critical_path[a] < critical_path[b];
                               }
                               return degree_[a] < degree_[b];
                             });
  }
  if (mode == 3)
    return zigzag_take_back ? candidates.back() : candidates.front();

  std::uniform_int_distribution<int> dist(
      0, static_cast<int>(candidates.size()) - 1);
  return candidates[dist(rng_)];
}

std::vector<Step> Placement2DArrayEngine::BuildPaperGuidedTraversalPlan(
    bool annotate) {
  const int kMaxAnnotationDistance = 2;
  std::vector<Step> plan;
  std::vector<char> visited_nodes(dfg_.GetNodeNum(), false);
  std::vector<std::vector<int>> local_fanin = predecessors_;
  std::vector<std::vector<int>> local_fanout = successors_;
  std::vector<std::pair<int, int>> stack;
  for (int root : RawTraversalRoots()) stack.push_back({root, 1});
  const auto critical_path = BuildCriticalPathScore();
  const auto betweenness = BuildBetweennessCentralityScore();
  int selection_mode = TraversalNeighborMode(IsPaperGuidedYOTT());

  auto remove_value = [](std::vector<int>& values, int value) {
    values.erase(std::remove(values.begin(), values.end(), value),
                 values.end());
  };

  auto push_remaining = [&]() {
    for (int node = 0; node < dfg_.GetNodeNum(); node++) {
      if (!local_fanin[node].empty() || !local_fanout[node].empty()) {
        stack.push_back({node, 1});
        return true;
      }
    }
    for (int node = 0; node < dfg_.GetNodeNum(); node++) {
      if (!visited_nodes[node]) {
        stack.push_back({node, 1});
        return true;
      }
    }
    return false;
  };

  auto append_step = [&](int from, int to) {
    if (from < 0 || to < 0 || from == to) return;
    Step step{from, to, FindEdgeId(from, to), {}};
    if (annotate && IsIONode(to)) {
      step.annotations.push_back(
          StepAnnotation{StepAnnotationKind::kIO, to, 1});
      BackpropagatePaperGuidedAnnotation(
          plan, from, to, StepAnnotationKind::kIO, kMaxAnnotationDistance, 2);
    }
    if (annotate && visited_nodes[to]) {
      BackpropagatePaperGuidedAnnotation(plan, from, to,
                                         StepAnnotationKind::kReconvergence,
                                         kMaxAnnotationDistance, 1);
    }
    plan.push_back(step);
  };

  if (stack.empty()) push_remaining();
  while (!stack.empty()) {
    const int a = stack.back().first;
    const int direction = stack.back().second;
    stack.pop_back();
    if (a < 0 || a >= dfg_.GetNodeNum()) continue;

    const int fanin = static_cast<int>(local_fanin[a].size());
    const int fanout = static_cast<int>(local_fanout[a].size());
    int b = -1;
    if (direction == 1) {
      if (fanout >= 1) {
        b = ChoosePaperGuidedNeighbor(local_fanout[a], true, selection_mode,
                                      true, critical_path, betweenness);
        for (int i = 0; i < fanin; i++) stack.push_back({a, 1});
        stack.push_back({b, 0});
        remove_value(local_fanout[a], b);
        remove_value(local_fanin[b], a);
        append_step(a, b);
      } else if (fanin >= 1) {
        b = ChoosePaperGuidedNeighbor(local_fanin[a], false, selection_mode,
                                      true, critical_path, betweenness);
        stack.push_back({a, 1});
        for (int i = 0; i < fanin; i++) stack.push_back({b, 1});
        remove_value(local_fanin[a], b);
        remove_value(local_fanout[b], a);
        append_step(a, b);
      }
    } else {
      if (fanin >= 1) {
        b = ChoosePaperGuidedNeighbor(local_fanin[a], false, selection_mode,
                                      false, critical_path, betweenness);
        for (int i = 0; i < fanout; i++) stack.push_back({a, 0});
        stack.push_back({b, 1});
        remove_value(local_fanin[a], b);
        remove_value(local_fanout[b], a);
        append_step(a, b);
      } else if (fanout >= 1) {
        b = ChoosePaperGuidedNeighbor(local_fanout[a], true, selection_mode,
                                      false, critical_path, betweenness);
        stack.push_back({a, 0});
        for (int i = 0; i < fanout; i++) stack.push_back({b, 0});
        remove_value(local_fanout[a], b);
        remove_value(local_fanin[b], a);
        append_step(a, b);
      }
    }
    visited_nodes[a] = true;
    if (stack.empty()) push_remaining();
  }
  return plan;
}

// Candidate generation and ranking for one edge-local placement step.

int Placement2DArrayEngine::PaperGuidedCellFreedom(
    int cell, const PlacementState& state) const {
  int result = 0;
  for (int other = 0; other < rows_ * cols_; other++) {
    if (other == cell || cell_to_mrrg_[other] < 0) continue;
    if (state.cell_to_dfg[other] >= 0) continue;
    if (DistanceCost(cell, other) == 1) result++;
  }
  return result;
}

double Placement2DArrayEngine::PaperGuidedDegreeMatchScore(
    int dfg_node, int cell, const PlacementState& state) const {
  const int target_degree = degree_[dfg_node];
  const int freedom = PaperGuidedCellFreedom(cell, state);
  if (freedom >= target_degree) return freedom - target_degree;
  return 4.0 * (target_degree - freedom);
}

int Placement2DArrayEngine::FreeCellsAtDistance(
    int anchor_cell, int target_distance, const PlacementState& state) const {
  int result = 0;
  for (int cell = 0; cell < rows_ * cols_; cell++) {
    if (cell_to_mrrg_[cell] < 0 || state.cell_to_dfg[cell] >= 0) continue;
    if (DistanceCost(anchor_cell, cell) == target_distance) result++;
  }
  return result;
}

double Placement2DArrayEngine::PaperGuidedAnnotationScore(
    const Step& step, int candidate_cell, const PlacementState& state,
    StepAnnotationKind kind, bool lookahead_only) const {
  double best = kImpossibleCost;
  for (const auto& annotation : step.annotations) {
    if (annotation.kind != kind) continue;
    if (lookahead_only && annotation.distance < 2) continue;
    if (!lookahead_only && kind == StepAnnotationKind::kReconvergence &&
        annotation.distance >= 2) {
      continue;
    }

    double score = 0.0;
    const int anchor_cell = state.dfg_to_cell[annotation.anchor_node];
    if (kind == StepAnnotationKind::kIO) {
      score += PaperGuidedIOPlacementScore(step.target, candidate_cell);
      if (anchor_cell >= 0) {
        const int distance = DistanceCost(candidate_cell, anchor_cell);
        score += 2.0 * std::abs(distance - annotation.distance);
      }
    } else if (anchor_cell >= 0) {
      const int distance = DistanceCost(candidate_cell, anchor_cell);
      score += 3.0 * std::abs(distance - annotation.distance);
      if (lookahead_only) {
        score -= 0.25 * FreeCellsAtDistance(anchor_cell,
                                            annotation.distance - 1, state);
      }
    } else {
      continue;
    }
    best = std::min(best, score);
  }
  return best;
}

bool Placement2DArrayEngine::HasPaperGuidedAnnotationKind(
    const Step& step, StepAnnotationKind kind, bool lookahead_only) const {
  for (const auto& annotation : step.annotations) {
    if (annotation.kind != kind) continue;
    if (lookahead_only && annotation.distance < 2) continue;
    if (!lookahead_only && kind == StepAnnotationKind::kReconvergence &&
        annotation.distance >= 2) {
      continue;
    }
    return true;
  }
  return false;
}

PaperGuidedRank Placement2DArrayEngine::PaperGuidedTraversalRank(
    const Step& step, int candidate_cell, const PlacementState& state,
    bool use_annotations, int random_order) const {
  const int anchor_cell = state.dfg_to_cell[step.anchor];
  PaperGuidedRank rank;
  rank.annotation_priority = 3;
  rank.annotation_cost = 0.0;
  rank.locality_distance = DistanceCost(anchor_cell, candidate_cell);
  rank.degree_cost = use_annotations ? PaperGuidedDegreeMatchScore(
                                           step.target, candidate_cell, state)
                                     : 0.0;
  rank.random_order = random_order;

  if (candidate_rank_policy_ == "random") {
    rank.annotation_priority = 0;
    rank.annotation_cost = 0.0;
    rank.locality_distance = 0;
    rank.degree_cost = 0.0;
    return rank;
  }
  if (candidate_rank_policy_ == "locality") {
    rank.annotation_priority = 0;
    rank.annotation_cost = 0.0;
    rank.degree_cost = 0.0;
    return rank;
  }
  if (candidate_rank_policy_ == "degree") {
    rank.annotation_priority = 0;
    rank.annotation_cost = 0.0;
    rank.locality_distance = 0;
    rank.degree_cost =
        PaperGuidedDegreeMatchScore(step.target, candidate_cell, state);
    return rank;
  }

  if (use_annotations &&
      HasPaperGuidedAnnotationKind(step, StepAnnotationKind::kIO, false)) {
    rank.annotation_priority = 0;
    rank.annotation_cost = PaperGuidedAnnotationScore(
        step, candidate_cell, state, StepAnnotationKind::kIO, false);
    return rank;
  }
  if (use_annotations && HasPaperGuidedAnnotationKind(
                             step, StepAnnotationKind::kReconvergence, true)) {
    rank.annotation_priority = 1;
    rank.annotation_cost = PaperGuidedAnnotationScore(
        step, candidate_cell, state, StepAnnotationKind::kReconvergence, true);
    return rank;
  }
  if (use_annotations && HasPaperGuidedAnnotationKind(
                             step, StepAnnotationKind::kReconvergence, false)) {
    rank.annotation_priority = 2;
    rank.annotation_cost = PaperGuidedAnnotationScore(
        step, candidate_cell, state, StepAnnotationKind::kReconvergence, false);
    return rank;
  }
  return rank;
}

bool Placement2DArrayEngine::IsBetterPaperGuidedRank(const PaperGuidedRank& a,
                                                     const PaperGuidedRank& b) {
  if (a.annotation_priority != b.annotation_priority) {
    return a.annotation_priority < b.annotation_priority;
  }
  if (a.annotation_cost != b.annotation_cost) {
    return a.annotation_cost < b.annotation_cost;
  }
  if (a.locality_distance != b.locality_distance) {
    return a.locality_distance < b.locality_distance;
  }
  if (a.degree_cost != b.degree_cost) return a.degree_cost < b.degree_cost;
  return a.random_order < b.random_order;
}

std::vector<int> Placement2DArrayEngine::ClosestCompatibleCells(
    int dfg_node, int anchor_cell, const PlacementState& state) const {
  if (candidate_scope_policy_ == "all") {
    return CompatibleCells(dfg_node, state);
  }
  if (anchor_cell < 0) return CompatibleCells(dfg_node, state);
  std::vector<int> result;
  int best_distance = std::numeric_limits<int>::max();
  for (int cell : compatible_cells_[dfg_node]) {
    if (!CanPlace(dfg_node, cell, state)) continue;
    const int distance = DistanceCost(anchor_cell, cell);
    if (distance > best_distance) continue;
    if (distance < best_distance) {
      result.clear();
      best_distance = distance;
    }
    result.push_back(cell);
  }
  return result;
}

int Placement2DArrayEngine::ChoosePaperGuidedInitialCell(
    int dfg_node, PlacementState& state) {
  std::vector<int> candidates = CompatibleCells(dfg_node, state);
  if (candidates.empty()) return -1;
  RecordPlacementSwapAttempts(static_cast<long long>(candidates.size()));
  std::shuffle(candidates.begin(), candidates.end(), rng_);
  std::sort(candidates.begin(), candidates.end(), [&](int a, int b) {
    const double sa = PaperGuidedInitialPlacementScore(dfg_node, a);
    const double sb = PaperGuidedInitialPlacementScore(dfg_node, b);
    if (sa != sb) return sa < sb;
    return a < b;
  });
  const double best_score =
      PaperGuidedInitialPlacementScore(dfg_node, candidates.front());
  std::vector<int> best_candidates;
  for (int cell : candidates) {
    if (PaperGuidedInitialPlacementScore(dfg_node, cell) <=
        best_score + 1.0e-9) {
      best_candidates.push_back(cell);
    }
  }
  std::uniform_int_distribution<int> dist(
      0, static_cast<int>(best_candidates.size()) - 1);
  return best_candidates[dist(rng_)];
}

bool Placement2DArrayEngine::PlacePaperGuidedInitialNode(
    int dfg_node, PlacementState& state) {
  if (state.dfg_to_cell[dfg_node] >= 0) return true;
  const int cell = ChoosePaperGuidedInitialCell(dfg_node, state);
  if (cell < 0) {
    RecordFailure(
        "initial placement has no cell for node=" + NodeLabel(dfg_node) +
        " io=" + std::to_string(IsIONode(dfg_node) ? 1 : 0) +
        " free_compatible=" +
        std::to_string(FreeCPUMappingCompatibleCellCount(dfg_node, state)) +
        " placed=" + std::to_string(PlacedCount(state)) + "/" +
        std::to_string(dfg_.GetNodeNum()));
    return false;
  }
  PlaceNode(dfg_node, cell, state);
  return true;
}

bool Placement2DArrayEngine::PlacePaperGuidedStep(const Step& step,
                                                  PlacementState& state,
                                                  bool use_annotations) {
  if (state.dfg_to_cell[step.target] >= 0) return true;
  if (state.dfg_to_cell[step.anchor] < 0) {
    if (!PlacePaperGuidedInitialNode(step.anchor, state)) return false;
  }
  std::vector<int> candidates = ClosestCompatibleCells(
      step.target, state.dfg_to_cell[step.anchor], state);
  if (candidates.empty()) {
    // Edge-local placement can be over-constrained late in the traversal,
    // especially for large graphs with many I/O nodes. Keep the trial alive
    // and let a later edge or the final fill place this node.
    return true;
  }
  RecordPlacementSwapAttempts(static_cast<long long>(candidates.size()));
  if (candidate_rank_policy_ == "random") {
    std::shuffle(candidates.begin(), candidates.end(), rng_);
  } else if (IsPaperGuidedYOTO()) {
    std::sort(candidates.begin(), candidates.end());
  } else {
    std::shuffle(candidates.begin(), candidates.end(), rng_);
  }

  std::vector<std::pair<PaperGuidedRank, int>> scored;
  scored.reserve(candidates.size());
  for (int order = 0; order < static_cast<int>(candidates.size()); order++) {
    const int cell = candidates[order];
    scored.push_back(
        {PaperGuidedTraversalRank(step, cell, state, use_annotations, order),
         cell});
  }
  std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
    return IsBetterPaperGuidedRank(a.first, b.first);
  });
  PlaceNode(step.target, scored.front().second, state);
  return true;
}

// Trial construction and multi-start search.

std::optional<PlacementState>
Placement2DArrayEngine::ConstructPaperGuidedTraversalPlacement() {
  const bool use_annotations = UsesYOTTAnnotations();
  const auto plan = BuildPaperGuidedTraversalPlan(use_annotations);
  PlacementState state;
  state.dfg_to_cell.assign(dfg_.GetNodeNum(), -1);
  state.cell_to_dfg.assign(rows_ * cols_, -1);
  for (const auto& step : plan) {
    if (!PlacePaperGuidedStep(step, state, use_annotations))
      return std::nullopt;
  }
  for (int node = 0; node < dfg_.GetNodeNum(); node++) {
    if (!PlacePaperGuidedInitialNode(node, state)) return std::nullopt;
  }
  return state;
}

std::optional<PlacementState>
Placement2DArrayEngine::RunPaperGuidedTraversalMultiStart(
    std::chrono::steady_clock::time_point start) {
  std::optional<PlacementState> best;
  double best_cost = std::numeric_limits<double>::infinity();
  int trials = 0;
  for (int seed = 0; seed < SeedCount() && !HasTimedOut(start, 0.05); seed++) {
    ResetSeed(seed);
    for (int trial = 0; trial < MaxTrials() && !HasTimedOut(start, 0.05);
         trial++) {
      auto placement = ConstructPaperGuidedTraversalPlacement();
      trials++;
      if (!placement.has_value()) continue;
      const double cost = PlacementCost(*placement);
      if (cost < best_cost) {
        best = placement;
        best_cost = cost;
      }
    }
  }
  Log("paper_guided_traversal trials=" + std::to_string(trials));
  return best;
}

}  // namespace mapper::detail::placement2d
