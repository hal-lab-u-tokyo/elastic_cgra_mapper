#include "../../array_engine_internal.hpp"

namespace mapper::detail::placement2d {
namespace {

void PushUniqueCell(std::vector<std::pair<int, int>>& cells, int freedom,
                    int cell) {
  for (const auto& existing : cells) {
    if (existing.second == cell) return;
  }
  cells.push_back({freedom, cell});
}

}  // namespace

// Candidate generation and ranking for the constructive pass.

std::vector<std::pair<int, int>>
Placement2DArrayEngine::CoreRepairCandidateCells(
    const Step& step, int dfg_node, int anchor_cell,
    const CPUMappingPlacementContext& context) {
  std::vector<std::pair<int, int>> candidates;

  if (anchor_cell >= 0) {
    for (const auto& cell :
         CPUMappingTipCells(anchor_cell, dfg_node, context)) {
      PushUniqueCell(candidates, cell.first, cell.second);
    }
  }

  for (const auto& annotation : step.annotations) {
    if (annotation.kind != StepAnnotationKind::kReconvergence) continue;
    if (annotation.distance > 2) continue;
    const int annotated_cell =
        context.state.dfg_to_cell[annotation.anchor_node];
    if (annotated_cell < 0) continue;
    auto cells = CPUMappingCellsWithinAnnotatedDistance(
        annotated_cell, annotation.distance, context);
    for (const auto& cell : cells) {
      PushUniqueCell(candidates, cell.first, cell.second);
    }
  }

  int neighbor_count = 0;
  int row_sum = 0;
  int col_sum = 0;
  for (int edge_id : incident_edge_ids_[dfg_node]) {
    const auto& edge = edges_[edge_id];
    const int other = edge.source == dfg_node ? edge.target : edge.source;
    const int other_cell = context.state.dfg_to_cell[other];
    if (other_cell < 0) continue;
    neighbor_count++;
    row_sum += Row(other_cell);
    col_sum += Col(other_cell);
  }

  if (neighbor_count > 0) {
    const int center_row = std::clamp(
        static_cast<int>(std::lround(static_cast<double>(row_sum) /
                                     static_cast<double>(neighbor_count))),
        0, rows_ - 1);
    const int center_col = std::clamp(
        static_cast<int>(std::lround(static_cast<double>(col_sum) /
                                     static_cast<double>(neighbor_count))),
        0, cols_ - 1);
    const int center_cell = Cell(center_row, center_col);

    std::vector<std::pair<int, int>> nearest;
    for (int cell : CPUMappingCompatibleCells(dfg_node, context.state)) {
      nearest.push_back({DistanceCost(center_cell, cell), cell});
    }
    std::sort(nearest.begin(), nearest.end());
    const int limit = std::min(12, static_cast<int>(nearest.size()));
    for (int i = 0; i < limit; i++) {
      const int cell = nearest[i].second;
      PushUniqueCell(candidates, context.freedom[cell], cell);
    }
  }

  if (candidates.empty()) {
    for (const auto& cell :
         CPUMappingAllCompatibleCellPairs(dfg_node, context)) {
      PushUniqueCell(candidates, cell.first, cell.second);
    }
  }

  return candidates;
}

double Placement2DArrayEngine::CoreRepairCandidateScore(
    const Step& step, int dfg_node, int candidate_cell,
    const CPUMappingPlacementContext& context) const {
  double score = 0.0;
  int placed_edges = 0;
  int direct_edges = 0;

  for (int edge_id : incident_edge_ids_[dfg_node]) {
    const auto& edge = edges_[edge_id];
    const int other = edge.source == dfg_node ? edge.target : edge.source;
    const int other_cell = context.state.dfg_to_cell[other];
    if (other_cell < 0) continue;
    placed_edges++;
    score += CoreRepairEdgeCost(edge_id, candidate_cell, other_cell);
    if (DistanceCost(candidate_cell, other_cell) <= 1) direct_edges++;

    if (CoreRepairUsesCutDemand()) {
      score += 0.12 * static_cast<double>(
                          std::abs(Row(candidate_cell) - Row(other_cell)) +
                          std::abs(Col(candidate_cell) - Col(other_cell)));
    }
    if (CoreRepairUsesFifoScore()) {
      const int tail =
          std::max(0, DistanceCost(candidate_cell, other_cell) - 1);
      score += 2.4 * static_cast<double>(tail) +
               1.2 * static_cast<double>(tail * tail);
    }
  }

  for (const auto& annotation : step.annotations) {
    if (annotation.kind != StepAnnotationKind::kReconvergence) continue;
    const int anchor_cell = context.state.dfg_to_cell[annotation.anchor_node];
    if (anchor_cell < 0) continue;
    const int distance = DistanceCost(candidate_cell, anchor_cell);
    score += 0.75 * std::abs(distance - annotation.distance);
  }

  const double freedom_bonus =
      candidate_cell >= 0 &&
              candidate_cell < static_cast<int>(context.freedom.size())
          ? static_cast<double>(context.freedom[candidate_cell])
          : 0.0;
  score -= 0.2 * static_cast<double>(direct_edges);
  score -= 0.035 * freedom_bonus;
  score -= 0.06 * static_cast<double>(placed_edges);
  score += static_cast<double>(candidate_cell % 997) * 1.0e-6;
  return score;
}

int Placement2DArrayEngine::ChooseCoreRepairCell(
    const std::vector<std::pair<int, int>>& cells, const Step& step,
    int dfg_node, const CPUMappingPlacementContext& context) {
  std::vector<int> valid_cells;
  for (const auto& [unused_freedom, cell] : cells) {
    (void)unused_freedom;
    if (CanPlaceCPUMapping(dfg_node, cell, context.state)) {
      valid_cells.push_back(cell);
    }
  }
  if (valid_cells.empty()) return -1;
  RecordPlacementSwapAttempts(static_cast<long long>(valid_cells.size()));

  int best_cell = -1;
  double best_score = kImpossibleCost;
  for (int cell : valid_cells) {
    const double score =
        CoreRepairCandidateScore(step, dfg_node, cell, context);
    if (score < best_score) {
      best_score = score;
      best_cell = cell;
    }
  }
  return best_cell;
}

bool Placement2DArrayEngine::PlaceCoreRepairStep(
    const Step& step, CPUMappingPlacementContext& context) {
  const int dfg_node = step.target;
  if (context.state.dfg_to_cell[dfg_node] >= 0) return true;

  const int anchor_cell = context.state.dfg_to_cell[step.anchor];
  if (anchor_cell < 0) {
    return PlaceCPUMappingInitialNode(dfg_node, context);
  }

  auto candidates =
      CoreRepairCandidateCells(step, dfg_node, anchor_cell, context);
  const int cell = ChooseCoreRepairCell(candidates, step, dfg_node, context);
  if (cell >= 0) {
    PlaceCPUMappingNode(dfg_node, cell, context);
    return true;
  }
  return TryCPUMappingAdjacency(dfg_node, anchor_cell, context, true);
}

}  // namespace mapper::detail::placement2d
