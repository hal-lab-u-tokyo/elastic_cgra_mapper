#include "engine.hpp"

namespace mapper::detail::placement2d_yott_core_repair_array {

double Placement2DYOTTCoreRepairEngine::PlacementCost(
    const PlacementState& state) const {
  double cost = 0.0;
  for (const auto& edge : edges_) {
    const int from = state.dfg_to_cell[edge.source];
    const int to = state.dfg_to_cell[edge.target];
    if (from < 0 || to < 0) return kImpossibleCost;
    cost += DistanceCost(from, to);
  }
  return cost;
}

CostAwarePRISAMetrics
Placement2DYOTTCoreRepairEngine::ComputeCostAwarePRISAMetrics(
    const PlacementState& state) const {
  CostAwarePRISAMetrics metrics;
  const int node_count = dfg_.GetNodeNum();
  std::vector<int> indegree(node_count, 0);
  std::vector<std::vector<std::pair<int, int>>> weighted_successors(node_count);

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
    weighted_successors[edge.source].push_back({edge.target, std::max(1, hop)});
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
      metrics.mapped_lp_mesh_hop = std::max(metrics.mapped_lp_mesh_hop, value);
    }
  }
  return metrics;
}

void Placement2DYOTTCoreRepairEngine::PlaceNode(int dfg_node, int cell,
                                                PlacementState& state) const {
  state.dfg_to_cell[dfg_node] = cell;
  state.cell_to_dfg[cell] = dfg_node;
}

}  // namespace mapper::detail::placement2d_yott_core_repair_array
