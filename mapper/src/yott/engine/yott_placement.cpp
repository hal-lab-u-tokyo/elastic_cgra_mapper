#include <mapper/yott/engine/yott_engine.hpp>

namespace mapper::yott {

std::vector<PlacementState> YottEngine::ConstructGreedyPlacementCandidates() {
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

std::optional<PlacementState> YottEngine::RunGreedyMultiStart(
    std::chrono::steady_clock::time_point start) {
  const int max_trials = MaxTrials();
  const int seed_count = SeedCount();
  std::optional<PlacementState> best_unrouted;
  double best_unrouted_cost = std::numeric_limits<double>::infinity();
  std::vector<std::pair<double, PlacementState>> elite;
  const bool route_all_placement_trials =
      UsesSpatialResources() || false ||
      search_kind_ == PlacementSearchKind::kYOTT || false ||
      search_kind_ == PlacementSearchKind::kYOTTWithFallback || false ||
      IsPhysicalThenContextLike();
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

// Initial placement, candidate ranking, and traversal execution.

std::optional<int> YottEngine::PlaceInitialNode(int dfg_node_id,
                                                PlacementState& state) {
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

int YottEngine::FreeResourcesAtDistance(int anchor_mrrg_node,
                                        int target_distance,
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

double YottEngine::AnnotationScore(const TraversalStep& step,
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

bool YottEngine::HasAnnotationKind(const TraversalStep& step,
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

CandidateRank YottEngine::TraversalPlacementRank(const TraversalStep& step,
                                                 int candidate_mrrg_node,
                                                 const PlacementState& state,
                                                 bool use_annotations,
                                                 int random_order) const {
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

bool YottEngine::PlaceTraversalStep(const TraversalStep& step,
                                    PlacementState& state,
                                    bool use_annotations) {
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

std::optional<PlacementState> YottEngine::ConstructTraversalPlacement(
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

// DFG traversal roots, order, and YOTT annotations.

int YottEngine::OtherEndpoint(const DFGEdgeInfo& edge, int node_id) const {
  if (edge.source == node_id) return edge.target;
  if (edge.target == node_id) return edge.source;
  return -1;
}

int YottEngine::FindLastStepTargeting(const TraversalPlan& plan,
                                      int target_node) const {
  for (int i = static_cast<int>(plan.sequence.size()) - 1; i >= 0; i--) {
    if (plan.sequence[i].target_node == target_node) return i;
  }
  return -1;
}

void YottEngine::BackpropagateAnnotation(TraversalPlan& plan, int from_node,
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

std::vector<double> YottEngine::BuildBetweennessCentralityScore() const {
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

TraversalPlan YottEngine::BuildPlacement2DTraversalPlan(bool annotate) {
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
  if (annotate && IsYOTTLike()) {
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

TraversalPlan YottEngine::BuildTraversalPlan(bool annotate) {
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

// Physical-placement to modulo-context assignment.

std::optional<int> YottEngine::FindSamePhysicalResourceAtContext(
    int base_resource, int dfg_node_id, int desired_context,
    const PlacementState& assigned) const {
  const int context_size = std::max(1, mrrg_.GetMRRGConfig().context_size);
  const auto base_position = mrrg_.GetNodeProperty(base_resource).position_id;
  for (int delta = 0; delta < context_size; delta++) {
    const int context_id = (desired_context + delta) % context_size;
    for (int resource = 0; resource < mrrg_.GetNodeNum(); resource++) {
      if (assigned.mrrg_to_dfg[resource] != -1) continue;
      const auto property = mrrg_.GetNodeProperty(resource);
      if (property.position_id != base_position) continue;
      if (property.context_id != context_id) continue;
      if (!IsCompatible(dfg_node_id, resource)) continue;
      return resource;
    }
  }
  return std::nullopt;
}

std::optional<PlacementState> YottEngine::AssignContextsToPhysicalPlacement(
    const PlacementState& physical_placement) const {
  if (!IsPhysicalThenContextLike()) return physical_placement;

  const int context_size = std::max(1, mrrg_.GetMRRGConfig().context_size);
  if (context_size == 1) return physical_placement;

  const std::vector<int> levels = BuildASAPContextLevels();
  PlacementState assigned;
  assigned.dfg_to_mrrg.assign(dfg_.GetNodeNum(), -1);
  assigned.mrrg_to_dfg.assign(mrrg_.GetNodeNum(), -1);

  for (int dfg_node_id = 0; dfg_node_id < dfg_.GetNodeNum(); dfg_node_id++) {
    const int base_resource = physical_placement.dfg_to_mrrg[dfg_node_id];
    if (base_resource < 0) return std::nullopt;
    const int desired_context = levels[dfg_node_id] % context_size;
    auto resource = FindSamePhysicalResourceAtContext(
        base_resource, dfg_node_id, desired_context, assigned);
    if (!resource.has_value()) return std::nullopt;
    assigned.dfg_to_mrrg[dfg_node_id] = resource.value();
    assigned.mrrg_to_dfg[resource.value()] = dfg_node_id;
  }

  return assigned;
}

std::optional<PlacementState> YottEngine::FinalizePhysicalPlacementIfNeeded(
    const PlacementState& placement) const {
  if (!IsPhysicalThenContextLike()) return placement;
  return AssignContextsToPhysicalPlacement(placement);
}

double YottEngine::EdgePlacementCost(
    const DFGEdgeInfo& edge, const std::vector<int>& dfg_to_mrrg) const {
  return ResourcePairPlacementCost(dfg_to_mrrg[edge.source],
                                   dfg_to_mrrg[edge.target]);
}

// cpu_mapping-style YOTO/YOTT pipeline.
//
//   1. GridBuildCPUMappingPlan()
//      Port the public cpu_mapping edge traversal order. YOTT attaches
//      reconvergence annotations after the plan is built.
//   2. GridMakeCPUMappingContext()
//      Keep placement, freedom grid, and mutable node degree together.
//   3. GridConstructCPUMappingPlacement(plan)
//      Walk the plan. Place missing endpoints using the selected YOTO/YOTT
//      step policy.
//   4. GridPlaceCPUMappingOrientedStep()
//      Dispatch one edge-local step to YOTO adjacency placement or YOTT
//      annotated-tip placement.
//   5. GridRunCPUMappingMultiStart()
//      Repeat plan generation and placement, then keep the lowest
//      cpu_mapping-style traversal edge cost.
//
// Ablation handles:
//   - traversal order: GridBuildCPUMappingPlan()
//   - neighbor choice: GridChooseCPUMappingZigZagNeighbor()
//   - YOTT cycle hints: GridApplyCPUMappingCycleAnnotations()
//   - local cells/tips: GridCPUMappingTipCells(), GridTryCPUMappingAdjacency()
//   - cell choice: GridBestCPUMappingDegreeCell()

CPUMappingPlacementContext YottEngine::GridMakeCPUMappingContext() const {
  CPUMappingPlacementContext context;
  context.state.dfg_to_cell.assign(dfg_.GetNodeNum(), -1);
  context.state.cell_to_dfg.assign(rows_ * cols_, -1);
  context.freedom = GridInitialFreedomGrid();
  context.mutable_degree = degree_;
  return context;
}

std::optional<CPUMappingTrialResult>
YottEngine::GridConstructCPUMappingPlacement(const std::vector<Step>& plan) {
  CPUMappingPlacementContext context = GridMakeCPUMappingContext();
  double traversal_edge_cost = 0.0;

  auto add_edge_cost = [&](const Step& step) {
    const int anchor_cell = context.state.dfg_to_cell[step.anchor];
    const int target_cell = context.state.dfg_to_cell[step.target];
    if (anchor_cell >= 0 && target_cell >= 0) {
      traversal_edge_cost += GridDistanceCost(anchor_cell, target_cell);
    } else {
      traversal_edge_cost += 1000.0;
    }
  };

  for (const auto& step : plan) {
    const bool anchor_placed = context.state.dfg_to_cell[step.anchor] >= 0;
    const bool target_placed = context.state.dfg_to_cell[step.target] >= 0;
    if (!anchor_placed && !target_placed) {
      if (!GridPlaceCPUMappingInitialNode(step.anchor, context)) {
        return std::nullopt;
      }
      GridPlaceCPUMappingOrientedStep(step, context);
    } else if (anchor_placed && !target_placed) {
      GridPlaceCPUMappingOrientedStep(step, context);
    } else if (!anchor_placed && target_placed) {
      Step reversed = step;
      reversed.anchor = step.target;
      reversed.target = step.anchor;
      GridPlaceCPUMappingOrientedStep(reversed, context);
    }
    add_edge_cost(step);
  }

  for (int node = 0; node < dfg_.GetNodeNum(); node++) {
    if (!GridPlaceCPUMappingInitialNode(node, context)) {
      return std::nullopt;
    }
  }
  CPUMappingTrialResult result;
  result.state = context.state;
  result.traversal_edge_cost = traversal_edge_cost;
  result.final_placement_cost = GridPlacementCost(context.state);
  return result;
}

std::optional<CPUMappingTrialResult>
YottEngine::GridConstructCPUMappingPlacement() {
  return GridConstructCPUMappingPlacement(GridBuildCPUMappingPlan());
}

std::optional<PlacementState> YottEngine::GridRunCPUMappingMultiStart(
    std::chrono::steady_clock::time_point start) {
  std::optional<PlacementState> best;
  double best_traversal_edge_cost = std::numeric_limits<double>::infinity();
  double best_final_placement_cost = std::numeric_limits<double>::infinity();
  int trials = 0;
  std::ofstream trace;
  if (GridTraceTrials()) {
    trace.open(GridTrialTracePath());
    if (trace) GridWriteTrialTraceHeader(trace);
  }

  const auto evaluate_trial = [&](int seed, int trial,
                                  const std::vector<Step>& plan) {
    auto trial_result = GridConstructCPUMappingPlacement(plan);
    trials++;
    bool better = false;
    std::optional<PlacementState> trace_state;
    double traversal_edge_cost = kGridImpossibleCost;
    double final_placement_cost = kGridImpossibleCost;
    if (trial_result.has_value()) {
      trace_state = trial_result->state;
      traversal_edge_cost = trial_result->traversal_edge_cost;
      final_placement_cost = trial_result->final_placement_cost;
      better = traversal_edge_cost < best_traversal_edge_cost ||
               (traversal_edge_cost == best_traversal_edge_cost &&
                final_placement_cost < best_final_placement_cost);
    }
    if (trace) {
      GridWriteTrialTraceRow(
          trace, "cpu_mapping", seed, trial, trial_result.has_value(), better,
          traversal_edge_cost, final_placement_cost, trace_state,
          SecondsSince(start), GridPlanHash(plan));
    }
    if (!trial_result.has_value() || !better) return;
    best = trial_result->state;
    best_traversal_edge_cost = trial_result->traversal_edge_cost;
    best_final_placement_cost = trial_result->final_placement_cost;
  };

  for (int seed = 0; seed < SeedCount() && !HasTimedOut(start, 0.05); seed++) {
    ResetSeed(seed);
    if (GridUsesPerTrialSeeds()) {
      for (int trial = 0; trial < MaxTrials() && !HasTimedOut(start, 0.05);
           trial++) {
        GridResetTrialSeed(seed, trial);
        const std::vector<Step> plan = GridBuildCPUMappingPlan();
        evaluate_trial(seed, trial, plan);
      }
      continue;
    }

    std::vector<std::vector<Step>> plans;
    plans.reserve(MaxTrials());
    for (int trial = 0; trial < MaxTrials() && !HasTimedOut(start, 0.05);
         trial++) {
      plans.push_back(GridBuildCPUMappingPlan());
    }
    for (int trial = 0; trial < static_cast<int>(plans.size()); trial++) {
      if (HasTimedOut(start, 0.05)) break;
      evaluate_trial(seed, trial, plans[trial]);
    }
  }
  Log("cpu_mapping_traversal trials=" + std::to_string(trials) +
      " best_traversal_edge_cost=" + std::to_string(best_traversal_edge_cost) +
      " best_final_placement_cost=" +
      std::to_string(best_final_placement_cost));
  return best;
}

// Candidate cells and local placement primitives.

std::vector<int> YottEngine::GridCPUMappingCompatibleCells(
    int dfg_node, const PlacementState& state) const {
  std::vector<int> result;
  for (int cell : compatible_cells_[dfg_node]) {
    if (!GridCanPlaceCPUMapping(dfg_node, cell, state)) continue;
    result.push_back(cell);
  }
  return result;
}

std::vector<std::pair<int, int>>
YottEngine::GridCPUMappingAllCompatibleCellPairs(
    int dfg_node, const CPUMappingPlacementContext& context) const {
  std::vector<std::pair<int, int>> result;
  for (int cell : GridCPUMappingCompatibleCells(dfg_node, context.state)) {
    result.push_back({context.freedom[cell], cell});
  }
  return result;
}

std::vector<int> YottEngine::GridCPUMappingIOCellsForInitialPlacement(
    int dfg_node, const PlacementState& state) const {
  std::vector<int> result;
  const int wanted_type = GridIsIONode(dfg_node) ? 1 : 0;
  for (int cell = 0; cell < rows_ * cols_; cell++) {
    if (GridCPUMappingCellType(cell) != wanted_type) continue;
    if (!GridCanPlaceCPUMapping(dfg_node, cell, state)) continue;
    result.push_back(cell);
  }
  return result;
}

int YottEngine::GridChooseCPUMappingInitialCell(int dfg_node,
                                                const PlacementState& state) {
  std::vector<int> candidates =
      GridCPUMappingIOCellsForInitialPlacement(dfg_node, state);
  if (candidates.empty()) {
    candidates = GridCPUMappingCompatibleCells(dfg_node, state);
  }
  if (candidates.empty()) return -1;
  GridRecordPlacementSwapAttempts(static_cast<long long>(candidates.size()));
  return candidates[GridAuthorRandomInt(static_cast<int>(candidates.size()))];
}

std::vector<std::pair<int, int>> YottEngine::GridCPUMappingTipCells(
    int anchor_cell, int dfg_node,
    const CPUMappingPlacementContext& context) const {
  static constexpr std::array<std::pair<int, int>, 8> kOneHopTips = {
      std::pair<int, int>{0, 1},
      {0, 2},
      {1, 0},
      {2, 0},
      {0, -1},
      {0, -2},
      {-1, 0},
      {-2, 0}};
  static constexpr std::array<std::pair<int, int>, 4> kMeshTips = {
      std::pair<int, int>{0, 1}, {1, 0}, {0, -1}, {-1, 0}};

  std::vector<std::pair<int, int>> result;
  const bool one_hop =
      config_.network_type == entity::MRRGNetworkType::kOneHop2d;
  const int anchor_row = GridRow(anchor_cell);
  const int anchor_col = GridCol(anchor_cell);
  const auto push_if_valid = [&](int dr, int dc) {
    const int row = anchor_row + dr;
    const int col = anchor_col + dc;
    // The public get_tips() uses strict lower bounds and non-strict upper
    // grid bounds; this keeps its top/left-border behavior.
    if (row <= 0 || col <= 0 || row >= rows_ || col >= cols_) return;
    const int cell = GridCell(row, col);
    if (cell < 0 || cell >= static_cast<int>(context.freedom.size())) return;
    if (context.freedom[cell] <= 0) return;
    if (!GridCanPlaceCPUMapping(dfg_node, cell, context.state)) return;
    result.push_back({context.freedom[cell], cell});
  };
  if (one_hop) {
    for (const auto& [dr, dc] : kOneHopTips) push_if_valid(dr, dc);
  } else {
    for (const auto& [dr, dc] : kMeshTips) push_if_valid(dr, dc);
  }
  return result;
}

std::vector<std::pair<int, int>>
YottEngine::GridCPUMappingCellsWithinAnnotatedDistance(
    int anchor_cell, int distance,
    const CPUMappingPlacementContext& context) const {
  std::vector<std::pair<int, int>> result;
  if (anchor_cell < 0 || distance < 0) return result;
  int search_distance = distance;
  const bool one_hop =
      config_.network_type == entity::MRRGNetworkType::kOneHop2d;
  if (one_hop) search_distance *= 2;
  if (search_distance == 0) search_distance = 1;
  const int anchor_row = GridRow(anchor_cell);
  const int anchor_col = GridCol(anchor_cell);
  for (int dr = -search_distance; dr <= search_distance; dr++) {
    for (int dc = -search_distance; dc <= search_distance; dc++) {
      const int row = anchor_row + dr;
      const int col = anchor_col + dc;
      if (row < 0 || col < 0 || row >= rows_ || col >= cols_) continue;
      const int cell = GridCell(row, col);
      if (cell < 0 || cell >= static_cast<int>(context.freedom.size())) {
        continue;
      }
      if (context.freedom[cell] <= 0) continue;
      int diff = 0;
      if (one_hop) {
        diff = (std::abs(dr) + 1) / 2 + (std::abs(dc) + 1) / 2 - 1;
      } else {
        diff = std::abs(dr) + std::abs(dc) - 1;
      }
      if (diff >= 0 && diff < search_distance) {
        result.push_back({context.freedom[cell], cell});
      }
    }
  }
  return result;
}

void YottEngine::GridSortCPUMappingCellPairs(
    std::vector<std::pair<int, int>>& cells) {
  std::sort(cells.begin(), cells.end());
  cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
}

std::vector<std::pair<int, int>> YottEngine::GridIntersectCPUMappingCellPairs(
    std::vector<std::pair<int, int>> lhs,
    std::vector<std::pair<int, int>> rhs) {
  GridSortCPUMappingCellPairs(lhs);
  GridSortCPUMappingCellPairs(rhs);
  std::vector<std::pair<int, int>> result;
  std::set_intersection(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(),
                        std::back_inserter(result));
  return result;
}

int YottEngine::GridBestCPUMappingDegreeCell(
    const std::vector<std::pair<int, int>>& cells, int dfg_node,
    const CPUMappingPlacementContext& context) {
  if (cells.empty()) return -1;
  GridRecordPlacementSwapAttempts(static_cast<long long>(cells.size()));
  std::vector<std::pair<int, int>> valid_cells;
  valid_cells.reserve(cells.size());
  for (const auto& cell : cells) {
    if (GridCanPlaceCPUMapping(dfg_node, cell.second, context.state)) {
      valid_cells.push_back(cell);
    }
  }
  if (valid_cells.empty()) return -1;

  if (candidate_rank_policy_ == "random") {
    return valid_cells[GridAuthorRandomInt(
                           static_cast<int>(valid_cells.size()))]
        .second;
  }
  if (candidate_rank_policy_ == "random_mt19937") {
    // Match YOTT Core's candidate stream in paired mechanism experiments.
    std::uniform_int_distribution<int> dist(
        0, static_cast<int>(valid_cells.size()) - 1);
    return valid_cells[dist(rng_)].second;
  }
  if (candidate_rank_policy_ == "high_freedom") {
    return std::max_element(valid_cells.begin(), valid_cells.end(),
                            [](const auto& a, const auto& b) {
                              if (a.first != b.first) return a.first < b.first;
                              return a.second > b.second;
                            })
        ->second;
  }
  if (candidate_rank_policy_ == "low_freedom") {
    return std::min_element(valid_cells.begin(), valid_cells.end(),
                            [](const auto& a, const auto& b) {
                              if (a.first != b.first) return a.first < b.first;
                              return a.second < b.second;
                            })
        ->second;
  }

  int best_degree = valid_cells.front().first;
  int best_cell = valid_cells.front().second;
  const int node_degree = context.mutable_degree[dfg_node];
  for (int i = 1; i < static_cast<int>(valid_cells.size()); i++) {
    const int available = valid_cells[i].first;
    const int cell = valid_cells[i].second;
    if (node_degree > 3) {
      // Degree matching compares candidate freedom. The released code compared
      // the cell index here, which makes the choice depend on numbering.
      if (available > best_degree) {
        best_degree = available;
        best_cell = cell;
      }
    } else {
      if (best_degree > available && node_degree >= available) {
        best_degree = available;
        best_cell = cell;
      }
    }
  }
  return best_cell;
}

void YottEngine::GridPlaceCPUMappingNode(
    int dfg_node, int cell, CPUMappingPlacementContext& context) const {
  GridPlaceNode(dfg_node, cell, context.state);
  if (dfg_node >= 0 &&
      dfg_node < static_cast<int>(context.mutable_degree.size())) {
    context.mutable_degree[dfg_node]--;
  }
  GridUpdateCPUMappingFreedomGrid(cell, context.freedom);
}

bool YottEngine::GridPlaceCPUMappingInitialNode(
    int dfg_node, CPUMappingPlacementContext& context) {
  if (context.state.dfg_to_cell[dfg_node] >= 0) return true;
  const int cell = GridChooseCPUMappingInitialCell(dfg_node, context.state);
  if (cell < 0) {
    GridRecordFailure(
        "initial placement has no cell for node=" + GridNodeLabel(dfg_node) +
        " io=" + std::to_string(GridIsIONode(dfg_node) ? 1 : 0) +
        " free_compatible=" +
        std::to_string(
            GridFreeCPUMappingCompatibleCellCount(dfg_node, context.state)) +
        " placed=" + std::to_string(GridPlacedCount(context.state)) + "/" +
        std::to_string(dfg_.GetNodeNum()));
    return false;
  }
  GridPlaceCPUMappingNode(dfg_node, cell, context);
  return true;
}

std::vector<std::pair<int, int>> YottEngine::GridCPUMappingAdjacencyOffsets() {
  std::vector<std::pair<int, int>> offsets;
  offsets.reserve(480);
  for (int distance = 1; distance <= 15; distance++) {
    offsets.push_back({0, distance});
    offsets.push_back({distance, 0});
    offsets.push_back({0, -distance});
    offsets.push_back({-distance, 0});
    for (int i = 1; i < distance; i++) offsets.push_back({i, distance - i});
    for (int i = distance - 1; i >= 1; i--) {
      offsets.push_back({i, -(distance - i)});
    }
    for (int i = 1; i < distance; i++) offsets.push_back({-i, -(distance - i)});
    for (int i = distance - 1; i >= 1; i--) {
      offsets.push_back({-i, distance - i});
    }
  }
  return offsets;
}

bool YottEngine::GridTryCPUMappingAdjacency(int dfg_node, int anchor_cell,
                                            CPUMappingPlacementContext& context,
                                            bool randomize_first) {
  if (anchor_cell < 0) {
    return GridPlaceCPUMappingInitialNode(dfg_node, context);
  }
  static const std::vector<std::pair<int, int>> kOffsets =
      GridCPUMappingAdjacencyOffsets();
  std::array<std::pair<int, int>, 4> first = {
      std::pair<int, int>{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
  if (randomize_first) GridShuffleWithAuthorRandom(first);

  const int anchor_row = GridRow(anchor_cell);
  const int anchor_col = GridCol(anchor_cell);
  for (int i = 0; i < static_cast<int>(kOffsets.size()); i++) {
    const auto [dr, dc] = i < 4 ? first[i] : kOffsets[i];
    const int row = anchor_row + dr;
    const int col = anchor_col + dc;
    if (row < 0 || col < 0 || row >= rows_ || col >= cols_) continue;
    const int cell = GridCell(row, col);
    cell_visits_++;
    GridRecordPlacementSwapAttempts();
    if (!GridCanPlaceCPUMapping(dfg_node, cell, context.state)) continue;
    GridPlaceCPUMappingNode(dfg_node, cell, context);
    return true;
  }
  GridRecordFailure(
      "adjacency placement has no cell for node=" + GridNodeLabel(dfg_node) +
      " anchor_cell=" + std::to_string(anchor_cell) + " anchor=(" +
      std::to_string(anchor_row) + "," + std::to_string(anchor_col) + ")" +
      " free_compatible=" +
      std::to_string(
          GridFreeCPUMappingCompatibleCellCount(dfg_node, context.state)) +
      " placed=" + std::to_string(GridPlacedCount(context.state)) + "/" +
      std::to_string(dfg_.GetNodeNum()));
  return false;
}

bool YottEngine::GridDeferCPUMappingIfNearPlacementFails(bool placed) {
  if (placed) return true;
  // The original cpu_mapping implementation does not abort a trial when an
  // edge-local placement attempt fails. It assigns a large edge cost and keeps
  // traversing; unplaced nodes can still be reached by later edges. We keep
  // that behavior and fill any remaining nodes after the traversal.
  return true;
}

bool YottEngine::GridPlaceCPUMappingNearNodeYOTT(
    const Step& step, CPUMappingPlacementContext& context) {
  const int dfg_node = step.target;
  if (context.state.dfg_to_cell[dfg_node] >= 0) return true;
  const int anchor_cell = context.state.dfg_to_cell[step.anchor];
  if (anchor_cell < 0) {
    return GridPlaceCPUMappingInitialNode(dfg_node, context);
  }
  if (candidate_scope_policy_ == "all") {
    const auto candidates =
        GridCPUMappingAllCompatibleCellPairs(dfg_node, context);
    const int cell =
        GridBestCPUMappingDegreeCell(candidates, dfg_node, context);
    if (cell >= 0) {
      GridPlaceCPUMappingNode(dfg_node, cell, context);
      return true;
    }
    return true;
  }
  if (candidate_scope_policy_ == "adjacent") {
    return GridTryCPUMappingAdjacency(dfg_node, anchor_cell, context, true);
  }

  std::vector<std::pair<int, int>> tips =
      GridCPUMappingTipCells(anchor_cell, dfg_node, context);
  std::vector<std::pair<int, int>> intersection = tips;
  std::vector<int> annotated_nodes;
  const bool use_intersection = candidate_scope_policy_ != "tips" &&
                                candidate_scope_policy_ != "tips_no_fallback";
  if (use_intersection) {
    for (const auto& annotation : step.annotations) {
      if (annotation.kind != StepAnnotationKind::kReconvergence) continue;
      if (annotation.distance > 2) continue;
      const int annotated_cell =
          context.state.dfg_to_cell[annotation.anchor_node];
      if (annotated_cell < 0) continue;
      annotated_nodes.push_back(annotation.anchor_node);
      auto cells = GridCPUMappingCellsWithinAnnotatedDistance(
          annotated_cell, annotation.distance, context);
      intersection = GridIntersectCPUMappingCellPairs(intersection, cells);
      if (intersection.empty()) break;
    }
  }

  int cell = GridBestCPUMappingDegreeCell(intersection, dfg_node, context);
  if (cell >= 0) {
    GridPlaceCPUMappingNode(dfg_node, cell, context);
    return true;
  }

  if (!tips.empty()) {
    std::vector<std::pair<int, int>> candidates = tips;
    if (!annotated_nodes.empty()) {
      int best_cost = std::numeric_limits<int>::max();
      std::vector<std::pair<int, int>> best_candidates;
      for (const auto& [available, candidate] : tips) {
        int cost = 0;
        for (int annotated_node : annotated_nodes) {
          const int annotated_cell = context.state.dfg_to_cell[annotated_node];
          if (annotated_cell >= 0)
            cost += GridDistanceCost(annotated_cell, candidate);
        }
        if (cost < best_cost) {
          best_cost = cost;
          best_candidates.clear();
          best_candidates.push_back({available, candidate});
        } else if (cost == best_cost) {
          best_candidates.push_back({available, candidate});
        }
      }
      if (!best_candidates.empty()) candidates = best_candidates;
    }
    cell = GridBestCPUMappingDegreeCell(candidates, dfg_node, context);
    if (cell >= 0) {
      GridPlaceCPUMappingNode(dfg_node, cell, context);
      return true;
    }
  }

  if (candidate_scope_policy_ == "annotation_intersection_only" ||
      candidate_scope_policy_ == "tips_no_fallback") {
    return true;
  }

  return GridTryCPUMappingAdjacency(dfg_node, anchor_cell, context, true);
}

// One plan step, shared context, and multi-start trial loop.

bool YottEngine::GridPlaceCPUMappingOrientedStep(
    const Step& step, CPUMappingPlacementContext& context) {
  {}
  return GridDeferCPUMappingIfNearPlacementFails(
      GridPlaceCPUMappingNearNodeYOTT(step, context));
}

void YottEngine::GridUpdateCPUMappingFreedomGrid(
    int placed_cell, std::vector<int>& freedom) const {
  if (placed_cell < 0 || placed_cell >= static_cast<int>(freedom.size())) {
    return;
  }
  freedom[placed_cell] = 0;
  const bool one_hop =
      config_.network_type == entity::MRRGNetworkType::kOneHop2d;
  static constexpr std::array<std::pair<int, int>, 8> kOneHopTips = {
      std::pair<int, int>{0, 1},
      {0, 2},
      {1, 0},
      {2, 0},
      {0, -1},
      {0, -2},
      {-1, 0},
      {-2, 0}};
  static constexpr std::array<std::pair<int, int>, 4> kMeshTips = {
      std::pair<int, int>{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
  const auto update = [&](int dr, int dc) {
    const int row = GridRow(placed_cell) + dr;
    const int col = GridCol(placed_cell) + dc;
    if (row <= 0 || col <= 0 || row >= rows_ || col >= cols_) return;
    const int cell = GridCell(row, col);
    if (freedom[cell] > 1) freedom[cell]--;
  };
  if (one_hop) {
    for (const auto& [dr, dc] : kOneHopTips) update(dr, dc);
  } else {
    for (const auto& [dr, dc] : kMeshTips) update(dr, dc);
  }
}

// Traversal plan and YOTT annotation construction.

int YottEngine::GridFindUnusedEdgeIndex(
    int source, int target, const std::vector<char>& used_edges) const {
  if (source < 0 || source >= static_cast<int>(incident_edge_ids_.size())) {
    return -1;
  }
  // Incident IDs retain DFG edge order, so this matches the first-unused
  // global scan without visiting unrelated edges.
  for (int edge_id : incident_edge_ids_[source]) {
    if (used_edges[edge_id]) continue;
    if (edges_[edge_id].source == source && edges_[edge_id].target == target) {
      return edge_id;
    }
  }
  return -1;
}

int YottEngine::GridChooseCPUMappingZigZagNeighbor(
    const std::vector<int>& candidates, bool choose_from_fanout, int mode,
    bool zigzag_take_back, const std::vector<int>& critical_path,
    const std::vector<double>& betweenness) {
  if (candidates.empty()) return -1;
  if (candidates.size() == 1) return candidates.front();

  if (mode == 0) {
    int best = candidates.front();
    for (int i = 1; i < static_cast<int>(candidates.size()); i++) {
      const int candidate = candidates[i];
      const int candidate_degree =
          choose_from_fanout
              ? static_cast<int>(successors_[candidate].size())
              : static_cast<int>(predecessors_[candidate].size());
      const int best_degree =
          choose_from_fanout ? static_cast<int>(successors_[best].size())
                             : static_cast<int>(predecessors_[best].size());
      if (candidate_degree > best_degree) best = candidate;
    }
    return best;
  }

  if (mode == 1) {
    int best = candidates.front();
    for (int i = 1; i < static_cast<int>(candidates.size()); i++) {
      const int candidate = candidates[i];
      if (betweenness[candidate] > betweenness[best]) best = candidate;
    }
    return best;
  }

  if (mode == 2) {
    int best = candidates.front();
    for (int i = 1; i < static_cast<int>(candidates.size()); i++) {
      const int candidate = candidates[i];
      if (critical_path[candidate] > critical_path[best]) best = candidate;
    }
    return best;
  }

  if (mode == 3) {
    return zigzag_take_back ? candidates.back() : candidates.front();
  }

  return candidates[GridAuthorRandomInt(static_cast<int>(candidates.size()))];
}

void YottEngine::GridAddCPUMappingCycleAnnotation(std::vector<Step>& plan,
                                                  int step_index,
                                                  int anchor_node,
                                                  int distance) const {
  if (step_index < 0 || step_index >= static_cast<int>(plan.size())) return;
  plan[step_index].annotations.push_back(StepAnnotation{
      StepAnnotationKind::kReconvergence, anchor_node, distance});
}

void YottEngine::GridApplyCPUMappingCycleAnnotations(
    std::vector<Step>& plan,
    const std::vector<std::pair<int, int>>& cycle_edges) const {
  for (const auto& [cycle_begin, cycle_end] : cycle_edges) {
    bool found_start = false;
    int count = 0;
    int value1 = -1;
    std::vector<int> walk_indices;
    for (int j = static_cast<int>(plan.size()) - 1; j >= 0; j--) {
      if (cycle_begin == plan[j].target && !found_start) {
        value1 = plan[j].anchor;
        GridAddCPUMappingCycleAnnotation(plan, j, cycle_end, count);
        count += 1;
        found_start = true;
      } else if (found_start &&
                 (value1 == plan[j].target || cycle_end == plan[j].anchor)) {
        value1 = plan[j].anchor;
        const int value2 = plan[j].target;
        if (value1 != cycle_end && value2 != cycle_end) {
          walk_indices.insert(walk_indices.begin(), j);
          GridAddCPUMappingCycleAnnotation(plan, j, cycle_end, count);
          count += 1;
        } else {
          for (int k = 0;
               k < count / 2 && k < static_cast<int>(walk_indices.size());
               k++) {
            Step& step = plan[walk_indices[k]];
            for (auto& annotation : step.annotations) {
              if (annotation.kind == StepAnnotationKind::kReconvergence &&
                  annotation.anchor_node == cycle_end &&
                  annotation.distance != k + 1) {
                annotation.distance = k + 1;
              }
            }
          }
          break;
        }
      }
    }
  }
}

std::vector<Step> YottEngine::GridBuildCPUMappingPlan() {
  std::vector<Step> plan;
  std::vector<char> used_edges(edges_.size(), false);
  std::vector<std::pair<int, int>> cycle_edges;
  std::vector<char> visited_nodes(dfg_.GetNodeNum(), false);
  std::vector<std::vector<int>> local_fanin = predecessors_;
  std::vector<std::vector<int>> local_fanout = successors_;
  std::vector<std::pair<int, int>> stack;
  for (int root : GridCPUMappingTraversalRoots()) stack.push_back({root, 1});
  int selection_mode = GridTraversalNeighborMode(GridIsCPUMappingYOTT());
  std::vector<int> critical_path;
  std::vector<double> betweenness;
  if (selection_mode == 1) betweenness = GridBuildBetweennessCentralityScore();
  if (selection_mode == 2)
    critical_path = GridBuildCPUMappingInputDistanceScore();

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
    if (visited_nodes[to]) cycle_edges.push_back({from, to});
    int edge_id = GridFindUnusedEdgeIndex(from, to, used_edges);
    if (edge_id < 0) {
      edge_id = GridFindUnusedEdgeIndex(to, from, used_edges);
    }
    if (edge_id >= 0) used_edges[edge_id] = true;
    plan.push_back(Step{from, to, edge_id, {}});
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
        b = GridChooseCPUMappingZigZagNeighbor(local_fanout[a], true,
                                               selection_mode, true,
                                               critical_path, betweenness);
        for (int i = 0; i < fanin; i++) stack.push_back({a, 1});
        stack.push_back({b, 0});
        remove_value(local_fanout[a], b);
        remove_value(local_fanin[b], a);
        append_step(a, b);
      } else if (fanin >= 1) {
        b = GridChooseCPUMappingZigZagNeighbor(local_fanin[a], false,
                                               selection_mode, true,
                                               critical_path, betweenness);
        stack.push_back({a, 1});
        for (int i = 0; i < fanin; i++) stack.push_back({b, 1});
        remove_value(local_fanin[a], b);
        remove_value(local_fanout[b], a);
        append_step(a, b);
      }
    } else {
      if (fanin >= 1) {
        b = GridChooseCPUMappingZigZagNeighbor(local_fanin[a], false,
                                               selection_mode, false,
                                               critical_path, betweenness);
        for (int i = 0; i < fanout; i++) stack.push_back({a, 0});
        stack.push_back({b, 1});
        remove_value(local_fanin[a], b);
        remove_value(local_fanout[b], a);
        append_step(a, b);
      } else if (fanout >= 1) {
        b = GridChooseCPUMappingZigZagNeighbor(local_fanout[a], true,
                                               selection_mode, false,
                                               critical_path, betweenness);
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

  std::vector<int> remaining;
  for (int edge_id = 0; edge_id < static_cast<int>(edges_.size()); edge_id++) {
    if (!used_edges[edge_id]) remaining.push_back(edge_id);
  }
  GridShuffleWithAuthorRandom(remaining);
  for (int edge_id : remaining) {
    plan.push_back(
        Step{edges_[edge_id].source, edges_[edge_id].target, edge_id, {}});
  }
  if (GridUsesYOTTAnnotations())
    GridApplyCPUMappingCycleAnnotations(plan, cycle_edges);
  return plan;
}

// Paper-guided YOTO/YOTT pipeline.
//
//   1. GridBuildPaperGuidedTraversalPlan()
//      Build an edge-local traversal plan. YOTT additionally records soft
//      annotations for I/O and reconvergent edges.
//   2. GridConstructPaperGuidedTraversalPlacement()
//      Walk the plan. Place each target near its anchor.
//   3. GridPlacePaperGuidedStep()
//      Generate closest legal cells, rank them, and place one node.
//   4. GridRunPaperGuidedTraversalMultiStart()
//      Repeat trials/seeds and keep the lowest GridPlacementCost().
//
// Ablation handles:
//   - traversal roots/order: GridRawTraversalRoots(),
//   GridBuildPaperGuidedTraversalPlan()
//   - neighbor choice: GridChoosePaperGuidedNeighbor()
//   - YOTT annotations: GridBackpropagatePaperGuidedAnnotation()
//   - candidate ranking: GridPaperGuidedTraversalRank()

// Paper-guided initial placement, edge order, and YOTT annotations.

double YottEngine::GridPaperGuidedIOPlacementScore(int dfg_node,
                                                   int cell) const {
  double score = GridBorderDistance(cell);
  if (GridIsIONode(dfg_node) || GridIsOutputNode(dfg_node)) {
    score += cell_memory_accessible_[cell] ? -8.0 : 40.0;
  } else {
    score += cell_memory_accessible_[cell] ? -1.0 : 0.0;
  }
  return score;
}

double YottEngine::GridPaperGuidedInitialPlacementScore(int dfg_node,
                                                        int cell) const {
  if (GridIsIONode(dfg_node)) {
    double score = GridPaperGuidedIOPlacementScore(dfg_node, cell);
    if (GridIsCornerCell(cell)) score += 4.0;
    return score;
  }
  double score = 0.0;
  if (GridBorderDistance(cell) == 0) score += 50.0;
  if (separate_io_cells_ && cell_memory_accessible_[cell]) score += 100.0;
  return score;
}

void YottEngine::GridBackpropagatePaperGuidedAnnotation(
    std::vector<Step>& plan, int from_node, int anchor_node,
    StepAnnotationKind kind, int max_depth, int initial_distance) const {
  int current = from_node;
  int distance = initial_distance;
  while (distance <= max_depth) {
    const int step_index = GridFindLastStepTargeting(plan, current);
    if (step_index < 0) return;
    plan[step_index].annotations.push_back(
        StepAnnotation{kind, anchor_node, distance});
    current = plan[step_index].anchor;
    distance++;
  }
}

int YottEngine::GridChoosePaperGuidedNeighbor(
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

std::vector<Step> YottEngine::GridBuildPaperGuidedTraversalPlan(bool annotate) {
  const int kMaxAnnotationDistance = 2;
  std::vector<Step> plan;
  std::vector<char> visited_nodes(dfg_.GetNodeNum(), false);
  std::vector<std::vector<int>> local_fanin = predecessors_;
  std::vector<std::vector<int>> local_fanout = successors_;
  std::vector<std::pair<int, int>> stack;
  for (int root : GridRawTraversalRoots()) stack.push_back({root, 1});
  const auto critical_path = BuildCriticalPathScore();
  const auto betweenness = GridBuildBetweennessCentralityScore();
  int selection_mode = GridTraversalNeighborMode(GridIsPaperGuidedYOTT());

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
    Step step{from, to, GridFindEdgeId(from, to), {}};
    if (annotate && GridIsIONode(to)) {
      step.annotations.push_back(
          StepAnnotation{StepAnnotationKind::kIO, to, 1});
      GridBackpropagatePaperGuidedAnnotation(
          plan, from, to, StepAnnotationKind::kIO, kMaxAnnotationDistance, 2);
    }
    if (annotate && visited_nodes[to]) {
      GridBackpropagatePaperGuidedAnnotation(plan, from, to,
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
        b = GridChoosePaperGuidedNeighbor(local_fanout[a], true, selection_mode,
                                          true, critical_path, betweenness);
        for (int i = 0; i < fanin; i++) stack.push_back({a, 1});
        stack.push_back({b, 0});
        remove_value(local_fanout[a], b);
        remove_value(local_fanin[b], a);
        append_step(a, b);
      } else if (fanin >= 1) {
        b = GridChoosePaperGuidedNeighbor(local_fanin[a], false, selection_mode,
                                          true, critical_path, betweenness);
        stack.push_back({a, 1});
        for (int i = 0; i < fanin; i++) stack.push_back({b, 1});
        remove_value(local_fanin[a], b);
        remove_value(local_fanout[b], a);
        append_step(a, b);
      }
    } else {
      if (fanin >= 1) {
        b = GridChoosePaperGuidedNeighbor(local_fanin[a], false, selection_mode,
                                          false, critical_path, betweenness);
        for (int i = 0; i < fanout; i++) stack.push_back({a, 0});
        stack.push_back({b, 1});
        remove_value(local_fanin[a], b);
        remove_value(local_fanout[b], a);
        append_step(a, b);
      } else if (fanout >= 1) {
        b = GridChoosePaperGuidedNeighbor(local_fanout[a], true, selection_mode,
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

int YottEngine::GridPaperGuidedCellFreedom(int cell,
                                           const PlacementState& state) const {
  int result = 0;
  for (int other = 0; other < rows_ * cols_; other++) {
    if (other == cell || cell_to_mrrg_[other] < 0) continue;
    if (state.cell_to_dfg[other] >= 0) continue;
    if (GridDistanceCost(cell, other) == 1) result++;
  }
  return result;
}

double YottEngine::GridPaperGuidedDegreeMatchScore(
    int dfg_node, int cell, const PlacementState& state) const {
  const int target_degree = degree_[dfg_node];
  const int freedom = GridPaperGuidedCellFreedom(cell, state);
  return mapper::yott::DegreePenalty(target_degree, freedom);
}

int YottEngine::GridFreeCellsAtDistance(int anchor_cell, int target_distance,
                                        const PlacementState& state) const {
  int result = 0;
  for (int cell = 0; cell < rows_ * cols_; cell++) {
    if (cell_to_mrrg_[cell] < 0 || state.cell_to_dfg[cell] >= 0) continue;
    if (GridDistanceCost(anchor_cell, cell) == target_distance) result++;
  }
  return result;
}

double YottEngine::GridPaperGuidedAnnotationScore(const Step& step,
                                                  int candidate_cell,
                                                  const PlacementState& state,
                                                  StepAnnotationKind kind,
                                                  bool lookahead_only) const {
  double best = kGridImpossibleCost;
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
      score += GridPaperGuidedIOPlacementScore(step.target, candidate_cell);
      if (anchor_cell >= 0) {
        const int distance = GridDistanceCost(candidate_cell, anchor_cell);
        score += 2.0 * std::abs(distance - annotation.distance);
      }
    } else if (anchor_cell >= 0) {
      const int distance = GridDistanceCost(candidate_cell, anchor_cell);
      score += 3.0 * std::abs(distance - annotation.distance);
      if (lookahead_only) {
        score -= 0.25 * GridFreeCellsAtDistance(anchor_cell,
                                                annotation.distance - 1, state);
      }
    } else {
      continue;
    }
    best = std::min(best, score);
  }
  return best;
}

bool YottEngine::GridHasPaperGuidedAnnotationKind(const Step& step,
                                                  StepAnnotationKind kind,
                                                  bool lookahead_only) const {
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

PaperGuidedRank YottEngine::GridPaperGuidedTraversalRank(
    const Step& step, int candidate_cell, const PlacementState& state,
    bool use_annotations, int random_order) const {
  const int anchor_cell = state.dfg_to_cell[step.anchor];
  PaperGuidedRank rank;
  rank.annotation_priority = 3;
  rank.annotation_cost = 0.0;
  rank.locality_distance = GridDistanceCost(anchor_cell, candidate_cell);
  rank.degree_cost = use_annotations ? GridPaperGuidedDegreeMatchScore(
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
        GridPaperGuidedDegreeMatchScore(step.target, candidate_cell, state);
    return rank;
  }

  if (use_annotations &&
      GridHasPaperGuidedAnnotationKind(step, StepAnnotationKind::kIO, false)) {
    rank.annotation_priority = 0;
    rank.annotation_cost = GridPaperGuidedAnnotationScore(
        step, candidate_cell, state, StepAnnotationKind::kIO, false);
    return rank;
  }
  if (use_annotations && GridHasPaperGuidedAnnotationKind(
                             step, StepAnnotationKind::kReconvergence, true)) {
    rank.annotation_priority = 1;
    rank.annotation_cost = GridPaperGuidedAnnotationScore(
        step, candidate_cell, state, StepAnnotationKind::kReconvergence, true);
    return rank;
  }
  if (use_annotations && GridHasPaperGuidedAnnotationKind(
                             step, StepAnnotationKind::kReconvergence, false)) {
    rank.annotation_priority = 2;
    rank.annotation_cost = GridPaperGuidedAnnotationScore(
        step, candidate_cell, state, StepAnnotationKind::kReconvergence, false);
    return rank;
  }
  return rank;
}

bool YottEngine::GridIsBetterPaperGuidedRank(const PaperGuidedRank& a,
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

std::vector<int> YottEngine::GridClosestCompatibleCells(
    int dfg_node, int anchor_cell, const PlacementState& state) const {
  if (candidate_scope_policy_ == "all") {
    return GridCompatibleCells(dfg_node, state);
  }
  if (anchor_cell < 0) return GridCompatibleCells(dfg_node, state);
  std::vector<int> result;
  int best_distance = std::numeric_limits<int>::max();
  for (int cell : compatible_cells_[dfg_node]) {
    if (!GridCanPlace(dfg_node, cell, state)) continue;
    const int distance = GridDistanceCost(anchor_cell, cell);
    if (distance > best_distance) continue;
    if (distance < best_distance) {
      result.clear();
      best_distance = distance;
    }
    result.push_back(cell);
  }
  return result;
}

int YottEngine::GridChoosePaperGuidedInitialCell(int dfg_node,
                                                 PlacementState& state) {
  std::vector<int> candidates = GridCompatibleCells(dfg_node, state);
  if (candidates.empty()) return -1;
  GridRecordPlacementSwapAttempts(static_cast<long long>(candidates.size()));
  std::shuffle(candidates.begin(), candidates.end(), rng_);
  std::sort(candidates.begin(), candidates.end(), [&](int a, int b) {
    const double sa = GridPaperGuidedInitialPlacementScore(dfg_node, a);
    const double sb = GridPaperGuidedInitialPlacementScore(dfg_node, b);
    if (sa != sb) return sa < sb;
    return a < b;
  });
  const double best_score =
      GridPaperGuidedInitialPlacementScore(dfg_node, candidates.front());
  std::vector<int> best_candidates;
  for (int cell : candidates) {
    if (GridPaperGuidedInitialPlacementScore(dfg_node, cell) <=
        best_score + 1.0e-9) {
      best_candidates.push_back(cell);
    }
  }
  std::uniform_int_distribution<int> dist(
      0, static_cast<int>(best_candidates.size()) - 1);
  return best_candidates[dist(rng_)];
}

bool YottEngine::GridPlacePaperGuidedInitialNode(int dfg_node,
                                                 PlacementState& state) {
  if (state.dfg_to_cell[dfg_node] >= 0) return true;
  const int cell = GridChoosePaperGuidedInitialCell(dfg_node, state);
  if (cell < 0) {
    GridRecordFailure(
        "initial placement has no cell for node=" + GridNodeLabel(dfg_node) +
        " io=" + std::to_string(GridIsIONode(dfg_node) ? 1 : 0) +
        " free_compatible=" +
        std::to_string(GridFreeCPUMappingCompatibleCellCount(dfg_node, state)) +
        " placed=" + std::to_string(GridPlacedCount(state)) + "/" +
        std::to_string(dfg_.GetNodeNum()));
    return false;
  }
  GridPlaceNode(dfg_node, cell, state);
  return true;
}

bool YottEngine::GridPlacePaperGuidedStep(const Step& step,
                                          PlacementState& state,
                                          bool use_annotations) {
  if (state.dfg_to_cell[step.target] >= 0) return true;
  if (state.dfg_to_cell[step.anchor] < 0) {
    if (!GridPlacePaperGuidedInitialNode(step.anchor, state)) return false;
  }
  std::vector<int> candidates = GridClosestCompatibleCells(
      step.target, state.dfg_to_cell[step.anchor], state);
  if (candidates.empty()) {
    // Edge-local placement can be over-constrained late in the traversal,
    // especially for large graphs with many I/O nodes. Keep the trial alive
    // and let a later edge or the final fill place this node.
    return true;
  }
  GridRecordPlacementSwapAttempts(static_cast<long long>(candidates.size()));
  if (candidate_rank_policy_ == "random") {
    std::shuffle(candidates.begin(), candidates.end(), rng_);
  } else {
    std::shuffle(candidates.begin(), candidates.end(), rng_);
  }

  std::vector<std::pair<PaperGuidedRank, int>> scored;
  scored.reserve(candidates.size());
  for (int order = 0; order < static_cast<int>(candidates.size()); order++) {
    const int cell = candidates[order];
    scored.push_back({GridPaperGuidedTraversalRank(step, cell, state,
                                                   use_annotations, order),
                      cell});
  }
  std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
    return GridIsBetterPaperGuidedRank(a.first, b.first);
  });
  GridPlaceNode(step.target, scored.front().second, state);
  return true;
}

// Trial construction and multi-start search.

std::optional<PlacementState>
YottEngine::GridConstructPaperGuidedTraversalPlacement() {
  const bool use_annotations = GridUsesYOTTAnnotations();
  const auto plan = GridBuildPaperGuidedTraversalPlan(use_annotations);
  PlacementState state;
  state.dfg_to_cell.assign(dfg_.GetNodeNum(), -1);
  state.cell_to_dfg.assign(rows_ * cols_, -1);
  for (const auto& step : plan) {
    if (!GridPlacePaperGuidedStep(step, state, use_annotations))
      return std::nullopt;
  }
  for (int node = 0; node < dfg_.GetNodeNum(); node++) {
    if (!GridPlacePaperGuidedInitialNode(node, state)) return std::nullopt;
  }
  return state;
}

std::optional<PlacementState> YottEngine::GridRunPaperGuidedTraversalMultiStart(
    std::chrono::steady_clock::time_point start) {
  std::optional<PlacementState> best;
  double best_cost = std::numeric_limits<double>::infinity();
  int trials = 0;
  for (int seed = 0; seed < SeedCount() && !HasTimedOut(start, 0.05); seed++) {
    ResetSeed(seed);
    for (int trial = 0; trial < MaxTrials() && !HasTimedOut(start, 0.05);
         trial++) {
      auto placement = GridConstructPaperGuidedTraversalPlacement();
      trials++;
      if (!placement.has_value()) continue;
      const double cost = GridPlacementCost(*placement);
      if (cost < best_cost) {
        best = placement;
        best_cost = cost;
      }
    }
  }
  Log("paper_guided_traversal trials=" + std::to_string(trials));
  return best;
}

double YottEngine::GridPlacementCost(const PlacementState& state) const {
  double cost = 0.0;
  for (const auto& edge : edges_) {
    const int from = state.dfg_to_cell[edge.source];
    const int to = state.dfg_to_cell[edge.target];
    if (from < 0 || to < 0) return kGridImpossibleCost;
    cost += GridDistanceCost(from, to);
  }
  return cost;
}

int YottEngine::GridBorderDistance(int cell) const {
  return std::min(std::min(GridRow(cell), rows_ - 1 - GridRow(cell)),
                  std::min(GridCol(cell), cols_ - 1 - GridCol(cell)));
}

std::vector<int> YottEngine::GridCompatibleCells(
    int dfg_node, const PlacementState& state) const {
  std::vector<int> cells;
  cells.reserve(compatible_cells_[dfg_node].size());
  for (int cell : compatible_cells_[dfg_node]) {
    if (GridCanPlace(dfg_node, cell, state)) cells.push_back(cell);
  }
  return cells;
}

void YottEngine::GridPlaceNode(int dfg_node, int cell,
                               PlacementState& state) const {
  state.dfg_to_cell[dfg_node] = cell;
  state.cell_to_dfg[cell] = dfg_node;
}

// Shared traversal graph utilities used by paper-guided and cpu_mapping
// YOTO/YOTT variants. Method-specific files decide how these scores are used.

bool YottEngine::GridIsOutputNode(int dfg_node) const {
  return dfg_.GetNodeProperty(dfg_node).op == entity::OpType::kOutput;
}

bool YottEngine::GridIsCornerCell(int cell) const {
  const int row = GridRow(cell);
  const int col = GridCol(cell);
  return (row == 0 || row == rows_ - 1) && (col == 0 || col == cols_ - 1);
}

std::vector<int> YottEngine::GridRawTraversalRoots() {
  std::vector<int> roots;
  for (int node = 0; node < dfg_.GetNodeNum(); node++) {
    if (successors_[node].empty() || GridIsOutputNode(node))
      roots.push_back(node);
  }
  if (roots.empty()) {
    roots.resize(dfg_.GetNodeNum());
    std::iota(roots.begin(), roots.end(), 0);
  }
  std::shuffle(roots.begin(), roots.end(), rng_);
  return roots;
}

std::vector<int> YottEngine::GridCPUMappingTraversalRoots() {
  std::vector<int> roots;
  for (int node = 0; node < dfg_.GetNodeNum(); node++) {
    if (successors_[node].empty() || GridIsOutputNode(node))
      roots.push_back(node);
  }
  if (roots.empty()) {
    roots.resize(dfg_.GetNodeNum());
    std::iota(roots.begin(), roots.end(), 0);
  }
  GridShuffleWithAuthorRandom(roots);
  return roots;
}

int YottEngine::GridFindEdgeId(int source, int target) const {
  for (int edge_id = 0; edge_id < static_cast<int>(edges_.size()); edge_id++) {
    const auto& edge = edges_[edge_id];
    if (edge.source == source && edge.target == target) return edge_id;
  }
  for (int edge_id = 0; edge_id < static_cast<int>(edges_.size()); edge_id++) {
    const auto& edge = edges_[edge_id];
    if (edge.source == target && edge.target == source) return edge_id;
  }
  return -1;
}

int YottEngine::GridFindLastStepTargeting(const std::vector<Step>& plan,
                                          int target_node) const {
  for (int i = static_cast<int>(plan.size()) - 1; i >= 0; i--) {
    if (plan[i].target == target_node) return i;
  }
  return -1;
}

std::vector<int> YottEngine::GridBuildCPUMappingInputDistanceScore() const {
  const int n = dfg_.GetNodeNum();
  std::vector<int> distance(n, -1);
  std::queue<int> q;
  for (int node = 0; node < n; node++) {
    if (!predecessors_[node].empty()) continue;
    distance[node] = 0;
    q.push(node);
  }
  if (q.empty()) {
    for (int node = 0; node < n; node++) {
      distance[node] = 0;
      q.push(node);
    }
  }
  while (!q.empty()) {
    const int node = q.front();
    q.pop();
    for (int successor : successors_[node]) {
      if (successor == node) continue;
      const int next_distance = distance[node] + 1;
      if (next_distance <= distance[successor]) continue;
      distance[successor] = next_distance;
      q.push(successor);
    }
  }
  for (int& value : distance) {
    if (value < 0) value = 0;
  }
  return distance;
}

std::vector<double> YottEngine::GridBuildBetweennessCentralityScore() const {
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
      const int node = q.front();
      q.pop();
      order.push_back(node);
      for (int successor : successors_[node]) {
        if (successor == node) continue;
        if (distance[successor] < 0) {
          distance[successor] = distance[node] + 1;
          q.push(successor);
        }
        if (distance[successor] == distance[node] + 1) {
          sigma[successor] += sigma[node];
          predecessors_on_paths[successor].push_back(node);
        }
      }
    }

    for (auto it = order.rbegin(); it != order.rend(); ++it) {
      const int node = *it;
      if (sigma[node] == 0.0) continue;
      for (int predecessor : predecessors_on_paths[node]) {
        dependency[predecessor] +=
            (sigma[predecessor] / sigma[node]) * (1.0 + dependency[node]);
      }
      if (node != source) centrality[node] += dependency[node];
    }
  }
  return centrality;
}

}  // namespace mapper::yott
