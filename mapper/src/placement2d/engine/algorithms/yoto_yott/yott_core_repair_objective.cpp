#include "../../array_engine_internal.hpp"

namespace mapper::detail::placement2d {

// Edge, FIFO, cut-demand, and local-swap objectives used by construction and
// bounded repair.

double Placement2DArrayEngine::CoreRepairEdgeWeight(int edge_id) const {
  if (edge_id < 0 || edge_id >= static_cast<int>(edges_.size())) {
    return 1.0;
  }
  const auto& edge = edges_[edge_id];
  const int max_critical = std::max(1, critical_path_max_);
  const double source_critical =
      static_cast<double>(critical_path_score_[edge.source]) /
      static_cast<double>(max_critical);
  const double target_critical =
      static_cast<double>(critical_path_score_[edge.target]) /
      static_cast<double>(max_critical);
  const double critical = std::max(source_critical, target_critical);
  const int fanout =
      std::max(0, static_cast<int>(successors_[edge.source].size()) - 1);
  return 1.0 + 1.6 * critical + 0.18 * static_cast<double>(fanout);
}

double Placement2DArrayEngine::CoreRepairEdgeCost(int source_cell,
                                                  int target_cell) const {
  return CoreRepairEdgeCost(-1, source_cell, target_cell);
}

double Placement2DArrayEngine::CoreRepairEdgeCost(int edge_id, int source_cell,
                                                  int target_cell) const {
  const int distance = DistanceCost(source_cell, target_cell);
  double score = static_cast<double>(distance);
  const int tail = std::max(0, distance - 1);
  score += 2.6 * static_cast<double>(tail * tail);
  if (distance > 1) score += 1.6;
  return CoreRepairEdgeWeight(edge_id) * score;
}

double Placement2DArrayEngine::CoreRepairCutScore(
    const PlacementState& state) const {
  if (!CoreRepairUsesCutDemand()) return 0.0;

  const auto [row_cut, col_cut] = CoreRepairCutDemand(state);
  if (row_cut.empty() && rows_ > 1) return kImpossibleCost;
  if (col_cut.empty() && cols_ > 1) return kImpossibleCost;

  double score = 0.0;
  constexpr double kSoftCapacity = 2.0;
  for (double demand : row_cut) {
    const double overflow = std::max(0.0, demand - kSoftCapacity);
    score += overflow * overflow;
  }
  for (double demand : col_cut) {
    const double overflow = std::max(0.0, demand - kSoftCapacity);
    score += overflow * overflow;
  }
  return score;
}

double Placement2DArrayEngine::CoreRepairFifoScore(
    const PlacementState& state) const {
  if (!CoreRepairUsesFifoScore()) return 0.0;

  double tail_sum = 0.0;
  double tail_square_sum = 0.0;
  int max_tail = 0;
  for (const auto& edge : edges_) {
    const int source_cell = state.dfg_to_cell[edge.source];
    const int target_cell = state.dfg_to_cell[edge.target];
    if (source_cell < 0 || target_cell < 0) return kImpossibleCost;
    const int tail = std::max(0, DistanceCost(source_cell, target_cell) - 1);
    tail_sum += static_cast<double>(tail);
    tail_square_sum += static_cast<double>(tail * tail);
    max_tail = std::max(max_tail, tail);
  }

  // Same tail definition as the paper-style FIFO metric in the reports:
  // direct edges cost zero; each extra segment adds one FIFO slot.
  return 1.6 * tail_sum + 1.2 * tail_square_sum +
         12.0 * static_cast<double>(max_tail);
}

std::pair<std::vector<double>, std::vector<double>>
Placement2DArrayEngine::CoreRepairCutDemand(const PlacementState& state) const {
  std::vector<double> row_cut(std::max(0, rows_ - 1), 0.0);
  std::vector<double> col_cut(std::max(0, cols_ - 1), 0.0);
  for (const auto& edge : edges_) {
    const int source_cell = state.dfg_to_cell[edge.source];
    const int target_cell = state.dfg_to_cell[edge.target];
    if (source_cell < 0 || target_cell < 0) return {{}, {}};

    const int r0 = Row(source_cell);
    const int r1 = Row(target_cell);
    const int c0 = Col(source_cell);
    const int c1 = Col(target_cell);
    for (int r = std::min(r0, r1); r < std::max(r0, r1); r++) {
      if (r >= 0 && r < static_cast<int>(row_cut.size())) row_cut[r] += 1.0;
    }
    for (int c = std::min(c0, c1); c < std::max(c0, c1); c++) {
      if (c >= 0 && c < static_cast<int>(col_cut.size())) col_cut[c] += 1.0;
    }
  }
  return {row_cut, col_cut};
}

double Placement2DArrayEngine::CoreRepairEdgePriority(
    const PlacementState& state, int edge_id,
    const std::vector<double>& row_cut,
    const std::vector<double>& col_cut) const {
  if (edge_id < 0 || edge_id >= static_cast<int>(edges_.size())) {
    return -1.0;
  }
  const auto& edge = edges_[edge_id];
  const int source_cell = state.dfg_to_cell[edge.source];
  const int target_cell = state.dfg_to_cell[edge.target];
  if (source_cell < 0 || target_cell < 0) return -1.0;

  const int distance = DistanceCost(source_cell, target_cell);
  double cut_demand = 0.0;
  const int r0 = Row(source_cell);
  const int r1 = Row(target_cell);
  const int c0 = Col(source_cell);
  const int c1 = Col(target_cell);
  for (int r = std::min(r0, r1); r < std::max(r0, r1); r++) {
    if (r >= 0 && r < static_cast<int>(row_cut.size())) {
      cut_demand += row_cut[r];
    }
  }
  for (int c = std::min(c0, c1); c < std::max(c0, c1); c++) {
    if (c >= 0 && c < static_cast<int>(col_cut.size())) {
      cut_demand += col_cut[c];
    }
  }

  const int max_critical = std::max(1, critical_path_max_);
  const double critical =
      static_cast<double>(std::max(critical_path_score_[edge.source],
                                   critical_path_score_[edge.target])) /
      static_cast<double>(max_critical);
  const int fanout =
      std::max(0, static_cast<int>(successors_[edge.source].size()) - 1);

  double priority = CoreRepairEdgeCost(edge_id, source_cell, target_cell);
  const int tail = std::max(0, distance - 1);
  priority += 80.0 * static_cast<double>(tail * tail);
  priority += 24.0 * static_cast<double>(tail);
  priority += 0.22 * cut_demand * static_cast<double>(std::max(1, distance));
  priority += 1.10 * critical;
  priority += 0.18 * static_cast<double>(fanout);
  return priority;
}

std::vector<int> Placement2DArrayEngine::CoreRepairEdgePool(
    const PlacementState& state) const {
  const auto [row_cut, col_cut] = CoreRepairCutDemand(state);
  std::vector<std::pair<double, int>> scored_edges;
  for (int edge_id = 0; edge_id < static_cast<int>(edges_.size()); edge_id++) {
    const double score =
        CoreRepairEdgePriority(state, edge_id, row_cut, col_cut);
    if (score >= 0.0) {
      scored_edges.push_back({score, edge_id});
    }
  }
  std::sort(scored_edges.begin(), scored_edges.end(),
            [](const auto& lhs, const auto& rhs) {
              if (std::abs(lhs.first - rhs.first) > 1.0e-12) {
                return lhs.first > rhs.first;
              }
              return lhs.second < rhs.second;
            });

  const int pool_size = CoreRepairEdgePoolSize();
  std::vector<int> edge_pool;
  for (int i = 0;
       i < std::min(pool_size, static_cast<int>(scored_edges.size())); i++) {
    edge_pool.push_back(scored_edges[i].second);
  }
  return edge_pool;
}

std::vector<int> Placement2DArrayEngine::CoreRepairCells(int center_cell,
                                                         int radius) const {
  std::vector<int> cells;
  if (center_cell < 0 || center_cell >= rows_ * cols_ || radius < 0) {
    return cells;
  }

  const int center_row = Row(center_cell);
  const int center_col = Col(center_cell);
  for (int dr = -radius; dr <= radius; dr++) {
    const int dc_abs = radius - std::abs(dr);
    const std::array<int, 2> dcs = {-dc_abs, dc_abs};
    for (int dc : dcs) {
      const int row = center_row + dr;
      const int col = center_col + dc;
      if (row < 0 || row >= rows_ || col < 0 || col >= cols_) continue;
      cells.push_back(Cell(row, col));
      if (dc_abs == 0) break;
    }
  }
  return cells;
}

double Placement2DArrayEngine::CoreRepairLocalSwapDelta(
    const PlacementState& state, int first_cell, int second_cell,
    const std::vector<int>& affected_edges) const {
  if (affected_edges.empty()) return kImpossibleCost;

  double edge_delta = 0.0;
  double fifo_delta = 0.0;
  int direct_delta = 0;
  int before_max_tail = 0;
  int after_max_tail = 0;
  for (int edge_id : affected_edges) {
    const auto& edge = edges_[edge_id];
    const int source_cell = state.dfg_to_cell[edge.source];
    const int target_cell = state.dfg_to_cell[edge.target];
    const int next_source = CellAfterSwap(source_cell, first_cell, second_cell);
    const int next_target = CellAfterSwap(target_cell, first_cell, second_cell);

    const int before = DistanceCost(source_cell, target_cell);
    const int after = DistanceCost(next_source, next_target);
    edge_delta += CoreRepairEdgeCost(edge_id, next_source, next_target) -
                  CoreRepairEdgeCost(edge_id, source_cell, target_cell);

    const int before_tail = std::max(0, before - 1);
    const int after_tail = std::max(0, after - 1);
    before_max_tail = std::max(before_max_tail, before_tail);
    after_max_tail = std::max(after_max_tail, after_tail);
    if (CoreRepairUsesFifoScore()) {
      fifo_delta += 1.6 * static_cast<double>(after_tail - before_tail);
      fifo_delta += 1.2 * static_cast<double>(after_tail * after_tail -
                                              before_tail * before_tail);
    }
    if (before <= 1 && after > 1) direct_delta--;
    if (before > 1 && after <= 1) direct_delta++;
  }

  // PRISA-style local acceptance: only edges incident to the swapped cells
  // decide the move. This keeps repair cheap and avoids a global rescore per
  // candidate, while preserving direct edges unless the local tail gain is
  // large.
  double delta = edge_delta + fifo_delta;
  delta += 18.0 * static_cast<double>(after_max_tail - before_max_tail);
  delta -= 4.0 * static_cast<double>(direct_delta);
  if (direct_delta < 0) delta += 24.0 * static_cast<double>(-direct_delta);
  return delta;
}

double Placement2DArrayEngine::CoreRepairScore(
    const PlacementState& state) const {
  double score = 0.0;
  for (int edge_id = 0; edge_id < static_cast<int>(edges_.size()); edge_id++) {
    const auto& edge = edges_[edge_id];
    const int source_cell = state.dfg_to_cell[edge.source];
    const int target_cell = state.dfg_to_cell[edge.target];
    if (source_cell < 0 || target_cell < 0) return kImpossibleCost;
    score += CoreRepairEdgeCost(edge_id, source_cell, target_cell);
  }

  if (CoreRepairUsesCutDemand()) {
    const CostAwarePRISAMetrics metrics = ComputeCostAwarePRISAMetrics(state);
    if (metrics.mesh_hop_sum >= kImpossibleCost) return kImpossibleCost;
    score += 0.18 * CoreRepairCutScore(state);
    score += 0.28 * static_cast<double>(metrics.mapped_lp_mesh_hop);
    score += 1.25 * static_cast<double>(metrics.max_mesh_hop);
  }
  score += CoreRepairFifoScore(state);
  return score;
}

}  // namespace mapper::detail::placement2d
