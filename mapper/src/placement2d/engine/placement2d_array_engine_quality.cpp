#include "placement2d_array_engine_internal.hpp"

namespace mapper::detail::placement2d {

double Placement2DArrayEngine::PlacementCost(const PlacementState& state) const {
  double cost = 0.0;
  for (const auto& edge : edges_) {
    const int from = state.dfg_to_cell[edge.source];
    const int to = state.dfg_to_cell[edge.target];
    if (from < 0 || to < 0) return kImpossibleCost;
    cost += DistanceCost(from, to);
  }
  return cost;
}

PlacementQuality Placement2DArrayEngine::ComputePlacementQuality(const PlacementState& state) const {
  PlacementQuality quality;
  quality.cost = 0.0;
  quality.direct_edges = 0;
  quality.max_distance = 0;
  for (const auto& edge : edges_) {
    const int from = state.dfg_to_cell[edge.source];
    const int to = state.dfg_to_cell[edge.target];
    if (from < 0 || to < 0) {
      quality.cost = kImpossibleCost;
      return quality;
    }
    const int distance = DistanceCost(from, to);
    quality.cost += distance;
    if (distance <= 1) quality.direct_edges++;
    quality.max_distance = std::max(quality.max_distance, distance);
  }
  return quality;
}

bool Placement2DArrayEngine::IsBetterQuality(const PlacementQuality& candidate,
                     const PlacementQuality& best) const {
  if (candidate.cost + 1.0e-9 < best.cost) return true;
  if (candidate.cost > best.cost + 1.0e-9) return false;
  if (candidate.direct_edges != best.direct_edges &&
      candidate.max_distance <= best.max_distance) {
    return candidate.direct_edges > best.direct_edges;
  }
  if (candidate.max_distance != best.max_distance) {
    return candidate.max_distance < best.max_distance;
  }
  return candidate.direct_edges > best.direct_edges;
}

int Placement2DArrayEngine::PRISAWeakEdgeCount(const PlacementState& state) const {
  int result = 0;
  for (int edge_id = 0; edge_id < static_cast<int>(edges_.size());
       edge_id++) {
    bool is_weak = false;
    bool is_potential = false;
    if (PRISAEdgeRegion(state, edge_id, &is_weak, &is_potential) &&
        is_weak) {
      result++;
    }
  }
  return result;
}

CostAwarePRISAMetrics Placement2DArrayEngine::ComputeCostAwarePRISAMetrics(
    const PlacementState& state) const {
  CostAwarePRISAMetrics metrics;
  const int node_count = dfg_.GetNodeNum();
  std::vector<int> indegree(node_count, 0);
  std::vector<std::vector<std::pair<int, int>>> weighted_successors(
      node_count);

  for (const auto& edge : edges_) {
    const int from = state.dfg_to_cell[edge.source];
    const int to = state.dfg_to_cell[edge.target];
    if (from < 0 || to < 0) {
      metrics.mesh_hop_sum = kImpossibleCost;
      metrics.max_mesh_hop = std::numeric_limits<int>::max();
      metrics.mapped_lp_mesh_hop = std::numeric_limits<int>::max();
      return metrics;
    }
    const int hop = DistanceCost(from, to);
    metrics.mesh_hop_sum += hop;
    metrics.max_mesh_hop = std::max(metrics.max_mesh_hop, hop);
    if (hop <= 1) metrics.direct_edge_count++;
    weighted_successors[edge.source].push_back(
        {edge.target, std::max(1, hop)});
    indegree[edge.target]++;
  }

  std::queue<int> q;
  std::vector<int> distance(node_count, 0);
  for (int node = 0; node < node_count; node++) {
    if (indegree[node] == 0) q.push(node);
  }

  int visited = 0;
  while (!q.empty()) {
    const int source = q.front();
    q.pop();
    visited++;
    metrics.mapped_lp_mesh_hop =
        std::max(metrics.mapped_lp_mesh_hop, distance[source]);
    for (const auto& [target, weight] : weighted_successors[source]) {
      distance[target] = std::max(distance[target], distance[source] + weight);
      indegree[target]--;
      if (indegree[target] == 0) q.push(target);
    }
  }

  if (visited != node_count) {
    metrics.mapped_lp_mesh_hop = static_cast<int>(metrics.mesh_hop_sum);
  } else {
    for (int value : distance) {
      metrics.mapped_lp_mesh_hop =
          std::max(metrics.mapped_lp_mesh_hop, value);
    }
  }
  return metrics;
}

double Placement2DArrayEngine::CostAwarePRISAScore(const PlacementState& state) const {
  const int weak_edges = PRISAWeakEdgeCount(state);
  const CostAwarePRISAMetrics metrics = ComputeCostAwarePRISAMetrics(state);
  if (metrics.mesh_hop_sum >= kImpossibleCost ||
      metrics.max_mesh_hop == std::numeric_limits<int>::max() ||
      metrics.mapped_lp_mesh_hop == std::numeric_limits<int>::max()) {
    return kImpossibleCost;
  }
  return weak_edges * 1000000.0 + metrics.mesh_hop_sum * 50.0 +
         metrics.mapped_lp_mesh_hop * 100.0 +
         metrics.max_mesh_hop * 200.0 -
         metrics.direct_edge_count * 80.0;
}

double Placement2DArrayEngine::PRISAAcceptanceScore(const PlacementState& state) const {
  if (UsesCostAwarePRISA()) return CostAwarePRISAScore(state);
  return PlacementCost(state);
}

int Placement2DArrayEngine::BorderDistance(int cell) const {
  return std::min(std::min(Row(cell), rows_ - 1 - Row(cell)),
                  std::min(Col(cell), cols_ - 1 - Col(cell)));
}

std::vector<int> Placement2DArrayEngine::CompatibleCells(int dfg_node,
                                 const PlacementState& state) const {
  std::vector<int> cells;
  cells.reserve(compatible_cells_[dfg_node].size());
  for (int cell : compatible_cells_[dfg_node]) {
    if (CanPlace(dfg_node, cell, state)) cells.push_back(cell);
  }
  return cells;
}

void Placement2DArrayEngine::PlaceNode(int dfg_node, int cell, PlacementState& state) const {
  state.dfg_to_cell[dfg_node] = cell;
  state.cell_to_dfg[cell] = dfg_node;
}

}  // namespace mapper::detail::placement2d
