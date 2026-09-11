#include "../../array_engine_internal.hpp"

namespace mapper::detail::placement2d {

// Initial placement and SIS order.

std::optional<PlacementState> Placement2DArrayEngine::RandomPlacement() {
  PlacementState state;
  state.dfg_to_cell.assign(dfg_.GetNodeNum(), -1);
  state.cell_to_dfg.assign(rows_ * cols_, -1);
  std::vector<int> nodes(dfg_.GetNodeNum());
  std::iota(nodes.begin(), nodes.end(), 0);
  std::shuffle(nodes.begin(), nodes.end(), rng_);
  std::stable_sort(nodes.begin(), nodes.end(), [&](int a, int b) {
    if (IsIONode(a) != IsIONode(b)) return IsIONode(a) > IsIONode(b);
    return degree_[a] > degree_[b];
  });
  for (int node : nodes) {
    auto cells = CompatibleCells(node, state);
    if (cells.empty()) return std::nullopt;
    std::uniform_int_distribution<int> dist(0,
                                            static_cast<int>(cells.size()) - 1);
    PlaceNode(node, cells[dist(rng_)], state);
  }
  return state;
}

int Placement2DArrayEngine::OtherEndpoint(int edge_id, int node) const {
  const auto& edge = edges_[edge_id];
  if (edge.source == node) return edge.target;
  if (edge.target == node) return edge.source;
  return -1;
}

std::vector<int> Placement2DArrayEngine::LowBandwidthNodeOrder() {
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
        const int v = OtherEndpoint(edge_id, u);
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
        const int v = OtherEndpoint(edge_id, u);
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
                  result = std::max(result, degree_[node]);
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
      if (degree_[node] > degree_[highest_degree] ||
          (degree_[node] == degree_[highest_degree] && node < highest_degree)) {
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
          if (degree_[a] != degree_[b]) return degree_[a] < degree_[b];
          return a < b;
        });

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
        const int v = OtherEndpoint(edge_id, u);
        if (v < 0 || !in_component[v] || labeled[v]) continue;
        labeled[v] = 1;
        neighbors.push_back(v);
      }
      std::sort(neighbors.begin(), neighbors.end(), [&](int a, int b) {
        if (degree_[a] != degree_[b]) return degree_[a] < degree_[b];
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
Placement2DArrayEngine::ConstructPRISAInitialPlacementFromOrder(
    const std::vector<int>& node_order) {
  PlacementState state;
  state.dfg_to_cell.assign(dfg_.GetNodeNum(), -1);
  state.cell_to_dfg.assign(rows_ * cols_, -1);
  for (int label = 0; label < static_cast<int>(node_order.size()); label++) {
    const int node = node_order[label];
    int best_cell = -1;
    int best_distance = std::numeric_limits<int>::max();
    for (int order_index = 0;
         order_index < static_cast<int>(prisa_cell_order_.size());
         order_index++) {
      const int cell = prisa_cell_order_[order_index];
      if (!CanPlace(node, cell, state)) continue;
      const int label_distance = std::abs(order_index - label);
      if (label_distance < best_distance) {
        best_distance = label_distance;
        best_cell = cell;
      }
    }
    if (best_cell < 0) return std::nullopt;
    PlaceNode(node, best_cell, state);
  }
  return state;
}

std::optional<PlacementState>
Placement2DArrayEngine::ConstructPRISAInitialPlacement() {
  if (!UsesPRISASIS()) return RandomPlacement();
  return ConstructPRISAInitialPlacementFromOrder(LowBandwidthNodeOrder());
}

}  // namespace mapper::detail::placement2d
