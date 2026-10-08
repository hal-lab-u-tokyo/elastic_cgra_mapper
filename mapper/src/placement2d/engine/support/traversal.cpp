#include "../array_engine_internal.hpp"

namespace mapper::detail::placement2d {

// Shared traversal graph utilities used by paper-guided and cpu_mapping
// YOTO/YOTT variants. Method-specific files decide how these scores are used.

bool Placement2DArrayEngine::IsOutputNode(int dfg_node) const {
  return dfg_.GetNodeProperty(dfg_node).op == entity::OpType::OUTPUT;
}

bool Placement2DArrayEngine::IsCornerCell(int cell) const {
  const int row = Row(cell);
  const int col = Col(cell);
  return (row == 0 || row == rows_ - 1) && (col == 0 || col == cols_ - 1);
}

std::vector<int> Placement2DArrayEngine::RawTraversalRoots() {
  std::vector<int> roots;
  for (int node = 0; node < dfg_.GetNodeNum(); node++) {
    if (successors_[node].empty() || IsOutputNode(node)) roots.push_back(node);
  }
  if (roots.empty()) {
    roots.resize(dfg_.GetNodeNum());
    std::iota(roots.begin(), roots.end(), 0);
  }
  std::shuffle(roots.begin(), roots.end(), rng_);
  return roots;
}

std::vector<int> Placement2DArrayEngine::CPUMappingTraversalRoots() {
  std::vector<int> roots;
  for (int node = 0; node < dfg_.GetNodeNum(); node++) {
    if (successors_[node].empty() || IsOutputNode(node)) roots.push_back(node);
  }
  if (roots.empty()) {
    roots.resize(dfg_.GetNodeNum());
    std::iota(roots.begin(), roots.end(), 0);
  }
  ShuffleWithAuthorRandom(roots);
  return roots;
}

int Placement2DArrayEngine::FindEdgeId(int source, int target) const {
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

int Placement2DArrayEngine::FindLastStepTargeting(const std::vector<Step>& plan,
                                                  int target_node) const {
  for (int i = static_cast<int>(plan.size()) - 1; i >= 0; i--) {
    if (plan[i].target == target_node) return i;
  }
  return -1;
}

std::vector<int> Placement2DArrayEngine::BuildCriticalPathScore() const {
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

std::vector<int> Placement2DArrayEngine::BuildCPUMappingInputDistanceScore()
    const {
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

std::vector<double> Placement2DArrayEngine::BuildBetweennessCentralityScore()
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

}  // namespace mapper::detail::placement2d
