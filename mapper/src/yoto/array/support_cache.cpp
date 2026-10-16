#include "engine.hpp"

namespace mapper::detail::yoto_array {

void Placement2DYOTOEngine::BuildDFGCache() {
  const int node_num = dfg_.GetNodeNum();
  successors_.assign(node_num, {});
  predecessors_.assign(node_num, {});
  incident_edge_ids_.assign(node_num, {});
  degree_.assign(node_num, 0);
  edges_.clear();
  for (int edge_id = 0; edge_id < dfg_.GetEdgeNum(); edge_id++) {
    const auto [source, target] = dfg_.GetEdgeSourceTarget(edge_id);
    const int local_edge_id = static_cast<int>(edges_.size());
    edges_.push_back({source, target});
    if (source >= 0 && source < node_num) {
      successors_[source].push_back(target);
      incident_edge_ids_[source].push_back(local_edge_id);
    }
    if (target >= 0 && target < node_num && target != source) {
      predecessors_[target].push_back(source);
      incident_edge_ids_[target].push_back(local_edge_id);
    }
  }
  for (int node = 0; node < node_num; node++) {
    degree_[node] =
        static_cast<int>(successors_[node].size() + predecessors_[node].size());
  }
}

void Placement2DYOTOEngine::BuildGridCache() {
  rows_ = config_.row;
  cols_ = config_.column;
  cell_to_mrrg_.assign(rows_ * cols_, -1);
  cell_ops_.assign(rows_ * cols_, {});
  cell_memory_accessible_.assign(rows_ * cols_, false);
  for (int node_id = 0; node_id < mrrg_.GetNodeNum(); node_id++) {
    const auto property = mrrg_.GetNodeProperty(node_id);
    if (property.context_id != 0) continue;
    const int row = property.position_id.first;
    const int col = property.position_id.second;
    if (row < 0 || row >= rows_ || col < 0 || col >= cols_) continue;
    const int cell = Cell(row, col);
    cell_to_mrrg_[cell] = node_id;
    cell_ops_[cell] = property.supported_operations;
    cell_memory_accessible_[cell] = property.is_memory_accessible;
  }
  bool has_io_cell = false;
  bool has_compute_cell = false;
  for (int cell = 0; cell < rows_ * cols_; cell++) {
    if (cell_to_mrrg_[cell] < 0) continue;
    has_io_cell = has_io_cell || cell_memory_accessible_[cell];
    has_compute_cell = has_compute_cell || !cell_memory_accessible_[cell];
  }
  separate_io_cells_ = has_io_cell && has_compute_cell;

  // Cell connectivity depends only on the architecture. Cache it once rather
  // than rebuilding the same all-pairs table for every placement trial.
  initial_freedom_.assign(rows_ * cols_, 0);
  for (int cell = 0; cell < rows_ * cols_; cell++) {
    if (cell_to_mrrg_[cell] >= 0) initial_freedom_[cell] = CellFreedom(cell);
  }
}

void Placement2DYOTOEngine::BuildCompatibilityCache() {
  compatible_cells_.assign(dfg_.GetNodeNum(), {});
  for (int node = 0; node < dfg_.GetNodeNum(); node++) {
    auto& cells = compatible_cells_[node];
    cells.reserve(cell_to_mrrg_.size());
    for (int cell = 0; cell < static_cast<int>(cell_to_mrrg_.size()); cell++) {
      if (IsCompatible(node, cell)) cells.push_back(cell);
    }
  }
}

std::string Placement2DYOTOEngine::NormalizePolicy(std::string policy) {
  std::transform(
      policy.begin(), policy.end(), policy.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return policy;
}

int Placement2DYOTOEngine::Cell(int row, int col) const {
  return row * cols_ + col;
}

int Placement2DYOTOEngine::Row(int cell) const { return cell / cols_; }

int Placement2DYOTOEngine::Col(int cell) const { return cell % cols_; }

bool Placement2DYOTOEngine::IsCompatible(int dfg_node, int cell) const {
  if (cell < 0 || cell >= static_cast<int>(cell_to_mrrg_.size())) return false;
  if (cell_to_mrrg_[cell] < 0) return false;
  if (UsesStructuralIOCellTypes()) {
    return true;
  }
  const entity::OpType op = dfg_.GetNodeProperty(dfg_node).op;
  return std::find(cell_ops_[cell].begin(), cell_ops_[cell].end(), op) !=
         cell_ops_[cell].end();
}

bool Placement2DYOTOEngine::IsIONode(int dfg_node) const {
  if (io_node_policy_ == "structural") {
    return predecessors_[dfg_node].empty() || successors_[dfg_node].empty();
  }
  const entity::OpType op = dfg_.GetNodeProperty(dfg_node).op;
  return entity::IsMemoryAccessOperation(op);
}

bool Placement2DYOTOEngine::CanPlace(int dfg_node, int cell,
                                     const PlacementState& state) const {
  if (state.cell_to_dfg[cell] >= 0 || !IsCompatible(dfg_node, cell)) {
    return false;
  }
  if (UsesStructuralIOCellTypes()) {
    const int cell_type = CPUMappingCellType(cell);
    return cell_type >= 0 && cell_type == (IsIONode(dfg_node) ? 1 : 0);
  }
  if (separate_io_cells_ &&
      cell_memory_accessible_[cell] != IsIONode(dfg_node)) {
    return false;
  }
  return true;
}

int Placement2DYOTOEngine::CPUMappingCellType(int cell) const {
  if (cell < 0 || cell >= static_cast<int>(cell_to_mrrg_.size())) return -1;
  if (cell_to_mrrg_[cell] < 0) return -1;
  const int row = Row(cell);
  const int col = Col(cell);
  const bool is_corner =
      (row == 0 || row == rows_ - 1) && (col == 0 || col == cols_ - 1);
  if (separate_io_cells_) {
    // Structural-I/O YOTO/YOTT uses a logical type matrix: perimeter cells
    // host source/sink-like nodes and interior cells host compute nodes.
    // `perimeter_no_corners` still creates corner MRRG nodes, but the
    // paper-style grid treats those corner sites as unavailable.
    if (is_corner && !cell_memory_accessible_[cell]) return -1;
    return cell_memory_accessible_[cell] ? 1 : 0;
  }
  if (is_corner) return -1;
  const bool is_perimeter =
      row == 0 || col == 0 || row == rows_ - 1 || col == cols_ - 1;
  return is_perimeter ? 1 : 0;
}

bool Placement2DYOTOEngine::IsCPUMappingCompatible(int dfg_node,
                                                   int cell) const {
  if (!IsCompatible(dfg_node, cell)) return false;
  const int cell_type = CPUMappingCellType(cell);
  if (cell_type < 0) return false;
  return cell_type == (IsIONode(dfg_node) ? 1 : 0);
}

bool Placement2DYOTOEngine::CanPlaceCPUMapping(
    int dfg_node, int cell, const PlacementState& state) const {
  return cell >= 0 && cell < static_cast<int>(state.cell_to_dfg.size()) &&
         state.cell_to_dfg[cell] < 0 && IsCPUMappingCompatible(dfg_node, cell);
}

int Placement2DYOTOEngine::CellFreedom(int cell) const {
  int result = 0;
  const int radius =
      config_.network_type == entity::MRRGNetworkType::kOneHopAxis2 ? 2 : 1;
  const int row = Row(cell);
  const int col = Col(cell);
  for (int dr = -radius; dr <= radius; dr++) {
    for (int dc = -radius; dc <= radius; dc++) {
      if (dr == 0 && dc == 0) continue;
      const int other_row = row + dr;
      const int other_col = col + dc;
      if (other_row < 0 || other_col < 0 || other_row >= rows_ ||
          other_col >= cols_) {
        continue;
      }
      const int other = Cell(other_row, other_col);
      if (cell_to_mrrg_[other] < 0) continue;
      if (DistanceCost(cell, other) == 1) result++;
    }
  }
  return result;
}

std::vector<int> Placement2DYOTOEngine::InitialFreedomGrid() const {
  return initial_freedom_;
}

int Placement2DYOTOEngine::DistanceCost(int a, int b) const {
  const int dx = std::abs(Row(a) - Row(b));
  const int dy = std::abs(Col(a) - Col(b));
  if (config_.network_type == entity::MRRGNetworkType::kOneHopAxis2) {
    return std::max(1, (dx + 1) / 2 + (dy + 1) / 2);
  }
  if (config_.network_type == entity::MRRGNetworkType::kDiagonal) {
    return std::max(1, std::max(dx, dy));
  }
  return std::max(1, dx + dy);
}

}  // namespace mapper::detail::yoto_array
