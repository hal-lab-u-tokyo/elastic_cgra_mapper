#include <mapper/prisa/engine/array/prisa_array_engine.hpp>

namespace mapper::prisa_array {

// Shared traversal graph utilities used by paper-guided and cpu_mapping
// YOTO/YOTT variants. Method-specific files decide how these scores are used.

std::vector<int> Placement2DPRISAEngine::BuildCriticalPathScore() const {
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

}  // namespace mapper::prisa_array
