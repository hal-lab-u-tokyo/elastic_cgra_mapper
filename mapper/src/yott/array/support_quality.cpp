#include "engine.hpp"

namespace mapper::detail::yott_array {

double Placement2DYOTTEngine::PlacementCost(const PlacementState& state) const {
  double cost = 0.0;
  for (const auto& edge : edges_) {
    const int from = state.dfg_to_cell[edge.source];
    const int to = state.dfg_to_cell[edge.target];
    if (from < 0 || to < 0) return kImpossibleCost;
    cost += DistanceCost(from, to);
  }
  return cost;
}

int Placement2DYOTTEngine::BorderDistance(int cell) const {
  return std::min(std::min(Row(cell), rows_ - 1 - Row(cell)),
                  std::min(Col(cell), cols_ - 1 - Col(cell)));
}

std::vector<int> Placement2DYOTTEngine::CompatibleCells(
    int dfg_node, const PlacementState& state) const {
  std::vector<int> cells;
  cells.reserve(compatible_cells_[dfg_node].size());
  for (int cell : compatible_cells_[dfg_node]) {
    if (CanPlace(dfg_node, cell, state)) cells.push_back(cell);
  }
  return cells;
}

void Placement2DYOTTEngine::PlaceNode(int dfg_node, int cell,
                                      PlacementState& state) const {
  state.dfg_to_cell[dfg_node] = cell;
  state.cell_to_dfg[cell] = dfg_node;
}

}  // namespace mapper::detail::yott_array
