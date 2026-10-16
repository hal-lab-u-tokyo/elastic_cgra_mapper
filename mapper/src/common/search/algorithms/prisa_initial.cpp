#include "../engine_internal.hpp"

namespace mapper::detail::placement_search {

// PRISA resource ordering, weak regions, and initial placement.

int PlacementSearchEngine::PRISAMaxIterations() const {
  if (options_.max_iterations.has_value()) {
    return std::max(1, options_.max_iterations.value());
  }
  return std::max(1, dfg_.GetNodeNum() * 10);
}

const std::vector<int>& PlacementSearchEngine::Placement2DResourceOrder()
    const {
  if (IsPRISALike() && !prisa_resources_.empty()) return prisa_resources_;
  return placement2d_resources_;
}

const std::vector<int>& PlacementSearchEngine::ResourceOrderIndex() const {
  if (IsPRISALike() && !prisa_resource_order_index_.empty()) {
    return prisa_resource_order_index_;
  }
  return placement2d_resource_order_index_;
}

int PlacementSearchEngine::PRISAResourceSideLength() const {
  const auto& resources = Placement2DResourceOrder();
  if (resources.empty()) return 1;
  int min_row = std::numeric_limits<int>::max();
  int max_row = std::numeric_limits<int>::min();
  int min_column = std::numeric_limits<int>::max();
  int max_column = std::numeric_limits<int>::min();
  for (int resource : resources) {
    const auto position = mrrg_.GetNodeProperty(resource).position_id;
    min_row = std::min(min_row, position.first);
    max_row = std::max(max_row, position.first);
    min_column = std::min(min_column, position.second);
    max_column = std::max(max_column, position.second);
  }
  return std::max(max_row - min_row + 1, max_column - min_column + 1);
}

int PlacementSearchEngine::PRISAWeakPairCount(int resource_count) const {
  const int k = std::max(1, PRISAResourceSideLength());
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

void PlacementSearchEngine::BuildPRISADistanceRegions() {
  const auto& resources = Placement2DResourceOrder();
  const int resource_count = static_cast<int>(resources.size());
  prisa_distance_matrix_.assign(resource_count * resource_count, 0);
  prisa_weak_region_matrix_.assign(resource_count * resource_count, 0);
  prisa_weak_distance_threshold_ = kInfDistance;
  if (resource_count < 2) return;

  std::vector<int> pair_distances;
  pair_distances.reserve(resource_count * (resource_count - 1) / 2);
  for (int row = 0; row < resource_count; row++) {
    for (int column = 0; column < resource_count; column++) {
      if (row == column) continue;
      const int distance = DirectedDistanceCost(
          ResourceDistance(resources[row], resources[column]));
      prisa_distance_matrix_[row * resource_count + column] = distance;
      if (row < column) pair_distances.push_back(distance);
    }
  }

  const int weak_pair_count = PRISAWeakPairCount(resource_count);
  if (weak_pair_count <= 0 || pair_distances.empty()) return;
  // PRISA defines PR/WR on the resource distance matrix. WR consists of the
  // heavy-distance entries; SIS only supplies a low-bandwidth initial
  // permutation before this distance-matrix-guided search.
  std::sort(pair_distances.begin(), pair_distances.end(), std::greater<int>());
  const int threshold_index =
      std::min(weak_pair_count, static_cast<int>(pair_distances.size())) - 1;
  prisa_weak_distance_threshold_ = pair_distances[threshold_index];

  for (int row = 0; row < resource_count; row++) {
    for (int column = 0; column < resource_count; column++) {
      if (row == column) continue;
      prisa_weak_region_matrix_[row * resource_count + column] =
          prisa_distance_matrix_[row * resource_count + column] >=
          prisa_weak_distance_threshold_;
    }
  }
}

bool PlacementSearchEngine::IsPRISAWeakRegion(int row, int column,
                                              int resource_count) const {
  if (row == column) return false;
  if (row < 0 || column < 0 || row >= resource_count ||
      column >= resource_count) {
    return false;
  }
  if (static_cast<int>(prisa_weak_region_matrix_.size()) ==
      resource_count * resource_count) {
    return prisa_weak_region_matrix_[row * resource_count + column] != 0;
  }
  if (static_cast<int>(prisa_distance_matrix_.size()) ==
      resource_count * resource_count) {
    return prisa_distance_matrix_[row * resource_count + column] >=
           prisa_weak_distance_threshold_;
  }
  return false;
}

bool PlacementSearchEngine::IsPRISAPotentialRegion(int row, int column,
                                                   int resource_count) const {
  if (row == column) return false;
  return !IsPRISAWeakRegion(row, column, resource_count);
}

std::vector<int> PlacementSearchEngine::LowBandwidthNodeOrder() {
  const int n = dfg_.GetNodeNum();
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
      for (int edge_id : incident_edge_ids_[u]) {
        const int v = OtherEndpoint(dfg_edges_[edge_id], u);
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
      for (int edge_id : incident_edge_ids_[u]) {
        const int v = OtherEndpoint(dfg_edges_[edge_id], u);
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
                  result = std::max(result, annotation_.degree[node]);
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
      if (annotation_.degree[node] > annotation_.degree[highest_degree] ||
          (annotation_.degree[node] == annotation_.degree[highest_degree] &&
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
          if (annotation_.degree[a] != annotation_.degree[b]) {
            return annotation_.degree[a] < annotation_.degree[b];
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
      for (int edge_id : incident_edge_ids_[u]) {
        const int v = OtherEndpoint(dfg_edges_[edge_id], u);
        if (v < 0 || !in_component[v] || labeled[v]) continue;
        labeled[v] = 1;
        neighbors.push_back(v);
      }
      std::sort(neighbors.begin(), neighbors.end(), [&](int a, int b) {
        if (annotation_.degree[a] != annotation_.degree[b]) {
          return annotation_.degree[a] < annotation_.degree[b];
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

std::optional<PlacementState>
PlacementSearchEngine::ConstructPRISAInitialPlacementFromOrder(
    const std::vector<int>& node_order) {
  const auto& resources = Placement2DResourceOrder();
  if (resources.empty()) return std::nullopt;
  PlacementState state;
  state.dfg_to_mrrg.assign(dfg_.GetNodeNum(), -1);
  state.mrrg_to_dfg.assign(mrrg_.GetNodeNum(), -1);
  for (int label = 0; label < static_cast<int>(node_order.size()); label++) {
    const int node = node_order[label];
    int best_resource = -1;
    int best_distance = kInfDistance;
    for (int order_index = 0; order_index < static_cast<int>(resources.size());
         order_index++) {
      const int r = resources[order_index];
      if (!CanOccupyResource(node, r, state)) continue;
      const int label_distance = std::abs(order_index - label);
      if (label_distance < best_distance) {
        best_distance = label_distance;
        best_resource = r;
      }
    }
    if (best_resource < 0) return std::nullopt;
    state.dfg_to_mrrg[node] = best_resource;
    state.mrrg_to_dfg[best_resource] = node;
  }
  return state;
}

int PlacementSearchEngine::PRISAInitialSolutionSampleCount() const {
  if (!UsesPRISASIS()) return 1;
  return 1;
}

std::optional<PlacementState>
PlacementSearchEngine::ConstructPRISAInitialPlacement() {
  if (!UsesPRISASIS()) return RandomLegalPlacement();
  std::optional<PlacementState> best;
  double best_cost = std::numeric_limits<double>::infinity();
  const int sample_count = PRISAInitialSolutionSampleCount();
  for (int sample = 0; sample < sample_count; sample++) {
    auto placement =
        ConstructPRISAInitialPlacementFromOrder(LowBandwidthNodeOrder());
    if (!placement.has_value()) continue;
    const double cost = PlacementCost(*placement);
    if (cost < best_cost) {
      best = placement;
      best_cost = cost;
    }
  }
  return best;
}

}  // namespace mapper::detail::placement_search
