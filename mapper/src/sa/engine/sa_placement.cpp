#include <mapper/sa/engine/sa_engine.hpp>

namespace mapper::sa {

std::optional<PlacementState> SaEngine::RunSAMultiSeed(
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

std::optional<PlacementState> SaEngine::RunSA(
    std::chrono::steady_clock::time_point start) {
  if (UsesSpatialResources()) return RunPlacement2DSA(start);

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

std::optional<PlacementState> SaEngine::RunPlacement2DSA(
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

std::optional<PlacementState> SaEngine::ConstructGreedyPlacement() {
  return ConstructTraversalPlacement(false, false);
}

// DFG traversal roots, order, and YOTT annotations.

int SaEngine::OtherEndpoint(const DFGEdgeInfo& edge, int node_id) const {
  if (edge.source == node_id) return edge.target;
  if (edge.target == node_id) return edge.source;
  return -1;
}

int SaEngine::FindLastStepTargeting(const TraversalPlan& plan,
                                    int target_node) const {
  for (int i = static_cast<int>(plan.sequence.size()) - 1; i >= 0; i--) {
    if (plan.sequence[i].target_node == target_node) return i;
  }
  return -1;
}

void SaEngine::BackpropagateAnnotation(TraversalPlan& plan, int from_node,
                                       int anchor_node,
                                       SequenceAnnotationKind kind,
                                       int max_depth,
                                       int initial_distance) const {
  int current = from_node;
  int distance = initial_distance;
  while (distance <= max_depth) {
    int step_index = FindLastStepTargeting(plan, current);
    if (step_index < 0) return;
    plan.sequence[step_index].annotations.push_back(
        SequenceAnnotation{kind, anchor_node, distance});
    current = plan.sequence[step_index].placed_node;
    distance++;
  }
}

std::vector<double> SaEngine::BuildBetweennessCentralityScore() const {
  const int n = dfg_.GetNodeNum();
  std::vector<double> centrality(n, 0.0);
  for (int source = 0; source < n; source++) {
    std::vector<std::vector<int>> predecessors_on_paths(n);
    std::vector<int> distance(n, -1);
    std::vector<double> sigma(n, 0.0);
    std::vector<double> dependency(n, 0.0);
    std::vector<int> order;
    std::queue<int> q;

    distance[source] = 0;
    sigma[source] = 1.0;
    q.push(source);
    while (!q.empty()) {
      const int v = q.front();
      q.pop();
      order.push_back(v);
      for (int w : successors_[v]) {
        if (w == v) continue;
        if (distance[w] < 0) {
          distance[w] = distance[v] + 1;
          q.push(w);
        }
        if (distance[w] == distance[v] + 1) {
          sigma[w] += sigma[v];
          predecessors_on_paths[w].push_back(v);
        }
      }
    }

    for (auto it = order.rbegin(); it != order.rend(); ++it) {
      const int w = *it;
      if (sigma[w] == 0.0) continue;
      for (int v : predecessors_on_paths[w]) {
        dependency[v] += (sigma[v] / sigma[w]) * (1.0 + dependency[w]);
      }
      if (w != source) centrality[w] += dependency[w];
    }
  }
  return centrality;
}

TraversalPlan SaEngine::BuildPlacement2DTraversalPlan(bool annotate) {
  const int kPlacement2DMaxAnnotationDistance = 2;
  TraversalPlan plan;
  std::vector<char> visited_nodes(dfg_.GetNodeNum(), false);
  std::vector<std::vector<int>> local_fanin = predecessors_;
  std::vector<std::vector<int>> local_fanout = successors_;
  std::vector<std::pair<int, int>> stack;
  for (int root : GetRawOutputTraversalRoots()) stack.push_back({root, 1});
  const auto critical_path = BuildCriticalPathScore();
  const auto betweenness = BuildBetweennessCentralityScore();
  int selection_mode = 0;
  if (annotate && false) {
    std::uniform_int_distribution<int> mode_dist(0, 3);
    selection_mode = mode_dist(rng_);
  }

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
    TraversalStep step{from, to, FindDFGEdgeId(from, to), {}};
    if (annotate && IsIONode(to)) {
      step.annotations.push_back(
          SequenceAnnotation{SequenceAnnotationKind::kIO, to, 1});
      BackpropagateAnnotation(plan, from, to, SequenceAnnotationKind::kIO,
                              kPlacement2DMaxAnnotationDistance, 2);
    }
    if (annotate && visited_nodes[to]) {
      BackpropagateAnnotation(plan, from, to,
                              SequenceAnnotationKind::kReconvergence,
                              kPlacement2DMaxAnnotationDistance, 1);
    }
    plan.sequence.push_back(step);
  };

  if (stack.empty()) push_remaining();
  while (!stack.empty()) {
    int a = stack.back().first;
    int direction = stack.back().second;
    stack.pop_back();
    if (a < 0 || a >= dfg_.GetNodeNum()) continue;
    if (!visited_nodes[a]) plan.roots.push_back(a);

    const int fanin = static_cast<int>(local_fanin[a].size());
    const int fanout = static_cast<int>(local_fanout[a].size());
    int b = -1;
    if (direction == 1) {
      if (fanout >= 1) {
        b = ChoosePlacement2DNeighbor(local_fanout[a], true, selection_mode,
                                      true, critical_path, betweenness);
        for (int i = 0; i < fanin; i++) stack.push_back({a, 1});
        stack.push_back({b, 0});
        remove_value(local_fanout[a], b);
        remove_value(local_fanin[b], a);
        append_step(a, b);
      } else if (fanin >= 1) {
        b = ChoosePlacement2DNeighbor(local_fanin[a], false, selection_mode,
                                      true, critical_path, betweenness);
        stack.push_back({a, 1});
        for (int i = 0; i < fanin; i++) stack.push_back({b, 1});
        remove_value(local_fanin[a], b);
        remove_value(local_fanout[b], a);
        append_step(a, b);
      }
    } else {
      if (fanin >= 1) {
        b = ChoosePlacement2DNeighbor(local_fanin[a], false, selection_mode,
                                      false, critical_path, betweenness);
        for (int i = 0; i < fanout; i++) stack.push_back({a, 0});
        stack.push_back({b, 1});
        remove_value(local_fanin[a], b);
        remove_value(local_fanout[b], a);
        append_step(a, b);
      } else if (fanout >= 1) {
        b = ChoosePlacement2DNeighbor(local_fanout[a], true, selection_mode,
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

TraversalPlan SaEngine::BuildTraversalPlan(bool annotate) {
  constexpr int kReconvergenceDepth = 3;
  constexpr int kIODepth = 3;
  TraversalPlan plan;
  std::vector<char> visited_nodes(dfg_.GetNodeNum(), false);
  std::vector<char> visited_edges(dfg_edges_.size(), false);
  std::vector<int> stack = GetOutputTraversalRoots();

  auto push_remaining_component_root = [&]() {
    for (int v = 0; v < dfg_.GetNodeNum(); v++) {
      if (!visited_nodes[v]) {
        stack.push_back(v);
        return true;
      }
    }
    return false;
  };

  if (stack.empty()) push_remaining_component_root();

  while (!stack.empty()) {
    int placed_node = stack.back();
    stack.pop_back();
    if (!visited_nodes[placed_node]) {
      visited_nodes[placed_node] = true;
      plan.roots.push_back(placed_node);
    }

    while (true) {
      std::vector<int> unvisited_edges =
          UnvisitedIncidentEdges(placed_node, visited_edges);
      if (unvisited_edges.empty()) break;
      if (unvisited_edges.size() > 1) stack.push_back(placed_node);

      const int edge_id =
          PickTraversalEdge(placed_node, visited_nodes, visited_edges);
      if (edge_id < 0) break;
      visited_edges[edge_id] = true;
      const int target_node = OtherEndpoint(dfg_edges_[edge_id], placed_node);
      if (target_node < 0 || target_node == placed_node) continue;

      TraversalStep pending_step{placed_node, target_node, edge_id, {}};
      if (annotate && IsIONode(target_node)) {
        pending_step.annotations.push_back(
            SequenceAnnotation{SequenceAnnotationKind::kIO, target_node, 1});
        BackpropagateAnnotation(plan, placed_node, target_node,
                                SequenceAnnotationKind::kIO, kIODepth, 2);
      }

      if (!visited_nodes[target_node]) {
        plan.sequence.push_back(pending_step);
        visited_nodes[target_node] = true;
        placed_node = target_node;
        continue;
      }

      if (annotate) {
        BackpropagateAnnotation(plan, placed_node, target_node,
                                SequenceAnnotationKind::kReconvergence,
                                kReconvergenceDepth, 1);
      }

      if (stack.empty()) break;
      placed_node = stack.back();
      stack.pop_back();
    }

    if (stack.empty()) {
      bool has_unvisited_edge = false;
      for (char visited : visited_edges) {
        if (!visited) {
          has_unvisited_edge = true;
          break;
        }
      }
      if (has_unvisited_edge) {
        for (int v = 0; v < dfg_.GetNodeNum(); v++) {
          if (!UnvisitedIncidentEdges(v, visited_edges).empty()) {
            stack.push_back(v);
            break;
          }
        }
      } else {
        push_remaining_component_root();
      }
    }
  }

  return plan;
}

// Initial placement, candidate ranking, and traversal execution.

std::optional<int> SaEngine::PlaceInitialNode(int dfg_node_id,
                                              PlacementState& state) {
  std::vector<int> candidates = CompatibleResources(dfg_node_id, state);
  if (candidates.empty()) return std::nullopt;
  std::shuffle(candidates.begin(), candidates.end(), rng_);
  int chosen = candidates.front();
  if (IsIONode(dfg_node_id)) {
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

int SaEngine::FreeResourcesAtDistance(int anchor_mrrg_node, int target_distance,
                                      const PlacementState& state) const {
  int result = 0;
  for (int r = 0; r < mrrg_.GetNodeNum(); r++) {
    if (UsesSpatialResources() && IsPhysicalPositionOccupied(r, state)) {
      continue;
    }
    if (!UsesSpatialResources() && state.mrrg_to_dfg[r] != -1) {
      continue;
    }
    if (!SupportsAnyDFGOperation(r)) continue;
    const int distance = ResourceDistance(anchor_mrrg_node, r);
    if (distance == target_distance) result++;
  }
  return result;
}

double SaEngine::AnnotationScore(const TraversalStep& step,
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
            false ? PhysicalDistance(candidate_mrrg_node, anchor_mrrg_node)
                  : ResourceDistance(candidate_mrrg_node, anchor_mrrg_node);
        score += 2.0 * std::abs(distance - annotation.distance);
      }
    } else if (anchor_mrrg_node >= 0) {
      const int distance =
          false ? PhysicalDistance(candidate_mrrg_node, anchor_mrrg_node)
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

bool SaEngine::HasAnnotationKind(const TraversalStep& step,
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

CandidateRank SaEngine::TraversalPlacementRank(const TraversalStep& step,
                                               int candidate_mrrg_node,
                                               const PlacementState& state,
                                               bool use_annotations,
                                               int random_order) const {
  const int placed_mrrg_node = state.dfg_to_mrrg[step.placed_node];
  const int locality_distance = DirectedDistanceCost(
      false ? PhysicalDistance(placed_mrrg_node, candidate_mrrg_node)
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

bool SaEngine::PlaceTraversalStep(const TraversalStep& step,
                                  PlacementState& state, bool use_annotations) {
  if (state.dfg_to_mrrg[step.target_node] >= 0) return true;
  if (state.dfg_to_mrrg[step.placed_node] < 0) {
    if (!PlaceInitialNode(step.placed_node, state).has_value()) return false;
  }
  std::vector<int> candidates = ClosestCompatibleResources(
      step.target_node, state.dfg_to_mrrg[step.placed_node], state);
  if (candidates.empty()) return false;
  if (false || false) {
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

std::optional<PlacementState> SaEngine::ConstructTraversalPlacement(
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

double SaEngine::EdgePlacementCost(const DFGEdgeInfo& edge,
                                   const std::vector<int>& dfg_to_mrrg) const {
  return ResourcePairPlacementCost(dfg_to_mrrg[edge.source],
                                   dfg_to_mrrg[edge.target]);
}

std::optional<PlacementState> SaEngine::RandomLegalPlacement() {
  PlacementState state;
  state.dfg_to_mrrg.assign(dfg_.GetNodeNum(), -1);
  state.mrrg_to_dfg.assign(mrrg_.GetNodeNum(), -1);
  std::vector<int> node_order(dfg_.GetNodeNum());
  std::iota(node_order.begin(), node_order.end(), 0);
  if (UsesSpatialResources()) {
    std::shuffle(node_order.begin(), node_order.end(), rng_);
  } else {
    std::sort(node_order.begin(), node_order.end(), [&](int a, int b) {
      return annotation_.degree[a] > annotation_.degree[b];
    });
  }
  for (int dfg_node_id : node_order) {
    auto candidates = CompatibleResources(dfg_node_id, state);
    if (candidates.empty()) return std::nullopt;
    std::uniform_int_distribution<int> dist(
        0, static_cast<int>(candidates.size()) - 1);
    int chosen = candidates[dist(rng_)];
    state.dfg_to_mrrg[dfg_node_id] = chosen;
    state.mrrg_to_dfg[chosen] = dfg_node_id;
  }
  return state;
}

// Initial placement and SIS order.

std::optional<PlacementState> SaEngine::GridRandomPlacement() {
  PlacementState state;
  state.dfg_to_cell.assign(dfg_.GetNodeNum(), -1);
  state.cell_to_dfg.assign(rows_ * cols_, -1);
  std::vector<int> nodes(dfg_.GetNodeNum());
  std::iota(nodes.begin(), nodes.end(), 0);
  std::shuffle(nodes.begin(), nodes.end(), rng_);
  std::stable_sort(nodes.begin(), nodes.end(), [&](int a, int b) {
    if (GridIsIONode(a) != GridIsIONode(b))
      return GridIsIONode(a) > GridIsIONode(b);
    return degree_[a] > degree_[b];
  });
  for (int node : nodes) {
    auto cells = GridCompatibleCells(node, state);
    if (cells.empty()) return std::nullopt;
    std::uniform_int_distribution<int> dist(0,
                                            static_cast<int>(cells.size()) - 1);
    GridPlaceNode(node, cells[dist(rng_)], state);
  }
  return state;
}

bool SaEngine::GridApplySwap(PlacementState& state, int a, int b) {
  if (a == b) return false;
  const int cell_a = state.dfg_to_cell[a];
  const int cell_b = state.dfg_to_cell[b];
  if (cell_a < 0 || cell_b < 0) return false;
  if (!GridIsCompatible(a, cell_b) || !GridIsCompatible(b, cell_a))
    return false;
  state.dfg_to_cell[a] = cell_b;
  state.dfg_to_cell[b] = cell_a;
  state.cell_to_dfg[cell_a] = b;
  state.cell_to_dfg[cell_b] = a;
  return true;
}

bool SaEngine::GridApplyMoveToFreeCell(PlacementState& state, int node) {
  auto cells = GridCompatibleCells(node, state);
  if (cells.empty()) return false;
  std::uniform_int_distribution<int> dist(0,
                                          static_cast<int>(cells.size()) - 1);
  const int old_cell = state.dfg_to_cell[node];
  const int new_cell = cells[dist(rng_)];
  state.cell_to_dfg[old_cell] = -1;
  state.cell_to_dfg[new_cell] = node;
  state.dfg_to_cell[node] = new_cell;
  return true;
}

std::optional<PlacementState> SaEngine::GridRunSA(
    std::chrono::steady_clock::time_point start) {
  auto current = GridRandomPlacement();
  if (!current.has_value()) return std::nullopt;
  PlacementState best = *current;
  double current_cost = GridPlacementCost(*current);
  double best_cost = current_cost;
  double temperature =
      std::max(10.0, current_cost / std::max(1, dfg_.GetNodeNum()));
  std::uniform_real_distribution<double> unit(0.0, 1.0);
  std::uniform_int_distribution<int> node_dist(
      0, std::max(0, dfg_.GetNodeNum() - 1));

  for (int iteration = 0;
       iteration < MaxIterations() && !HasTimedOut(start, 0.05); iteration++) {
    PlacementState next = *current;
    bool changed = false;
    if (unit(rng_) < 0.75 || dfg_.GetNodeNum() < rows_ * cols_) {
      changed = GridApplySwap(next, node_dist(rng_), node_dist(rng_));
    } else {
      changed = GridApplyMoveToFreeCell(next, node_dist(rng_));
    }
    if (!changed) continue;
    const double next_cost = GridPlacementCost(next);
    const double delta = next_cost - current_cost;
    if (delta <= 0.0 || unit(rng_) < std::exp(-delta / temperature)) {
      *current = next;
      current_cost = next_cost;
    }
    if (next_cost < best_cost) {
      best = next;
      best_cost = next_cost;
    }
    temperature *= 0.9975;
    if (temperature < 1.0e-4) temperature = std::max(1.0, best_cost / 10.0);
  }
  return best;
}

std::optional<PlacementState> SaEngine::GridRunSAMultiSeed(
    std::chrono::steady_clock::time_point start) {
  std::optional<PlacementState> best;
  double best_cost = std::numeric_limits<double>::infinity();
  int seeds = 0;
  for (int seed = 0; seed < SeedCount() && !HasTimedOut(start, 0.05); seed++) {
    ResetSeed(seed);
    auto placement = GridRunSA(start);
    seeds++;
    if (!placement.has_value()) continue;
    const double cost = GridPlacementCost(*placement);
    if (cost < best_cost) {
      best = placement;
      best_cost = cost;
    }
  }
  Log("array_sa seeds=" + std::to_string(seeds));
  return best;
}

double SaEngine::GridPlacementCost(const PlacementState& state) const {
  double cost = 0.0;
  for (const auto& edge : edges_) {
    const int from = state.dfg_to_cell[edge.source];
    const int to = state.dfg_to_cell[edge.target];
    if (from < 0 || to < 0) return kGridImpossibleCost;
    cost += GridDistanceCost(from, to);
  }
  return cost;
}

std::vector<int> SaEngine::GridCompatibleCells(
    int dfg_node, const PlacementState& state) const {
  std::vector<int> cells;
  cells.reserve(compatible_cells_[dfg_node].size());
  for (int cell : compatible_cells_[dfg_node]) {
    if (GridCanPlace(dfg_node, cell, state)) cells.push_back(cell);
  }
  return cells;
}

void SaEngine::GridPlaceNode(int dfg_node, int cell,
                             PlacementState& state) const {
  state.dfg_to_cell[dfg_node] = cell;
  state.cell_to_dfg[cell] = dfg_node;
}

}  // namespace mapper::sa
