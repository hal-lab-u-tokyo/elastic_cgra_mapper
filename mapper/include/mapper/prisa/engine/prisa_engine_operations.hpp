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
#include <mapper/prisa/prisa_options.hpp>
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

namespace mapper::prisa {
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
// Operations identical in the spatial and context engines. Resource legality
// and state remain in each concrete engine; this layer owns no dispatch.
template <typename Engine>
class PrisaEngineOperations {
 public:
  int PRISAMaxIterations() const {
    const auto& self = static_cast<const Engine&>(*this);
    if (self.options_.max_iterations.has_value()) {
      return std::max(1, self.options_.max_iterations.value());
    }
    return std::max(1, self.dfg_.GetNodeNum() * 10);
  }
  int PRISAResourceSideLength() const {
    const auto& self = static_cast<const Engine&>(*this);
    const auto& resources = self.Placement2DResourceOrder();
    if (resources.empty()) return 1;
    int min_row = std::numeric_limits<int>::max();
    int max_row = std::numeric_limits<int>::min();
    int min_column = std::numeric_limits<int>::max();
    int max_column = std::numeric_limits<int>::min();
    for (int resource : resources) {
      const auto position = self.mrrg_.GetNodeProperty(resource).position_id;
      min_row = std::min(min_row, position.first);
      max_row = std::max(max_row, position.first);
      min_column = std::min(min_column, position.second);
      max_column = std::max(max_column, position.second);
    }
    return std::max(max_row - min_row + 1, max_column - min_column + 1);
  }
  int PRISAWeakPairCount(int resource_count) const {
    const auto& self = static_cast<const Engine&>(*this);
    const int k = std::max(1, self.PRISAResourceSideLength());
    if (k < 2 || resource_count < 2) return 0;
    const long long kk = k;
    const long long numerator =
        kk * kk * kk * kk + 2 * kk * kk * kk - kk * kk - 2 * kk;
    const long long paper_weak_pairs = numerator / 12;
    const long long total_pairs =
        static_cast<long long>(resource_count) * (resource_count - 1) / 2;
    return static_cast<int>(
        std::max(0LL, std::min(paper_weak_pairs, total_pairs)));
  }
  void BuildPRISADistanceRegions() {
    auto& self = static_cast<Engine&>(*this);
    const auto& resources = self.Placement2DResourceOrder();
    const int resource_count = static_cast<int>(resources.size());
    self.prisa_distance_matrix_.assign(resource_count * resource_count, 0);
    self.prisa_weak_region_matrix_.assign(resource_count * resource_count, 0);
    self.prisa_weak_distance_threshold_ = (1 << 28);
    if (resource_count < 2) return;

    std::vector<int> pair_distances;
    pair_distances.reserve(resource_count * (resource_count - 1) / 2);
    for (int row = 0; row < resource_count; row++) {
      for (int column = 0; column < resource_count; column++) {
        if (row == column) continue;
        const int distance = DirectedDistanceCost(
            self.ResourceDistance(resources[row], resources[column]));
        self.prisa_distance_matrix_[row * resource_count + column] = distance;
        if (row < column) pair_distances.push_back(distance);
      }
    }

    const int weak_pair_count = self.PRISAWeakPairCount(resource_count);
    if (weak_pair_count <= 0 || pair_distances.empty()) return;
    // PRISA defines PR/WR on the resource distance matrix. WR consists of the
    // heavy-distance entries; SIS only supplies a low-bandwidth initial
    // permutation before this distance-matrix-guided search.
    std::sort(pair_distances.begin(), pair_distances.end(),
              std::greater<int>());
    const int threshold_index =
        std::min(weak_pair_count, static_cast<int>(pair_distances.size())) - 1;
    self.prisa_weak_distance_threshold_ = pair_distances[threshold_index];

    for (int row = 0; row < resource_count; row++) {
      for (int column = 0; column < resource_count; column++) {
        if (row == column) continue;
        self.prisa_weak_region_matrix_[row * resource_count + column] =
            self.prisa_distance_matrix_[row * resource_count + column] >=
            self.prisa_weak_distance_threshold_;
      }
    }
  }
  bool IsPRISAWeakRegion(int row, int column, int resource_count) const {
    const auto& self = static_cast<const Engine&>(*this);
    if (row == column) return false;
    if (row < 0 || column < 0 || row >= resource_count ||
        column >= resource_count) {
      return false;
    }
    if (static_cast<int>(self.prisa_weak_region_matrix_.size()) ==
        resource_count * resource_count) {
      return self.prisa_weak_region_matrix_[row * resource_count + column] != 0;
    }
    if (static_cast<int>(self.prisa_distance_matrix_.size()) ==
        resource_count * resource_count) {
      return self.prisa_distance_matrix_[row * resource_count + column] >=
             self.prisa_weak_distance_threshold_;
    }
    return false;
  }
  bool IsPRISAPotentialRegion(int row, int column, int resource_count) const {
    const auto& self = static_cast<const Engine&>(*this);
    if (row == column) return false;
    return !self.IsPRISAWeakRegion(row, column, resource_count);
  }
  std::vector<int> LowBandwidthNodeOrder() {
    auto& self = static_cast<Engine&>(*this);
    const int n = self.dfg_.GetNodeNum();
    if (n == 0) return {};

    auto bfs_distance = [&](int root,
                            const std::vector<char>* allowed = nullptr) {
      std::vector<int> distance(n, -1);
      std::queue<int> q;
      distance[root] = 0;
      q.push(root);
      while (!q.empty()) {
        const int u = q.front();
        q.pop();
        for (int edge_id : self.incident_edge_ids_[u]) {
          const int v = self.OtherEndpoint(self.dfg_edges_[edge_id], u);
          if (v < 0 || distance[v] >= 0) continue;
          if (allowed != nullptr && !(*allowed)[v]) continue;
          distance[v] = distance[u] + 1;
          q.push(v);
        }
      }
      return distance;
    };

    std::vector<char> visited(n, 0);
    std::vector<std::vector<int>> components;
    for (int root = 0; root < n; root++) {
      if (visited[root]) continue;
      std::vector<int> component;
      std::queue<int> q;
      visited[root] = 1;
      q.push(root);
      while (!q.empty()) {
        const int u = q.front();
        q.pop();
        component.push_back(u);
        for (int edge_id : self.incident_edge_ids_[u]) {
          const int v = self.OtherEndpoint(self.dfg_edges_[edge_id], u);
          if (v < 0 || visited[v]) continue;
          visited[v] = 1;
          q.push(v);
        }
      }
      components.push_back(std::move(component));
    }
    std::sort(components.begin(), components.end(),
              [&](const std::vector<int>& a, const std::vector<int>& b) {
                if (a.size() != b.size()) return a.size() > b.size();
                const auto max_degree = [&](const std::vector<int>& component) {
                  int result = -1;
                  for (int node : component) {
                    result = std::max(result, self.annotation_.degree[node]);
                  }
                  return result;
                };
                const int a_degree = max_degree(a);
                const int b_degree = max_degree(b);
                if (a_degree != b_degree) return a_degree > b_degree;
                return *std::min_element(a.begin(), a.end()) <
                       *std::min_element(b.begin(), b.end());
              });

    std::vector<int> order;
    order.reserve(n);
    for (const auto& component : components) {
      std::vector<char> in_component(n, 0);
      for (int node : component) in_component[node] = 1;

      int highest_degree = component.front();
      for (int node : component) {
        if (self.annotation_.degree[node] >
                self.annotation_.degree[highest_degree] ||
            (self.annotation_.degree[node] ==
                 self.annotation_.degree[highest_degree] &&
             node < highest_degree)) {
          highest_degree = node;
        }
      }

      const auto distance_from_high_degree =
          bfs_distance(highest_degree, &in_component);
      int max_distance = 0;
      std::vector<int> farthest_nodes;
      for (int node : component) {
        if (distance_from_high_degree[node] < 0) continue;
        if (distance_from_high_degree[node] > max_distance) {
          max_distance = distance_from_high_degree[node];
          farthest_nodes.clear();
        }
        if (distance_from_high_degree[node] == max_distance) {
          farthest_nodes.push_back(node);
        }
      }
      if (farthest_nodes.empty()) farthest_nodes.push_back(highest_degree);
      const int start = *std::min_element(
          farthest_nodes.begin(), farthest_nodes.end(), [&](int a, int b) {
            if (self.annotation_.degree[a] != self.annotation_.degree[b]) {
              return self.annotation_.degree[a] < self.annotation_.degree[b];
            }
            return a < b;
          });

      // Algorithm 2 is a low-bandwidth labeling. Keep the component-local BFS
      // visitation order instead of re-sorting entire levels, because the
      // latter breaks parent-neighbor locality on wide DFGs such as fir16.
      std::vector<char> labeled(n, 0);
      std::queue<int> q;
      labeled[start] = 1;
      q.push(start);
      while (!q.empty()) {
        const int u = q.front();
        q.pop();
        order.push_back(u);

        std::vector<int> neighbors;
        for (int edge_id : self.incident_edge_ids_[u]) {
          const int v = self.OtherEndpoint(self.dfg_edges_[edge_id], u);
          if (v < 0 || !in_component[v] || labeled[v]) continue;
          labeled[v] = 1;
          neighbors.push_back(v);
        }
        std::sort(neighbors.begin(), neighbors.end(), [&](int a, int b) {
          if (self.annotation_.degree[a] != self.annotation_.degree[b]) {
            return self.annotation_.degree[a] < self.annotation_.degree[b];
          }
          return a < b;
        });
        for (int neighbor : neighbors) q.push(neighbor);
      }

      for (int node : component) {
        if (!labeled[node]) order.push_back(node);
      }
    }
    return order;
  }
  int PRISAInitialSolutionSampleCount() const {
    const auto& self = static_cast<const Engine&>(*this);
    if (!self.UsesPRISASIS()) return 1;
    return 1;
  }
  template <typename State>
  bool CanSwapResources(const State& state, int ra, int rb) const {
    const auto& self = static_cast<const Engine&>(*this);
    if (ra == rb || ra < 0 || rb < 0) return false;
    const int a = state.mrrg_to_dfg[ra];
    const int b = state.mrrg_to_dfg[rb];
    if (a < 0 && b < 0) return false;
    if (a >= 0 && !self.IsCompatible(a, rb)) return false;
    if (b >= 0 && !self.IsCompatible(b, ra)) return false;
    return true;
  }
  template <typename State>
  bool ApplyResourceSwap(State& state, int ra, int rb) const {
    const auto& self = static_cast<const Engine&>(*this);
    if (!self.CanSwapResources(state, ra, rb)) return false;
    const int a = state.mrrg_to_dfg[ra];
    const int b = state.mrrg_to_dfg[rb];
    state.mrrg_to_dfg[ra] = b;
    state.mrrg_to_dfg[rb] = a;
    if (a >= 0) state.dfg_to_mrrg[a] = rb;
    if (b >= 0) state.dfg_to_mrrg[b] = ra;
    return true;
  }
  template <typename State>
  bool ApplyRandomResourceSwap(State& state,
                               const std::vector<int>& resources) {
    auto& self = static_cast<Engine&>(*this);
    if (resources.size() < 2) return false;
    std::uniform_int_distribution<int> dist(
        0, static_cast<int>(resources.size()) - 1);
    for (int attempt = 0; attempt < 32; attempt++) {
      const int ra = resources[dist(self.rng_)];
      const int rb = resources[dist(self.rng_)];
      if (self.ApplyResourceSwap(state, ra, rb)) return true;
    }
    return false;
  }
  template <typename State>
  bool ApplyPlacementCostSampledSwap(State& state,
                                     const std::vector<int>& resources) {
    auto& self = static_cast<Engine&>(*this);
    if (resources.size() < 2) return false;

    struct Candidate {
      double cost_delta = std::numeric_limits<double>::infinity();
      int first_resource = -1;
      int second_resource = -1;
      int random_order = 0;
    };

    const int resource_count = static_cast<int>(resources.size());
    const int total_pairs = resource_count * (resource_count - 1) / 2;
    const int sample_count =
        std::min(total_pairs, std::max(16, resource_count * 4));

    auto normalize_pair = [](int lhs, int rhs) {
      if (lhs > rhs) std::swap(lhs, rhs);
      return std::pair<int, int>(lhs, rhs);
    };

    std::set<std::pair<int, int>> sampled_pairs;
    if (sample_count == total_pairs) {
      for (int i = 0; i < resource_count; i++) {
        for (int j = i + 1; j < resource_count; j++) {
          sampled_pairs.insert({i, j});
        }
      }
    } else {
      std::uniform_int_distribution<int> resource_dist(0, resource_count - 1);
      int attempts = 0;
      while (static_cast<int>(sampled_pairs.size()) < sample_count &&
             attempts < sample_count * 16) {
        attempts++;
        const int first = resource_dist(self.rng_);
        const int second = resource_dist(self.rng_);
        if (first == second) continue;
        sampled_pairs.insert(normalize_pair(first, second));
      }
    }

    std::vector<Candidate> candidates;
    candidates.reserve(sampled_pairs.size());
    int random_order = 0;
    for (const auto& [first_index, second_index] : sampled_pairs) {
      const int first_resource = resources[first_index];
      const int second_resource = resources[second_index];
      if (!self.CanSwapResources(state, first_resource, second_resource))
        continue;
      candidates.push_back(
          Candidate{self.PlacementCostDeltaForSwap(state, first_resource,
                                                   second_resource),
                    first_resource, second_resource, random_order++});
    }
    if (candidates.empty())
      return self.ApplyRandomResourceSwap(state, resources);

    std::shuffle(candidates.begin(), candidates.end(), self.rng_);
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) {
                if (a.cost_delta != b.cost_delta) {
                  return a.cost_delta < b.cost_delta;
                }
                return a.random_order < b.random_order;
              });

    int selection_count = 1;
    while (selection_count < static_cast<int>(candidates.size()) &&
           candidates[selection_count].cost_delta ==
               candidates.front().cost_delta) {
      selection_count++;
    }
    std::uniform_int_distribution<int> dist(0, selection_count - 1);
    const Candidate& chosen = candidates[dist(self.rng_)];
    return self.ApplyResourceSwap(state, chosen.first_resource,
                                  chosen.second_resource);
  }
  template <typename State>
  int ResourceAfterSwap(int dfg_node_id, int first_resource,
                        int second_resource, const State& state) const {
    const auto& self = static_cast<const Engine&>(*this);
    int resource = state.dfg_to_mrrg[dfg_node_id];
    if (resource == first_resource) return second_resource;
    if (resource == second_resource) return first_resource;
    return resource;
  }
  template <typename State>
  double PlacementCostDeltaForSwap(const State& state, int first_resource,
                                   int second_resource) const {
    const auto& self = static_cast<const Engine&>(*this);
    std::vector<int> affected_edges;
    const int first_node = state.mrrg_to_dfg[first_resource];
    const int second_node = state.mrrg_to_dfg[second_resource];
    if (first_node >= 0) {
      affected_edges.insert(affected_edges.end(),
                            self.incident_edge_ids_[first_node].begin(),
                            self.incident_edge_ids_[first_node].end());
    }
    if (second_node >= 0) {
      affected_edges.insert(affected_edges.end(),
                            self.incident_edge_ids_[second_node].begin(),
                            self.incident_edge_ids_[second_node].end());
    }
    std::sort(affected_edges.begin(), affected_edges.end());
    affected_edges.erase(
        std::unique(affected_edges.begin(), affected_edges.end()),
        affected_edges.end());

    double before = 0.0;
    double after = 0.0;
    for (int edge_id : affected_edges) {
      const auto& edge = self.dfg_edges_[edge_id];
      before += self.EdgePlacementCost(edge, state.dfg_to_mrrg);
      after += self.ResourcePairPlacementCost(
          self.ResourceAfterSwap(edge.source, first_resource, second_resource,
                                 state),
          self.ResourceAfterSwap(edge.target, first_resource, second_resource,
                                 state));
    }
    return after - before;
  }
  template <typename State>
  int PRISAWeakEdgeDeltaForSwap(const State& state, int first_resource,
                                int second_resource) const {
    const auto& self = static_cast<const Engine&>(*this);
    std::vector<int> affected_edges;
    const int first_node = state.mrrg_to_dfg[first_resource];
    const int second_node = state.mrrg_to_dfg[second_resource];
    if (first_node >= 0) {
      affected_edges.insert(affected_edges.end(),
                            self.incident_edge_ids_[first_node].begin(),
                            self.incident_edge_ids_[first_node].end());
    }
    if (second_node >= 0) {
      affected_edges.insert(affected_edges.end(),
                            self.incident_edge_ids_[second_node].begin(),
                            self.incident_edge_ids_[second_node].end());
    }
    std::sort(affected_edges.begin(), affected_edges.end());
    affected_edges.erase(
        std::unique(affected_edges.begin(), affected_edges.end()),
        affected_edges.end());

    const int resource_count =
        static_cast<int>(self.Placement2DResourceOrder().size());
    int before = 0;
    int after = 0;
    for (int edge_id : affected_edges) {
      const auto& edge = self.dfg_edges_[edge_id];
      before +=
          self.PRISAEdgeIsWeak(state.dfg_to_mrrg[edge.source],
                               state.dfg_to_mrrg[edge.target], resource_count)
              ? 1
              : 0;
      after += self.PRISAEdgeIsWeak(
                   self.ResourceAfterSwap(edge.source, first_resource,
                                          second_resource, state),
                   self.ResourceAfterSwap(edge.target, first_resource,
                                          second_resource, state),
                   resource_count)
                   ? 1
                   : 0;
    }
    return after - before;
  }
  template <typename State>
  double CostAwarePRISAScore(const State& state) const {
    const auto& self = static_cast<const Engine&>(*this);
    const int weak_edges = self.PRISAWeakEdgeCount(state);
    const auto metrics = self.ComputeCostAwarePRISAMetrics(state);
    if (metrics.mesh_hop_sum >= 1.0e8 || metrics.max_mesh_hop >= (1 << 28) ||
        metrics.mapped_lp_mesh_hop >= (1 << 28)) {
      return 1.0e8;
    }
    // The strict PRISA WR test is intentionally coarse. This derived mapper
    // keeps WR as the first-stage objective, then optimizes the placement-only
    // paper metrics directly: mapped LP, max FIFO/hop, and total mesh hop.
    return weak_edges * 1000000.0 + metrics.mesh_hop_sum * 50.0 +
           metrics.mapped_lp_mesh_hop * 100.0 + metrics.max_mesh_hop * 200.0 -
           metrics.direct_edge_count * 80.0;
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
  bool NeedsAllPairsDistances() const {
    const auto& self = static_cast<const Engine&>(*this);
    return !self.UsesApproximatePlacementDistance();
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
  bool PRISAEdgeIsWeak(int source_resource, int target_resource,
                       int resource_count) const {
    const auto& self = static_cast<const Engine&>(*this);
    if (source_resource < 0 || target_resource < 0) return false;
    const auto& resource_order = self.ResourceOrderIndex();
    if (source_resource >= static_cast<int>(resource_order.size()) ||
        target_resource >= static_cast<int>(resource_order.size())) {
      return false;
    }
    const int row = resource_order[source_resource];
    const int column = resource_order[target_resource];
    if (row < 0 || column < 0) return false;
    return self.IsPRISAWeakRegion(row, column, resource_count);
  }
  template <typename State>
  int PRISAWeakEdgeCount(const State& state) const {
    const auto& self = static_cast<const Engine&>(*this);
    const int resource_count =
        static_cast<int>(self.Placement2DResourceOrder().size());
    int weak_edges = 0;
    for (const auto& edge : self.dfg_edges_) {
      if (self.PRISAEdgeIsWeak(state.dfg_to_mrrg[edge.source],
                               state.dfg_to_mrrg[edge.target],
                               resource_count)) {
        weak_edges++;
      }
    }
    return weak_edges;
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
  void BuildAllPairsDistances() {
    auto& self = static_cast<Engine&>(*this);
    const int node_num = self.mrrg_.GetNodeNum();
    self.distance_.assign(node_num, std::vector<int>(node_num, (1 << 28)));
    for (int source = 0; source < node_num; source++) {
      std::queue<int> q;
      self.distance_[source][source] = 0;
      q.push(source);
      while (!q.empty()) {
        int u = q.front();
        q.pop();
        for (int v : self.mrrg_.GetAdjacentNodeIdVec(u)) {
          if (self.distance_[source][v] != (1 << 28)) continue;
          self.distance_[source][v] = self.distance_[source][u] + 1;
          q.push(v);
        }
      }
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
}  // namespace mapper::prisa
