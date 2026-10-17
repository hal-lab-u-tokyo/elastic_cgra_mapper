#pragma once
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <limits>
#include <mapper/yott/yott_options.hpp>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mapper::yott {
inline double SecondsSince(std::chrono::steady_clock::time_point start) {
  const auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::duration<double>>(now - start)
      .count();
}
inline bool SupportsOperation(const entity::MRRGNodeProperty& node_property,
                              entity::OpType op) {
  return std::find(node_property.supported_operations.begin(),
                   node_property.supported_operations.end(),
                   op) != node_property.supported_operations.end();
}
inline int DirectedDistanceCost(int distance) {
  return distance >= (1 << 28) ? (1 << 28) : std::max(1, distance);
}
inline double DegreePenalty(int target_degree, int freedom) {
  if (freedom >= target_degree) return freedom - target_degree;
  return 4.0 * (target_degree - freedom);
}

// Operations identical in the spatial and context engines. Resource legality
// and state remain in each concrete engine; this layer owns no dispatch.
template <typename Engine>
class YottEngineOperations {
 public:
  template <typename State>
  double DegreeMatchScore(int dfg_node_id, int mrrg_node_id,
                          const State& state) const {
    const auto& self = static_cast<const Engine&>(*this);
    const int target_degree = self.annotation_.degree[dfg_node_id];
    const int freedom = self.ResourceFreedom(mrrg_node_id, state);
    return DegreePenalty(target_degree, freedom);
  }
  std::vector<int> GetRawOutputTraversalRoots() {
    auto& self = static_cast<Engine&>(*this);
    std::vector<int> roots;
    for (int v = 0; v < self.dfg_.GetNodeNum(); v++) {
      if (self.successors_[v].empty() ||
          self.dfg_.GetNodeProperty(v).op == entity::OpType::kOutput) {
        roots.push_back(v);
      }
    }
    if (roots.empty()) {
      for (int v = 0; v < self.dfg_.GetNodeNum(); v++) roots.push_back(v);
    }
    std::shuffle(roots.begin(), roots.end(), self.rng_);
    return roots;
  }
  std::vector<int> GetOutputTraversalRoots() {
    auto& self = static_cast<Engine&>(*this);
    std::vector<int> roots;
    for (int v = 0; v < self.dfg_.GetNodeNum(); v++) {
      if (self.successors_[v].empty() ||
          self.dfg_.GetNodeProperty(v).op == entity::OpType::kOutput) {
        roots.push_back(v);
      }
    }
    if (roots.empty()) {
      for (int v = 0; v < self.dfg_.GetNodeNum(); v++) roots.push_back(v);
    }
    std::shuffle(roots.begin(), roots.end(), self.rng_);
    std::stable_sort(roots.begin(), roots.end(), [&](int a, int b) {
      if (self.IsIONode(a) != self.IsIONode(b))
        return self.IsIONode(a) > self.IsIONode(b);
      return self.annotation_.degree[a] > self.annotation_.degree[b];
    });
    return roots;
  }
  int FindDFGEdgeId(int source, int target) const {
    const auto& self = static_cast<const Engine&>(*this);
    for (const auto& edge : self.dfg_edges_) {
      if (edge.source == source && edge.target == target) return edge.id;
    }
    for (const auto& edge : self.dfg_edges_) {
      if (edge.source == target && edge.target == source) return edge.id;
    }
    return -1;
  }
  std::vector<int> UnvisitedIncidentEdges(
      int dfg_node_id, const std::vector<char>& visited_edges) const {
    const auto& self = static_cast<const Engine&>(*this);
    std::vector<int> result;
    for (int edge_id : self.incident_edge_ids_[dfg_node_id]) {
      if (!visited_edges[edge_id]) result.push_back(edge_id);
    }
    return result;
  }
  int PickTraversalEdge(int dfg_node_id, const std::vector<char>& visited_nodes,
                        const std::vector<char>& visited_edges) {
    auto& self = static_cast<Engine&>(*this);
    std::vector<int> candidates =
        self.UnvisitedIncidentEdges(dfg_node_id, visited_edges);
    if (candidates.empty()) return -1;
    std::shuffle(candidates.begin(), candidates.end(), self.rng_);
    std::stable_sort(candidates.begin(), candidates.end(), [&](int a, int b) {
      const int oa = self.OtherEndpoint(self.dfg_edges_[a], dfg_node_id);
      const int ob = self.OtherEndpoint(self.dfg_edges_[b], dfg_node_id);
      const bool a_unvisited = oa >= 0 && !visited_nodes[oa];
      const bool b_unvisited = ob >= 0 && !visited_nodes[ob];
      if (a_unvisited != b_unvisited) return a_unvisited > b_unvisited;
      const int degree_a = oa >= 0 ? self.annotation_.degree[oa] : -1;
      const int degree_b = ob >= 0 ? self.annotation_.degree[ob] : -1;
      return degree_a > degree_b;
    });
    return candidates.front();
  }
  std::vector<int> BuildCriticalPathScore() const {
    const auto& self = static_cast<const Engine&>(*this);
    const int n = self.dfg_.GetNodeNum();
    std::vector<int> memo(n, -1);
    std::vector<char> visiting(n, false);
    std::function<int(int)> dfs = [&](int node) {
      if (memo[node] >= 0) return memo[node];
      if (visiting[node]) return 0;
      visiting[node] = true;
      int best = 0;
      for (int successor : self.successors_[node]) {
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
  int ChoosePlacement2DNeighbor(const std::vector<int>& candidates,
                                bool choose_from_fanout, int mode,
                                bool zigzag_take_back,
                                const std::vector<int>& critical_path,
                                const std::vector<double>& betweenness) {
    auto& self = static_cast<Engine&>(*this);
    if (candidates.empty()) return -1;
    if (candidates.size() == 1) return candidates.front();

    if (mode == 0) {
      return *std::max_element(
          candidates.begin(), candidates.end(), [&](int a, int b) {
            const int da = choose_from_fanout
                               ? static_cast<int>(self.successors_[a].size())
                               : static_cast<int>(self.predecessors_[a].size());
            const int db = choose_from_fanout
                               ? static_cast<int>(self.successors_[b].size())
                               : static_cast<int>(self.predecessors_[b].size());
            if (da != db) return da < db;
            return self.annotation_.degree[a] < self.annotation_.degree[b];
          });
    }
    if (mode == 1) {
      return *std::max_element(
          candidates.begin(), candidates.end(), [&](int a, int b) {
            if (betweenness[a] != betweenness[b]) {
              return betweenness[a] < betweenness[b];
            }
            return self.annotation_.degree[a] < self.annotation_.degree[b];
          });
    }
    if (mode == 2) {
      return *std::max_element(
          candidates.begin(), candidates.end(), [&](int a, int b) {
            if (critical_path[a] != critical_path[b]) {
              return critical_path[a] < critical_path[b];
            }
            return self.annotation_.degree[a] < self.annotation_.degree[b];
          });
    }
    if (mode == 3) {
      return zigzag_take_back ? candidates.back() : candidates.front();
    }

    std::uniform_int_distribution<int> dist(
        0, static_cast<int>(candidates.size()) - 1);
    return candidates[dist(self.rng_)];
  }
  std::vector<int> BuildASAPContextLevels() const {
    const auto& self = static_cast<const Engine&>(*this);
    const int n = self.dfg_.GetNodeNum();
    std::vector<int> indegree(n, 0);
    std::vector<int> level(n, 0);
    for (const auto& edge : self.dfg_edges_) {
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
      for (int successor : self.successors_[node_id]) {
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
  template <typename State>
  void PushPlacementCandidate(std::vector<State>& result,
                              const std::optional<State>& placement) {
    auto& self = static_cast<Engine&>(*this);
    if (!placement.has_value()) return;
    auto finalized = self.FinalizePhysicalPlacementIfNeeded(*placement);
    if (finalized.has_value()) result.push_back(*finalized);
  }
  int SeedCount() const {
    const auto& self = static_cast<const Engine&>(*this);
    if (!self.options_.seed_count.has_value()) return 1;
    return std::max(1, self.options_.seed_count.value());
  }
  int MaxTrials() const {
    const auto& self = static_cast<const Engine&>(*this);
    const int default_trials = std::max(100, self.dfg_.GetNodeNum() * 4);
    if (!self.options_.max_trials.has_value()) return default_trials;
    return std::max(1, self.options_.max_trials.value());
  }
  int RoutingRetryCount() const {
    const auto& self = static_cast<const Engine&>(*this);
    if (!self.options_.routing_retry_count.has_value()) return 4;
    return std::max(1, self.options_.routing_retry_count.value());
  }
  int MaxIterations() const {
    const auto& self = static_cast<const Engine&>(*this);
    const int default_iterations =
        std::max(5000, self.dfg_.GetNodeNum() * self.mrrg_.GetNodeNum() * 30);
    if (!self.options_.max_iterations.has_value()) return default_iterations;
    return std::max(1, self.options_.max_iterations.value());
  }
  unsigned int SeedFor(int seed_index) const {
    const auto& self = static_cast<const Engine&>(*this);
    const unsigned int configured_seed =
        self.options_.random_seed.has_value()
            ? static_cast<unsigned int>(self.options_.random_seed.value())
            : self.base_seed_;
    return configured_seed +
           static_cast<unsigned int>(0x9E3779B9u * seed_index);
  }
  void ResetSeed(int seed_index) {
    auto& self = static_cast<Engine&>(*this);
    self.rng_.seed(self.SeedFor(seed_index));
  }
  void Log(const std::string& message) const {
    const auto& self = static_cast<const Engine&>(*this);
    if (!self.log_file_path_.has_value()) return;
    std::ofstream ofs(self.log_file_path_.value(), std::ios::app);
    if (ofs) ofs << "[" << self.MapperName() << "] " << message << "\n";
  }
  double ResourcePairPlacementCost(int from_mrrg_node, int to_mrrg_node) const {
    const auto& self = static_cast<const Engine&>(*this);
    if (from_mrrg_node < 0 || to_mrrg_node < 0) return 1.0e8;
    const int distance =
        self.DirectedResourceDistance(from_mrrg_node, to_mrrg_node);
    if (distance >= (1 << 28)) return 1.0e8;
    return DirectedDistanceCost(distance);
  }
  template <typename State>
  double PlacementCost(const State& state) const {
    const auto& self = static_cast<const Engine&>(*this);
    double cost = 0.0;
    for (const auto& edge : self.dfg_edges_) {
      const double edge_cost = self.EdgePlacementCost(edge, state.dfg_to_mrrg);
      if (edge_cost >= 1.0e8) return 1.0e8;
      cost += edge_cost;
    }
    return cost;
  }
  void BuildMRRGCache() {
    auto& self = static_cast<Engine&>(*this);
    const int node_num = self.mrrg_.GetNodeNum();
    self.placement2d_resources_.clear();
    self.placement2d_resources_.reserve(node_num);
    std::set<std::pair<int, int>> seen_positions;
    for (int r = 0; r < node_num; r++) {
      const auto property = self.mrrg_.GetNodeProperty(r);
      if (property.context_id != 0) continue;
      if (!self.SupportsAnyDFGOperation(r)) continue;
      if (!seen_positions.insert(property.position_id).second) continue;
      self.placement2d_resources_.push_back(r);
    }
    std::sort(self.placement2d_resources_.begin(),
              self.placement2d_resources_.end(), [&](int a, int b) {
                const auto pa = self.mrrg_.GetNodeProperty(a).position_id;
                const auto pb = self.mrrg_.GetNodeProperty(b).position_id;
                if (pa.first != pb.first) return pa.first < pb.first;
                // Paper-style 2D placement labels resources in row-major
                // order before applying PRISA's distance-matrix/SIS logic.
                return pa.second < pb.second;
              });
    self.placement2d_resource_order_index_.assign(node_num, -1);
    for (int i = 0; i < static_cast<int>(self.placement2d_resources_.size());
         i++) {
      self.placement2d_resource_order_index_[self.placement2d_resources_[i]] =
          i;
    }

    self.prisa_resources_ = self.placement2d_resources_;
    int min_row = std::numeric_limits<int>::max();
    for (int r : self.prisa_resources_) {
      min_row =
          std::min(min_row, self.mrrg_.GetNodeProperty(r).position_id.first);
    }
    std::sort(self.prisa_resources_.begin(), self.prisa_resources_.end(),
              [&](int a, int b) {
                const auto pa = self.mrrg_.GetNodeProperty(a).position_id;
                const auto pb = self.mrrg_.GetNodeProperty(b).position_id;
                if (pa.first != pb.first) return pa.first < pb.first;
                // PRISA's low-bandwidth labeling assumes nearby labels are
                // nearby resources. A serpentine path avoids row-major jumps
                // from the end of one row to the start of the next row.
                const bool reverse_row = ((pa.first - min_row) % 2) != 0;
                return reverse_row ? pa.second > pb.second
                                   : pa.second < pb.second;
              });
    self.prisa_resource_order_index_.assign(node_num, -1);
    for (int i = 0; i < static_cast<int>(self.prisa_resources_.size()); i++) {
      self.prisa_resource_order_index_[self.prisa_resources_[i]] = i;
    }
  }
  bool HasTimedOut(std::chrono::steady_clock::time_point start,
                   double reserve_s = 0.02) const {
    const auto& self = static_cast<const Engine&>(*this);
    return SecondsSince(start) + reserve_s >= self.timeout_s_;
  }
  bool IsCompatible(int dfg_node_id, int mrrg_node_id) const {
    const auto& self = static_cast<const Engine&>(*this);
    const entity::OpType op = self.dfg_.GetNodeProperty(dfg_node_id).op;
    return SupportsOperation(self.mrrg_.GetNodeProperty(mrrg_node_id), op);
  }
  bool SamePhysicalPosition(int lhs_mrrg_node, int rhs_mrrg_node) const {
    const auto& self = static_cast<const Engine&>(*this);
    return self.mrrg_.GetNodeProperty(lhs_mrrg_node).position_id ==
           self.mrrg_.GetNodeProperty(rhs_mrrg_node).position_id;
  }
  template <typename State>
  bool IsPhysicalPositionOccupied(int mrrg_node_id, const State& state) const {
    const auto& self = static_cast<const Engine&>(*this);
    for (int r = 0; r < self.mrrg_.GetNodeNum(); r++) {
      if (state.mrrg_to_dfg[r] == -1) continue;
      if (self.SamePhysicalPosition(mrrg_node_id, r)) return true;
    }
    return false;
  }
  template <typename State>
  std::vector<int> CompatibleResources(int dfg_node_id,
                                       const State& state) const {
    const auto& self = static_cast<const Engine&>(*this);
    std::vector<int> result;
    result.reserve(self.mrrg_.GetNodeNum());
    for (int r = 0; r < self.mrrg_.GetNodeNum(); r++) {
      if (!self.CanOccupyResource(dfg_node_id, r, state)) continue;
      result.push_back(r);
    }
    return result;
  }
  bool IsIONode(int dfg_node_id) const {
    const auto& self = static_cast<const Engine&>(*this);
    const entity::OpType op = self.dfg_.GetNodeProperty(dfg_node_id).op;
    return entity::IsMemoryAccessOperation(op);
  }
  bool SupportsAnyDFGOperation(int mrrg_node_id) const {
    const auto& self = static_cast<const Engine&>(*this);
    const auto property = self.mrrg_.GetNodeProperty(mrrg_node_id);
    for (entity::OpType op : property.supported_operations) {
      if (entity::IsDFGOp(op) && op != entity::OpType::kRoute &&
          op != entity::OpType::kNop) {
        return true;
      }
    }
    return false;
  }
  int ResourceDistance(int from_mrrg_node, int to_mrrg_node) const {
    const auto& self = static_cast<const Engine&>(*this);
    if (from_mrrg_node < 0 || to_mrrg_node < 0) return (1 << 28);
    return std::min(
        self.DirectedResourceDistance(from_mrrg_node, to_mrrg_node),
        self.DirectedResourceDistance(to_mrrg_node, from_mrrg_node));
  }
  int DirectedResourceDistance(int from_mrrg_node, int to_mrrg_node) const {
    const auto& self = static_cast<const Engine&>(*this);
    if (from_mrrg_node < 0 || to_mrrg_node < 0) return (1 << 28);
    if (!self.distance_.empty())
      return self.distance_[from_mrrg_node][to_mrrg_node];
    return self.ApproximateDirectedResourceDistance(from_mrrg_node,
                                                    to_mrrg_node);
  }
  int SpatialStepDistance(int from_mrrg_node, int to_mrrg_node) const {
    const auto& self = static_cast<const Engine&>(*this);
    if (from_mrrg_node < 0 || to_mrrg_node < 0) return (1 << 28);
    const auto from = self.mrrg_.GetNodeProperty(from_mrrg_node).position_id;
    const auto to = self.mrrg_.GetNodeProperty(to_mrrg_node).position_id;
    const int row_distance = std::abs(from.first - to.first);
    const int column_distance = std::abs(from.second - to.second);
    const auto config = self.mrrg_.GetMRRGConfig();
    if (config.network_type == entity::MRRGNetworkType::kOneHop2d) {
      return (row_distance + 1) / 2 + (column_distance + 1) / 2;
    }
    if (config.network_type == entity::MRRGNetworkType::kDiagonal) {
      return std::max(row_distance, column_distance);
    }
    return row_distance + column_distance;
  }
  int ApproximateDirectedResourceDistance(int from_mrrg_node,
                                          int to_mrrg_node) const {
    const auto& self = static_cast<const Engine&>(*this);
    if (from_mrrg_node == to_mrrg_node) return 0;
    const auto from = self.mrrg_.GetNodeProperty(from_mrrg_node);
    const auto to = self.mrrg_.GetNodeProperty(to_mrrg_node);
    const int context_size = std::max(1, from.context_size);
    const int context_delta =
        (to.context_id - from.context_id + context_size) % context_size;
    const int spatial_steps =
        self.SpatialStepDistance(from_mrrg_node, to_mrrg_node);

    if (self.mrrg_.GetMRRGConfig().cgra_type ==
        entity::MRRGCGRAType::kElastic) {
      if (spatial_steps > 0) return std::max(1, spatial_steps);
      return context_delta == 0 ? 0 : context_delta;
    }

    int steps = std::max(spatial_steps, context_delta);
    if (steps == 0) steps = context_size;
    while (steps % context_size != context_delta) {
      steps++;
    }
    return steps;
  }
  int PhysicalDistance(int from_mrrg_node, int to_mrrg_node) const {
    const auto& self = static_cast<const Engine&>(*this);
    if (from_mrrg_node < 0 || to_mrrg_node < 0) return (1 << 28);
    return std::max(1, self.SpatialStepDistance(from_mrrg_node, to_mrrg_node));
  }
  int BorderDistance(int mrrg_node_id) const {
    const auto& self = static_cast<const Engine&>(*this);
    const auto property = self.mrrg_.GetNodeProperty(mrrg_node_id);
    const int row = property.position_id.first;
    const int column = property.position_id.second;
    const auto config = self.mrrg_.GetMRRGConfig();
    return std::min(std::min(row, config.row - 1 - row),
                    std::min(column, config.column - 1 - column));
  }
  bool IsCornerResource(int mrrg_node_id) const {
    const auto& self = static_cast<const Engine&>(*this);
    const auto property = self.mrrg_.GetNodeProperty(mrrg_node_id);
    const auto config = self.mrrg_.GetMRRGConfig();
    const bool row_on_border = property.position_id.first == 0 ||
                               property.position_id.first == config.row - 1;
    const bool column_on_border =
        property.position_id.second == 0 ||
        property.position_id.second == config.column - 1;
    return row_on_border && column_on_border;
  }
  double IOPlacementScore(int dfg_node_id, int mrrg_node_id) const {
    const auto& self = static_cast<const Engine&>(*this);
    const auto property = self.mrrg_.GetNodeProperty(mrrg_node_id);
    double score = self.BorderDistance(mrrg_node_id);
    if (entity::IsMemoryAccessOperation(
            self.dfg_.GetNodeProperty(dfg_node_id).op) ||
        self.dfg_.GetNodeProperty(dfg_node_id).op == entity::OpType::kOutput) {
      score += property.is_memory_accessible ? -8.0 : 40.0;
    } else {
      score += property.is_memory_accessible ? -1.0 : 0.0;
    }
    return score;
  }
  double Placement2DInitialPlacementScore(int dfg_node_id,
                                          int mrrg_node_id) const {
    const auto& self = static_cast<const Engine&>(*this);
    if (self.IsIONode(dfg_node_id)) {
      double score = self.IOPlacementScore(dfg_node_id, mrrg_node_id);
      if (self.IsCornerResource(mrrg_node_id)) score += 4.0;
      return score;
    }
    double score = 0.0;
    if (self.BorderDistance(mrrg_node_id) == 0) score += 50.0;
    if (self.UsesSeparatePlacement2DIOCells() &&
        self.mrrg_.GetNodeProperty(mrrg_node_id).is_memory_accessible) {
      score += 100.0;
    }
    return score;
  }
  int ManhattanRouteScore(int from_mrrg_node, int to_mrrg_node) const {
    const auto& self = static_cast<const Engine&>(*this);
    if (from_mrrg_node < 0 || to_mrrg_node < 0) return (1 << 28);
    if (from_mrrg_node == to_mrrg_node) return 0;
    const auto from = self.mrrg_.GetNodeProperty(from_mrrg_node);
    const auto to = self.mrrg_.GetNodeProperty(to_mrrg_node);
    const int context_size = std::max(1, from.context_size);
    const int context_delta =
        (to.context_id - from.context_id + context_size) % context_size;
    return self.SpatialStepDistance(from_mrrg_node, to_mrrg_node) +
           context_delta;
  }
  bool PlacementOnly() const {
    const auto& self = static_cast<const Engine&>(*this);
    return self.options_.placement_only;
  }
};
}  // namespace mapper::yott
