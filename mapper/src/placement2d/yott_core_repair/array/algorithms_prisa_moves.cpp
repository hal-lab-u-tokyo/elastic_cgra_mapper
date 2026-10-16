#include "engine.hpp"

namespace mapper::detail::placement2d_yott_core_repair_array {

// PR/WR region accounting and local swap deltas.

std::vector<int> Placement2DYOTTCoreRepairEngine::AffectedEdgesForCellPair(
    const PlacementState& state, int cell_a, int cell_b) const {
  std::vector<int> result;
  const int node_a = state.cell_to_dfg[cell_a];
  const int node_b = state.cell_to_dfg[cell_b];
  if (node_a >= 0) {
    result.insert(result.end(), incident_edge_ids_[node_a].begin(),
                  incident_edge_ids_[node_a].end());
  }
  if (node_b >= 0) {
    result.insert(result.end(), incident_edge_ids_[node_b].begin(),
                  incident_edge_ids_[node_b].end());
  }
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}

int Placement2DYOTTCoreRepairEngine::CellAfterSwap(int cell, int first_cell,
                                                   int second_cell) const {
  if (cell == first_cell) return second_cell;
  if (cell == second_cell) return first_cell;
  return cell;
}

bool Placement2DYOTTCoreRepairEngine::CanSwapCells(const PlacementState& state,
                                                   int first_cell,
                                                   int second_cell) const {
  if (first_cell == second_cell || first_cell < 0 || second_cell < 0) {
    return false;
  }
  const int first_node = state.cell_to_dfg[first_cell];
  const int second_node = state.cell_to_dfg[second_cell];
  if (first_node < 0 && second_node < 0) return false;

  // Structural-I/O placement assigns source/sink nodes to logical I/O cells
  // and all other nodes to compute cells. IsCompatible() intentionally skips
  // that logical type check, so every move must restore it explicitly.
  const auto can_occupy = [this](int node, int cell) {
    if (node < 0) return true;
    if (UsesStructuralIOCellTypes()) {
      return IsCPUMappingCompatible(node, cell);
    }
    return IsCompatible(node, cell);
  };
  if (!can_occupy(first_node, second_cell)) {
    return false;
  }
  if (!can_occupy(second_node, first_cell)) {
    return false;
  }
  return true;
}

bool Placement2DYOTTCoreRepairEngine::ApplyCellSwap(PlacementState& state,
                                                    int first_cell,
                                                    int second_cell) const {
  if (!CanSwapCells(state, first_cell, second_cell)) return false;
  const int first_node = state.cell_to_dfg[first_cell];
  const int second_node = state.cell_to_dfg[second_cell];
  state.cell_to_dfg[first_cell] = second_node;
  state.cell_to_dfg[second_cell] = first_node;
  if (first_node >= 0) state.dfg_to_cell[first_node] = second_cell;
  if (second_node >= 0) state.dfg_to_cell[second_node] = first_cell;
  return true;
}

// PRISA move proposal. The base variant tries to repair weak-region edges;
// cost-aware variants use additional direct-edge and hop-distance terms.

}  // namespace mapper::detail::placement2d_yott_core_repair_array
