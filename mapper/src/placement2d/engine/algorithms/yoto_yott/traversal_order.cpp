#include "../../array_engine_internal.hpp"

namespace mapper::detail::placement2d {

// DFS/BFS baselines traverse DFG edges as an undirected graph so that, like
// zigzag, a Step always means "place target near anchor" regardless of the
// original data-flow direction.
std::vector<Step> Placement2DArrayEngine::BuildGraphSearchPlan(
    bool breadth_first) {
  std::vector<Step> plan;
  std::vector<std::pair<int, int>> cycle_edges;
  std::vector<char> used_edges(edges_.size(), false);
  std::vector<char> discovered(dfg_.GetNodeNum(), false);
  std::vector<std::vector<int>> adjacency = incident_edge_ids_;
  for (auto& edge_ids : adjacency) ShuffleWithAuthorRandom(edge_ids);

  std::vector<int> starts = CPUMappingTraversalRoots();
  std::vector<int> remaining_nodes(dfg_.GetNodeNum());
  std::iota(remaining_nodes.begin(), remaining_nodes.end(), 0);
  ShuffleWithAuthorRandom(remaining_nodes);
  starts.insert(starts.end(), remaining_nodes.begin(), remaining_nodes.end());

  auto other_endpoint = [&](int edge_id, int node) {
    const EdgeInfo& edge = edges_[edge_id];
    return edge.source == node ? edge.target : edge.source;
  };

  auto append_edge = [&](int anchor, int target, int edge_id) {
    if (discovered[target]) cycle_edges.push_back({anchor, target});
    plan.push_back(Step{anchor, target, edge_id, {}});
  };

  for (int start : starts) {
    bool has_unused_edge = false;
    for (int edge_id : adjacency[start]) {
      if (!used_edges[edge_id]) {
        has_unused_edge = true;
        break;
      }
    }
    if (!has_unused_edge) {
      discovered[start] = true;
      continue;
    }

    discovered[start] = true;
    if (breadth_first) {
      std::queue<int> queue;
      queue.push(start);
      while (!queue.empty()) {
        const int anchor = queue.front();
        queue.pop();
        for (int edge_id : adjacency[anchor]) {
          if (used_edges[edge_id]) continue;
          used_edges[edge_id] = true;
          const int target = other_endpoint(edge_id, anchor);
          const bool target_was_discovered = discovered[target];
          append_edge(anchor, target, edge_id);
          if (!target_was_discovered) {
            discovered[target] = true;
            queue.push(target);
          }
        }
      }
      continue;
    }

    std::vector<std::pair<int, int>> stack{{start, 0}};
    while (!stack.empty()) {
      const int anchor = stack.back().first;
      int& next_index = stack.back().second;
      while (next_index < static_cast<int>(adjacency[anchor].size()) &&
             used_edges[adjacency[anchor][next_index]]) {
        next_index++;
      }
      if (next_index == static_cast<int>(adjacency[anchor].size())) {
        stack.pop_back();
        continue;
      }

      const int edge_id = adjacency[anchor][next_index++];
      used_edges[edge_id] = true;
      const int target = other_endpoint(edge_id, anchor);
      const bool target_was_discovered = discovered[target];
      append_edge(anchor, target, edge_id);
      if (!target_was_discovered) {
        discovered[target] = true;
        stack.push_back({target, 0});
      }
    }
  }

  if (plan.size() != edges_.size()) {
    throw std::runtime_error("graph traversal did not visit every DFG edge");
  }
  if (UsesYOTTAnnotations()) ApplyCPUMappingCycleAnnotations(plan, cycle_edges);
  return plan;
}

// This baseline removes all graph-traversal structure: both edge order and
// orientation are independently randomized for every trial.
std::vector<Step> Placement2DArrayEngine::BuildFullyRandomPlan() {
  std::vector<int> edge_ids(edges_.size());
  std::iota(edge_ids.begin(), edge_ids.end(), 0);
  ShuffleWithAuthorRandom(edge_ids);

  std::vector<Step> plan;
  std::vector<std::pair<int, int>> cycle_edges;
  std::vector<char> seen(dfg_.GetNodeNum(), false);
  plan.reserve(edge_ids.size());
  for (int edge_id : edge_ids) {
    int anchor = edges_[edge_id].source;
    int target = edges_[edge_id].target;
    if (AuthorRandomInt(2) != 0) std::swap(anchor, target);
    if (seen[anchor] && seen[target]) cycle_edges.push_back({anchor, target});
    plan.push_back(Step{anchor, target, edge_id, {}});
    seen[anchor] = true;
    seen[target] = true;
  }

  if (plan.size() != edges_.size()) {
    throw std::runtime_error("random traversal did not visit every DFG edge");
  }
  if (UsesYOTTAnnotations()) ApplyCPUMappingCycleAnnotations(plan, cycle_edges);
  return plan;
}

}  // namespace mapper::detail::placement2d
