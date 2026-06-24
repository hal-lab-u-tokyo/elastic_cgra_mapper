#include "placement_search_engine_internal.hpp"

namespace mapper::detail::placement_search {

std::vector<int> PlacementSearchEngine::GetRawOutputTraversalRoots() {
  std::vector<int> roots;
  for (int v = 0; v < dfg_.GetNodeNum(); v++) {
    if (successors_[v].empty() ||
        dfg_.GetNodeProperty(v).op == entity::OpType::OUTPUT) {
      roots.push_back(v);
    }
  }
  if (roots.empty()) {
    for (int v = 0; v < dfg_.GetNodeNum(); v++) roots.push_back(v);
  }
  std::shuffle(roots.begin(), roots.end(), rng_);
  return roots;
}

std::vector<int> PlacementSearchEngine::GetOutputTraversalRoots() {
  std::vector<int> roots;
  for (int v = 0; v < dfg_.GetNodeNum(); v++) {
    if (successors_[v].empty() ||
        dfg_.GetNodeProperty(v).op == entity::OpType::OUTPUT) {
      roots.push_back(v);
    }
  }
  if (roots.empty()) {
    for (int v = 0; v < dfg_.GetNodeNum(); v++) roots.push_back(v);
  }
  std::shuffle(roots.begin(), roots.end(), rng_);
  std::stable_sort(roots.begin(), roots.end(), [&](int a, int b) {
    if (IsIONode(a) != IsIONode(b)) return IsIONode(a) > IsIONode(b);
    return annotation_.degree[a] > annotation_.degree[b];
  });
  return roots;
}

int PlacementSearchEngine::OtherEndpoint(const DFGEdgeInfo& edge, int node_id) const {
  if (edge.source == node_id) return edge.target;
  if (edge.target == node_id) return edge.source;
  return -1;
}

int PlacementSearchEngine::FindDFGEdgeId(int source, int target) const {
  for (const auto& edge : dfg_edges_) {
    if (edge.source == source && edge.target == target) return edge.id;
  }
  for (const auto& edge : dfg_edges_) {
    if (edge.source == target && edge.target == source) return edge.id;
  }
  return -1;
}

std::vector<int> PlacementSearchEngine::UnvisitedIncidentEdges(
    int dfg_node_id, const std::vector<char>& visited_edges) const {
  std::vector<int> result;
  for (int edge_id : incident_edge_ids_[dfg_node_id]) {
    if (!visited_edges[edge_id]) result.push_back(edge_id);
  }
  return result;
}

int PlacementSearchEngine::PickTraversalEdge(int dfg_node_id, const std::vector<char>& visited_nodes,
                      const std::vector<char>& visited_edges) {
  std::vector<int> candidates = UnvisitedIncidentEdges(dfg_node_id, visited_edges);
  if (candidates.empty()) return -1;
  std::shuffle(candidates.begin(), candidates.end(), rng_);
  std::stable_sort(candidates.begin(), candidates.end(), [&](int a, int b) {
    const int oa = OtherEndpoint(dfg_edges_[a], dfg_node_id);
    const int ob = OtherEndpoint(dfg_edges_[b], dfg_node_id);
    const bool a_unvisited = oa >= 0 && !visited_nodes[oa];
    const bool b_unvisited = ob >= 0 && !visited_nodes[ob];
    if (a_unvisited != b_unvisited) return a_unvisited > b_unvisited;
    const int degree_a = oa >= 0 ? annotation_.degree[oa] : -1;
    const int degree_b = ob >= 0 ? annotation_.degree[ob] : -1;
    return degree_a > degree_b;
  });
  return candidates.front();
}

int PlacementSearchEngine::FindLastStepTargeting(const TraversalPlan& plan, int target_node) const {
  for (int i = static_cast<int>(plan.sequence.size()) - 1; i >= 0; i--) {
    if (plan.sequence[i].target_node == target_node) return i;
  }
  return -1;
}

void PlacementSearchEngine::BackpropagateAnnotation(TraversalPlan& plan, int from_node,
                             int anchor_node,
                             SequenceAnnotationKind kind, int max_depth,
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

std::vector<int> PlacementSearchEngine::BuildCriticalPathScore() const {
  const int n = dfg_.GetNodeNum();
  std::vector<int> memo(n, -1);
  std::vector<char> visiting(n, false);
  std::function<int(int)> dfs = [&](int node) {
    if (memo[node] >= 0) return memo[node];
    if (visiting[node]) return 0;
    visiting[node] = true;
    int best = 0;
    for (int successor : successors_[node]) {
      if (successor == node) continue;
      best = std::max(best, 1 + dfs(successor));
    }
    visiting[node] = false;
    memo[node] = best;
    return best;
  };
  for (int node = 0; node < n; node++) dfs(node);
  return memo;
}

std::vector<double> PlacementSearchEngine::BuildBetweennessCentralityScore() const {
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
        dependency[v] +=
            (sigma[v] / sigma[w]) * (1.0 + dependency[w]);
      }
      if (w != source) centrality[w] += dependency[w];
    }
  }
  return centrality;
}

int PlacementSearchEngine::Placement2DCentralityScore(int dfg_node_id) const {
  return annotation_.degree[dfg_node_id] +
         annotation_.in_degree[dfg_node_id] * annotation_.out_degree[dfg_node_id];
}

int PlacementSearchEngine::ChoosePlacement2DNeighbor(const std::vector<int>& candidates,
                         bool choose_from_fanout, int mode,
                         bool zigzag_take_back,
                         const std::vector<int>& critical_path,
                         const std::vector<double>& betweenness) {
  if (candidates.empty()) return -1;
  if (candidates.size() == 1) return candidates.front();

  if (mode == 0) {
    return *std::max_element(candidates.begin(), candidates.end(),
                             [&](int a, int b) {
      const int da = choose_from_fanout
                         ? static_cast<int>(successors_[a].size())
                         : static_cast<int>(predecessors_[a].size());
      const int db = choose_from_fanout
                         ? static_cast<int>(successors_[b].size())
                         : static_cast<int>(predecessors_[b].size());
      if (da != db) return da < db;
      return annotation_.degree[a] < annotation_.degree[b];
    });
  }
  if (mode == 1) {
    return *std::max_element(candidates.begin(), candidates.end(),
                             [&](int a, int b) {
      if (betweenness[a] != betweenness[b]) {
        return betweenness[a] < betweenness[b];
      }
      return annotation_.degree[a] < annotation_.degree[b];
    });
  }
  if (mode == 2) {
    return *std::max_element(candidates.begin(), candidates.end(),
                             [&](int a, int b) {
      if (critical_path[a] != critical_path[b]) {
        return critical_path[a] < critical_path[b];
      }
      return annotation_.degree[a] < annotation_.degree[b];
    });
  }
  if (mode == 3) {
    return zigzag_take_back ? candidates.back() : candidates.front();
  }

  std::uniform_int_distribution<int> dist(
      0, static_cast<int>(candidates.size()) - 1);
  return candidates[dist(rng_)];
}

TraversalPlan PlacementSearchEngine::BuildPlacement2DTraversalPlan(bool annotate) {
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

TraversalPlan PlacementSearchEngine::BuildTraversalPlan(bool annotate) {
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
        pending_step.annotations.push_back(SequenceAnnotation{
            SequenceAnnotationKind::kIO, target_node, 1});
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

std::optional<int> PlacementSearchEngine::PlaceInitialNode(int dfg_node_id, PlacementState& state) {
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

double PlacementSearchEngine::DegreeMatchScore(int dfg_node_id, int mrrg_node_id,
                        const PlacementState& state) const {
  const int target_degree = annotation_.degree[dfg_node_id];
  const int freedom = ResourceFreedom(mrrg_node_id, state);
  if (freedom >= target_degree) return freedom - target_degree;
  return 4.0 * (target_degree - freedom);
}

int PlacementSearchEngine::FreeResourcesAtDistance(int anchor_mrrg_node, int target_distance,
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

double PlacementSearchEngine::AnnotationScore(const TraversalStep& step, int candidate_mrrg_node,
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
        const int distance = UsesPhysicalPlacementStage()
                                 ? PhysicalDistance(candidate_mrrg_node,
                                                    anchor_mrrg_node)
                                 : ResourceDistance(candidate_mrrg_node,
                                                    anchor_mrrg_node);
        score += 2.0 * std::abs(distance - annotation.distance);
      }
    } else if (anchor_mrrg_node >= 0) {
      const int distance = UsesPhysicalPlacementStage()
                               ? PhysicalDistance(candidate_mrrg_node,
                                                  anchor_mrrg_node)
                               : ResourceDistance(candidate_mrrg_node,
                                                  anchor_mrrg_node);
      score += 3.0 * std::abs(distance - annotation.distance);
      if (lookahead_only) {
        score -= 0.25 * FreeResourcesAtDistance(
                           anchor_mrrg_node, annotation.distance - 1, state);
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

CandidateRank PlacementSearchEngine::TraversalPlacementRank(const TraversalStep& step,
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
      use_annotations ? DegreeMatchScore(step.target_node,
                                         candidate_mrrg_node, state)
                      : 0.0;
  rank.random_order = random_order;

  if (use_annotations && HasAnnotationKind(step, SequenceAnnotationKind::kIO,
                                           false)) {
    rank.annotation_priority = 0;
    rank.annotation_cost = AnnotationScore(step, candidate_mrrg_node, state,
                                           SequenceAnnotationKind::kIO,
                                           false);
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
  if (use_annotations && HasAnnotationKind(
                             step, SequenceAnnotationKind::kReconvergence,
                             false)) {
    rank.annotation_priority = 2;
    rank.annotation_cost =
        AnnotationScore(step, candidate_mrrg_node, state,
                        SequenceAnnotationKind::kReconvergence, false);
    return rank;
  }

  return rank;
}

bool PlacementSearchEngine::PlaceTraversalStep(const TraversalStep& step, PlacementState& state,
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
        {TraversalPlacementRank(step, candidate, state, use_annotations,
                                order),
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
                return a.first.locality_distance <
                       b.first.locality_distance;
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

std::optional<PlacementState> PlacementSearchEngine::ConstructTraversalPlacement(
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

std::vector<int> PlacementSearchEngine::BuildASAPContextLevels() const {
  const int n = dfg_.GetNodeNum();
  std::vector<int> indegree(n, 0);
  std::vector<int> level(n, 0);
  for (const auto& edge : dfg_edges_) {
    if (edge.source == edge.target) continue;
    indegree[edge.target]++;
  }

  std::queue<int> ready;
  for (int node_id = 0; node_id < n; node_id++) {
    if (indegree[node_id] == 0) ready.push(node_id);
  }

  std::vector<char> visited(n, false);
  while (!ready.empty()) {
    const int node_id = ready.front();
    ready.pop();
    visited[node_id] = true;
    for (int successor : successors_[node_id]) {
      if (successor == node_id) continue;
      level[successor] = std::max(level[successor], level[node_id] + 1);
      indegree[successor]--;
      if (indegree[successor] == 0) ready.push(successor);
    }
  }

  // Most DFGs are DAGs, with recurrent behavior represented by self edges.
  // If a benchmark contains a non-self cycle, keep the assignment
  // deterministic rather than introducing another heuristic fallback.
  for (int node_id = 0; node_id < n; node_id++) {
    if (!visited[node_id]) level[node_id] = node_id;
  }
  return level;
}

std::optional<int> PlacementSearchEngine::FindSamePhysicalResourceAtContext(
    int base_resource, int dfg_node_id, int desired_context,
    const PlacementState& assigned) const {
  const int context_size =
      std::max(1, mrrg_.GetMRRGConfig().context_size);
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

std::optional<PlacementState> PlacementSearchEngine::AssignContextsToPhysicalPlacement(
    const PlacementState& physical_placement) const {
  if (!IsPhysicalThenContextLike()) return physical_placement;

  const int context_size =
      std::max(1, mrrg_.GetMRRGConfig().context_size);
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

std::optional<PlacementState> PlacementSearchEngine::FinalizePhysicalPlacementIfNeeded(
    const PlacementState& placement) const {
  if (!IsPhysicalThenContextLike()) return placement;
  return AssignContextsToPhysicalPlacement(placement);
}

void PlacementSearchEngine::PushPlacementCandidate(std::vector<PlacementState>& result,
                            const std::optional<PlacementState>& placement) {
  if (!placement.has_value()) return;
  auto finalized = FinalizePhysicalPlacementIfNeeded(*placement);
  if (finalized.has_value()) result.push_back(*finalized);
}

std::optional<PlacementState> PlacementSearchEngine::ConstructGreedyPlacement() {
  return ConstructTraversalPlacement(UsesPaperTraversalPlan(), IsYOTTLike());
}

std::vector<PlacementState> PlacementSearchEngine::ConstructGreedyPlacementCandidates() {
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
      IsPlacement2DTraversalLike() ||
      IsPhysicalThenContextLike();
  const int placement_variants_per_trial =
      UsesModuloFallbackCandidates() ? (IsYOTTLike() ? 4 : 2) : 1;
  const int elite_limit = route_all_placement_trials
                              ? std::max(8, max_trials * seed_count *
                                                placement_variants_per_trial)
                              : std::max(8, RoutingRetryCount() * 2);
  int total_trials = 0;

  for (int seed_index = 0;
       seed_index < seed_count && !HasTimedOut(start, 0.1);
       seed_index++) {
    ResetSeed(seed_index);
    for (int trial = 0;
         trial < max_trials && !HasTimedOut(start, 0.1);
         trial++, total_trials++) {
      auto placements = ConstructGreedyPlacementCandidates();
      for (const auto& placement : placements) {
        const double cost = PlacementCost(placement);
        if (cost < best_unrouted_cost) {
          best_unrouted = placement;
          best_unrouted_cost = cost;
        }
        elite.push_back({cost, placement});
        std::sort(elite.begin(), elite.end(),
                  [](const auto& a, const auto& b) {
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
        " elite=" + std::to_string(elite.size()) +
        " placement_only=1");
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
