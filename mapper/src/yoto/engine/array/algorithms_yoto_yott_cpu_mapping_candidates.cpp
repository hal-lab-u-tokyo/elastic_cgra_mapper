#include <mapper/yoto/engine/array/yoto_array_engine.hpp>

namespace mapper::yoto_array {

// Candidate cells and local placement primitives.

std::vector<int> Placement2DYOTOEngine::CPUMappingCompatibleCells(
    int dfg_node, const PlacementState& state) const {
  std::vector<int> result;
  for (int cell : compatible_cells_[dfg_node]) {
    if (!CanPlaceCPUMapping(dfg_node, cell, state)) continue;
    result.push_back(cell);
  }
  return result;
}

std::vector<std::pair<int, int>>
Placement2DYOTOEngine::CPUMappingAllCompatibleCellPairs(
    int dfg_node, const CPUMappingPlacementContext& context) const {
  std::vector<std::pair<int, int>> result;
  for (int cell : CPUMappingCompatibleCells(dfg_node, context.state)) {
    result.push_back({context.freedom[cell], cell});
  }
  return result;
}

std::vector<int> Placement2DYOTOEngine::CPUMappingIOCellsForInitialPlacement(
    int dfg_node, const PlacementState& state) const {
  std::vector<int> result;
  const int wanted_type = IsIONode(dfg_node) ? 1 : 0;
  for (int cell = 0; cell < rows_ * cols_; cell++) {
    if (CPUMappingCellType(cell) != wanted_type) continue;
    if (!CanPlaceCPUMapping(dfg_node, cell, state)) continue;
    result.push_back(cell);
  }
  return result;
}

int Placement2DYOTOEngine::ChooseCPUMappingInitialCell(
    int dfg_node, const PlacementState& state) {
  std::vector<int> candidates =
      CPUMappingIOCellsForInitialPlacement(dfg_node, state);
  if (candidates.empty()) {
    candidates = CPUMappingCompatibleCells(dfg_node, state);
  }
  if (candidates.empty()) return -1;
  RecordPlacementSwapAttempts(static_cast<long long>(candidates.size()));
  return candidates[AuthorRandomInt(static_cast<int>(candidates.size()))];
}

std::vector<std::pair<int, int>> Placement2DYOTOEngine::CPUMappingTipCells(
    int anchor_cell, int dfg_node,
    const CPUMappingPlacementContext& context) const {
  static constexpr std::array<std::pair<int, int>, 8> kOneHopTips = {
      std::pair<int, int>{0, 1},
      {0, 2},
      {1, 0},
      {2, 0},
      {0, -1},
      {0, -2},
      {-1, 0},
      {-2, 0}};
  static constexpr std::array<std::pair<int, int>, 4> kMeshTips = {
      std::pair<int, int>{0, 1}, {1, 0}, {0, -1}, {-1, 0}};

  std::vector<std::pair<int, int>> result;
  const bool one_hop =
      config_.network_type == entity::MRRGNetworkType::kOneHop2d;
  const int anchor_row = Row(anchor_cell);
  const int anchor_col = Col(anchor_cell);
  const auto push_if_valid = [&](int dr, int dc) {
    const int row = anchor_row + dr;
    const int col = anchor_col + dc;
    // The public get_tips() uses strict lower bounds and non-strict upper
    // grid bounds; this keeps its top/left-border behavior.
    if (row <= 0 || col <= 0 || row >= rows_ || col >= cols_) return;
    const int cell = Cell(row, col);
    if (cell < 0 || cell >= static_cast<int>(context.freedom.size())) return;
    if (context.freedom[cell] <= 0) return;
    if (!CanPlaceCPUMapping(dfg_node, cell, context.state)) return;
    result.push_back({context.freedom[cell], cell});
  };
  if (one_hop) {
    for (const auto& [dr, dc] : kOneHopTips) push_if_valid(dr, dc);
  } else {
    for (const auto& [dr, dc] : kMeshTips) push_if_valid(dr, dc);
  }
  return result;
}

int Placement2DYOTOEngine::BestCPUMappingDegreeCell(
    const std::vector<std::pair<int, int>>& cells, int dfg_node,
    const CPUMappingPlacementContext& context) {
  if (cells.empty()) return -1;
  RecordPlacementSwapAttempts(static_cast<long long>(cells.size()));
  std::vector<std::pair<int, int>> valid_cells;
  valid_cells.reserve(cells.size());
  for (const auto& cell : cells) {
    if (CanPlaceCPUMapping(dfg_node, cell.second, context.state)) {
      valid_cells.push_back(cell);
    }
  }
  if (valid_cells.empty()) return -1;

  if (candidate_rank_policy_ == "random") {
    return valid_cells[AuthorRandomInt(static_cast<int>(valid_cells.size()))]
        .second;
  }
  if (candidate_rank_policy_ == "random_mt19937") {
    // Match YOTT Core's candidate stream in paired mechanism experiments.
    std::uniform_int_distribution<int> dist(
        0, static_cast<int>(valid_cells.size()) - 1);
    return valid_cells[dist(rng_)].second;
  }
  if (candidate_rank_policy_ == "high_freedom") {
    return std::max_element(valid_cells.begin(), valid_cells.end(),
                            [](const auto& a, const auto& b) {
                              if (a.first != b.first) return a.first < b.first;
                              return a.second > b.second;
                            })
        ->second;
  }
  if (candidate_rank_policy_ == "low_freedom") {
    return std::min_element(valid_cells.begin(), valid_cells.end(),
                            [](const auto& a, const auto& b) {
                              if (a.first != b.first) return a.first < b.first;
                              return a.second < b.second;
                            })
        ->second;
  }

  int best_degree = valid_cells.front().first;
  int best_cell = valid_cells.front().second;
  const int node_degree = context.mutable_degree[dfg_node];
  for (int i = 1; i < static_cast<int>(valid_cells.size()); i++) {
    const int available = valid_cells[i].first;
    const int cell = valid_cells[i].second;
    if (node_degree > 3) {
      // Degree matching compares candidate freedom. The released code compared
      // the cell index here, which makes the choice depend on numbering.
      if (available > best_degree) {
        best_degree = available;
        best_cell = cell;
      }
    } else {
      if (best_degree > available && node_degree >= available) {
        best_degree = available;
        best_cell = cell;
      }
    }
  }
  return best_cell;
}

void Placement2DYOTOEngine::PlaceCPUMappingNode(
    int dfg_node, int cell, CPUMappingPlacementContext& context) const {
  PlaceNode(dfg_node, cell, context.state);
  if (dfg_node >= 0 &&
      dfg_node < static_cast<int>(context.mutable_degree.size())) {
    context.mutable_degree[dfg_node]--;
  }
  UpdateCPUMappingFreedomGrid(cell, context.freedom);
}

bool Placement2DYOTOEngine::PlaceCPUMappingInitialNode(
    int dfg_node, CPUMappingPlacementContext& context) {
  if (context.state.dfg_to_cell[dfg_node] >= 0) return true;
  const int cell = ChooseCPUMappingInitialCell(dfg_node, context.state);
  if (cell < 0) {
    RecordFailure(
        "initial placement has no cell for node=" + NodeLabel(dfg_node) +
        " io=" + std::to_string(IsIONode(dfg_node) ? 1 : 0) +
        " free_compatible=" +
        std::to_string(
            FreeCPUMappingCompatibleCellCount(dfg_node, context.state)) +
        " placed=" + std::to_string(PlacedCount(context.state)) + "/" +
        std::to_string(dfg_.GetNodeNum()));
    return false;
  }
  PlaceCPUMappingNode(dfg_node, cell, context);
  return true;
}

std::vector<std::pair<int, int>>
Placement2DYOTOEngine::CPUMappingAdjacencyOffsets() {
  std::vector<std::pair<int, int>> offsets;
  offsets.reserve(480);
  for (int distance = 1; distance <= 15; distance++) {
    offsets.push_back({0, distance});
    offsets.push_back({distance, 0});
    offsets.push_back({0, -distance});
    offsets.push_back({-distance, 0});
    for (int i = 1; i < distance; i++) offsets.push_back({i, distance - i});
    for (int i = distance - 1; i >= 1; i--) {
      offsets.push_back({i, -(distance - i)});
    }
    for (int i = 1; i < distance; i++) offsets.push_back({-i, -(distance - i)});
    for (int i = distance - 1; i >= 1; i--) {
      offsets.push_back({-i, distance - i});
    }
  }
  return offsets;
}

bool Placement2DYOTOEngine::TryCPUMappingAdjacency(
    int dfg_node, int anchor_cell, CPUMappingPlacementContext& context,
    bool randomize_first) {
  if (anchor_cell < 0) {
    return PlaceCPUMappingInitialNode(dfg_node, context);
  }
  static const std::vector<std::pair<int, int>> kOffsets =
      CPUMappingAdjacencyOffsets();
  std::array<std::pair<int, int>, 4> first = {
      std::pair<int, int>{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
  if (randomize_first) ShuffleWithAuthorRandom(first);

  const int anchor_row = Row(anchor_cell);
  const int anchor_col = Col(anchor_cell);
  for (int i = 0; i < static_cast<int>(kOffsets.size()); i++) {
    const auto [dr, dc] = i < 4 ? first[i] : kOffsets[i];
    const int row = anchor_row + dr;
    const int col = anchor_col + dc;
    if (row < 0 || col < 0 || row >= rows_ || col >= cols_) continue;
    const int cell = Cell(row, col);
    cell_visits_++;
    RecordPlacementSwapAttempts();
    if (!CanPlaceCPUMapping(dfg_node, cell, context.state)) continue;
    PlaceCPUMappingNode(dfg_node, cell, context);
    return true;
  }
  RecordFailure(
      "adjacency placement has no cell for node=" + NodeLabel(dfg_node) +
      " anchor_cell=" + std::to_string(anchor_cell) + " anchor=(" +
      std::to_string(anchor_row) + "," + std::to_string(anchor_col) + ")" +
      " free_compatible=" +
      std::to_string(
          FreeCPUMappingCompatibleCellCount(dfg_node, context.state)) +
      " placed=" + std::to_string(PlacedCount(context.state)) + "/" +
      std::to_string(dfg_.GetNodeNum()));
  return false;
}

bool Placement2DYOTOEngine::PlaceCPUMappingNearNodeYOTO(
    int dfg_node, int anchor_node, CPUMappingPlacementContext& context) {
  if (context.state.dfg_to_cell[dfg_node] >= 0) return true;
  const int anchor_cell = context.state.dfg_to_cell[anchor_node];
  if (candidate_scope_policy_ == "all") {
    const auto candidates = CPUMappingAllCompatibleCellPairs(dfg_node, context);
    const int cell = BestCPUMappingDegreeCell(candidates, dfg_node, context);
    if (cell >= 0) {
      PlaceCPUMappingNode(dfg_node, cell, context);
      return true;
    }
    return true;
  }
  if (candidate_scope_policy_ == "tips" ||
      candidate_scope_policy_ == "tips_no_fallback") {
    const auto tips = CPUMappingTipCells(anchor_cell, dfg_node, context);
    const int cell = BestCPUMappingDegreeCell(tips, dfg_node, context);
    if (cell >= 0) {
      PlaceCPUMappingNode(dfg_node, cell, context);
      return true;
    }
    if (candidate_scope_policy_ == "tips_no_fallback") return true;
  }
  return TryCPUMappingAdjacency(dfg_node, anchor_cell, context, false);
}

bool Placement2DYOTOEngine::DeferCPUMappingIfNearPlacementFails(bool placed) {
  if (placed) return true;
  // The original cpu_mapping implementation does not abort a trial when an
  // edge-local placement attempt fails. It assigns a large edge cost and keeps
  // traversing; unplaced nodes can still be reached by later edges. We keep
  // that behavior and fill any remaining nodes after the traversal.
  return true;
}

// One plan step, shared context, and multi-start trial loop.

bool Placement2DYOTOEngine::PlaceCPUMappingOrientedStep(
    const Step& step, CPUMappingPlacementContext& context) {
  return DeferCPUMappingIfNearPlacementFails(
      PlaceCPUMappingNearNodeYOTO(step.target, step.anchor, context));
}

void Placement2DYOTOEngine::UpdateCPUMappingFreedomGrid(
    int placed_cell, std::vector<int>& freedom) const {
  if (placed_cell < 0 || placed_cell >= static_cast<int>(freedom.size())) {
    return;
  }
  freedom[placed_cell] = 0;
  const bool one_hop =
      config_.network_type == entity::MRRGNetworkType::kOneHop2d;
  static constexpr std::array<std::pair<int, int>, 8> kOneHopTips = {
      std::pair<int, int>{0, 1},
      {0, 2},
      {1, 0},
      {2, 0},
      {0, -1},
      {0, -2},
      {-1, 0},
      {-2, 0}};
  static constexpr std::array<std::pair<int, int>, 4> kMeshTips = {
      std::pair<int, int>{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
  const auto update = [&](int dr, int dc) {
    const int row = Row(placed_cell) + dr;
    const int col = Col(placed_cell) + dc;
    if (row <= 0 || col <= 0 || row >= rows_ || col >= cols_) return;
    const int cell = Cell(row, col);
    if (freedom[cell] > 1) freedom[cell]--;
  };
  if (one_hop) {
    for (const auto& [dr, dc] : kOneHopTips) update(dr, dc);
  } else {
    for (const auto& [dr, dc] : kMeshTips) update(dr, dc);
  }
}

}  // namespace mapper::yoto_array
