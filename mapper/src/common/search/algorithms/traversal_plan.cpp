#include "../engine_internal.hpp"

namespace mapper::detail::placement_search {

// DFG traversal roots, order, and YOTT annotations.

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

int PlacementSearchEngine::OtherEndpoint(const DFGEdgeInfo& edge,
                                         int node_id) const {
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

int PlacementSearchEngine::PickTraversalEdge(
    int dfg_node_id, const std::vector<char>& visited_nodes,
    const std::vector<char>& visited_edges) {
  std::vector<int> candidates =
      UnvisitedIncidentEdges(dfg_node_id, visited_edges);
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

int PlacementSearchEngine::FindLastStepTargeting(const TraversalPlan& plan,
                                                 int target_node) const {
  for (int i = static_cast<int>(plan.sequence.size()) - 1; i >= 0; i--) {
    if (plan.sequence[i].target_node == target_node) return i;
  }
  return -1;
}

void PlacementSearchEngine::BackpropagateAnnotation(
    TraversalPlan& plan, int from_node, int anchor_node,
    SequenceAnnotationKind kind, int max_depth, int initial_distance) const {
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

std::vector<double> PlacementSearchEngine::BuildBetweennessCentralityScore()
    const {
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

int PlacementSearchEngine::Placement2DCentralityScore(int dfg_node_id) const {
  return annotation_.degree[dfg_node_id] +
         annotation_.in_degree[dfg_node_id] *
             annotation_.out_degree[dfg_node_id];
}

int PlacementSearchEngine::ChoosePlacement2DNeighbor(
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
          return annotation_.degree[a] < annotation_.degree[b];
        });
  }
  if (mode == 1) {
    return *std::max_element(
        candidates.begin(), candidates.end(), [&](int a, int b) {
          if (betweenness[a] != betweenness[b]) {
            return betweenness[a] < betweenness[b];
          }
          return annotation_.degree[a] < annotation_.degree[b];
        });
  }
  if (mode == 2) {
    return *std::max_element(
        candidates.begin(), candidates.end(), [&](int a, int b) {
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

TraversalPlan PlacementSearchEngine::BuildPlacement2DTraversalPlan(
    bool annotate) {
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

}  // namespace mapper::detail::placement_search
