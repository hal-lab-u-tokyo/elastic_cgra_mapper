#include "../../array_engine_internal.hpp"

namespace mapper::detail::placement2d {

// Traversal plan and YOTT annotation construction.

int Placement2DArrayEngine::FindUnusedEdgeIndex(
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

int Placement2DArrayEngine::ChooseCPUMappingZigZagNeighbor(
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

  return candidates[AuthorRandomInt(static_cast<int>(candidates.size()))];
}

void Placement2DArrayEngine::AddCPUMappingCycleAnnotation(
    std::vector<Step>& plan, int step_index, int anchor_node,
    int distance) const {
  if (step_index < 0 || step_index >= static_cast<int>(plan.size())) return;
  plan[step_index].annotations.push_back(StepAnnotation{
      StepAnnotationKind::kReconvergence, anchor_node, distance});
}

void Placement2DArrayEngine::ApplyCPUMappingCycleAnnotations(
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
        AddCPUMappingCycleAnnotation(plan, j, cycle_end, count);
        count += 1;
        found_start = true;
      } else if (found_start &&
                 (value1 == plan[j].target || cycle_end == plan[j].anchor)) {
        value1 = plan[j].anchor;
        const int value2 = plan[j].target;
        if (value1 != cycle_end && value2 != cycle_end) {
          walk_indices.insert(walk_indices.begin(), j);
          AddCPUMappingCycleAnnotation(plan, j, cycle_end, count);
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

std::vector<Step> Placement2DArrayEngine::BuildCPUMappingPlan() {
  std::vector<Step> plan;
  std::vector<char> used_edges(edges_.size(), false);
  std::vector<std::pair<int, int>> cycle_edges;
  std::vector<char> visited_nodes(dfg_.GetNodeNum(), false);
  std::vector<std::vector<int>> local_fanin = predecessors_;
  std::vector<std::vector<int>> local_fanout = successors_;
  std::vector<std::pair<int, int>> stack;
  for (int root : CPUMappingTraversalRoots()) stack.push_back({root, 1});
  int selection_mode = TraversalNeighborMode(IsCPUMappingYOTT());
  std::vector<int> critical_path;
  std::vector<double> betweenness;
  if (selection_mode == 1) betweenness = BuildBetweennessCentralityScore();
  if (selection_mode == 2) critical_path = BuildCPUMappingInputDistanceScore();

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
    int edge_id = FindUnusedEdgeIndex(from, to, used_edges);
    if (edge_id < 0) {
      edge_id = FindUnusedEdgeIndex(to, from, used_edges);
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
        b = ChooseCPUMappingZigZagNeighbor(local_fanout[a], true,
                                           selection_mode, true, critical_path,
                                           betweenness);
        for (int i = 0; i < fanin; i++) stack.push_back({a, 1});
        stack.push_back({b, 0});
        remove_value(local_fanout[a], b);
        remove_value(local_fanin[b], a);
        append_step(a, b);
      } else if (fanin >= 1) {
        b = ChooseCPUMappingZigZagNeighbor(local_fanin[a], false,
                                           selection_mode, true, critical_path,
                                           betweenness);
        stack.push_back({a, 1});
        for (int i = 0; i < fanin; i++) stack.push_back({b, 1});
        remove_value(local_fanin[a], b);
        remove_value(local_fanout[b], a);
        append_step(a, b);
      }
    } else {
      if (fanin >= 1) {
        b = ChooseCPUMappingZigZagNeighbor(local_fanin[a], false,
                                           selection_mode, false, critical_path,
                                           betweenness);
        for (int i = 0; i < fanout; i++) stack.push_back({a, 0});
        stack.push_back({b, 1});
        remove_value(local_fanin[a], b);
        remove_value(local_fanout[b], a);
        append_step(a, b);
      } else if (fanout >= 1) {
        b = ChooseCPUMappingZigZagNeighbor(local_fanout[a], true,
                                           selection_mode, false, critical_path,
                                           betweenness);
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
  ShuffleWithAuthorRandom(remaining);
  for (int edge_id : remaining) {
    plan.push_back(
        Step{edges_[edge_id].source, edges_[edge_id].target, edge_id, {}});
  }
  if (UsesYOTTAnnotations()) ApplyCPUMappingCycleAnnotations(plan, cycle_edges);
  return plan;
}

}  // namespace mapper::detail::placement2d
