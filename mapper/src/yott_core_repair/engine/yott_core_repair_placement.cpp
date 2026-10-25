#include <mapper/yott_core_repair/engine/yott_core_repair_engine.hpp>

namespace mapper::yott_core_repair {
// PR/WR region accounting and local swap deltas.

std::vector<int> YottCoreRepairEngine::AffectedEdgesForCellPair(
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

int YottCoreRepairEngine::CellAfterSwap(int cell, int first_cell,
                                        int second_cell) const {
  if (cell == first_cell) return second_cell;
  if (cell == second_cell) return first_cell;
  return cell;
}

bool YottCoreRepairEngine::CanSwapCells(const PlacementState& state,
                                        int first_cell, int second_cell) const {
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

bool YottCoreRepairEngine::ApplyCellSwap(PlacementState& state, int first_cell,
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

// cpu_mapping-style YOTO/YOTT pipeline.
//
//   1. BuildCPUMappingPlan()
//      Port the public cpu_mapping edge traversal order. YOTT attaches
//      reconvergence annotations after the plan is built.
//   2. MakeCPUMappingContext()
//      Keep placement, freedom grid, and mutable node degree together.
//   3. ConstructCPUMappingPlacement(plan)
//      Walk the plan. Place missing endpoints using the selected YOTO/YOTT
//      step policy.
//   4. PlaceCPUMappingOrientedStep()
//      Dispatch one edge-local step to YOTO adjacency placement or YOTT
//      annotated-tip placement.
//   5. RunCPUMappingMultiStart()
//      Repeat plan generation and placement, then keep the lowest
//      cpu_mapping-style traversal edge cost.
//
// Ablation handles:
//   - traversal order: BuildCPUMappingPlan()
//   - neighbor choice: ChooseCPUMappingZigZagNeighbor()
//   - YOTT cycle hints: ApplyCPUMappingCycleAnnotations()
//   - local cells/tips: CPUMappingTipCells(), TryCPUMappingAdjacency()
//   - cell choice: BestCPUMappingDegreeCell()

CPUMappingPlacementContext YottCoreRepairEngine::MakeCPUMappingContext() const {
  CPUMappingPlacementContext context;
  context.state.dfg_to_cell.assign(dfg_.GetNodeNum(), -1);
  context.state.cell_to_dfg.assign(rows_ * cols_, -1);
  context.freedom = InitialFreedomGrid();
  context.mutable_degree = degree_;
  return context;
}

// Candidate cells and local placement primitives.

std::vector<int> YottCoreRepairEngine::CPUMappingCompatibleCells(
    int dfg_node, const PlacementState& state) const {
  std::vector<int> result;
  for (int cell : compatible_cells_[dfg_node]) {
    if (!CanPlaceCPUMapping(dfg_node, cell, state)) continue;
    result.push_back(cell);
  }
  return result;
}

std::vector<std::pair<int, int>>
YottCoreRepairEngine::CPUMappingAllCompatibleCellPairs(
    int dfg_node, const CPUMappingPlacementContext& context) const {
  std::vector<std::pair<int, int>> result;
  for (int cell : CPUMappingCompatibleCells(dfg_node, context.state)) {
    result.push_back({context.freedom[cell], cell});
  }
  return result;
}

std::vector<int> YottCoreRepairEngine::CPUMappingIOCellsForInitialPlacement(
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

int YottCoreRepairEngine::ChooseCPUMappingInitialCell(
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

std::vector<std::pair<int, int>> YottCoreRepairEngine::CPUMappingTipCells(
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

std::vector<std::pair<int, int>>
YottCoreRepairEngine::CPUMappingCellsWithinAnnotatedDistance(
    int anchor_cell, int distance,
    const CPUMappingPlacementContext& context) const {
  std::vector<std::pair<int, int>> result;
  if (anchor_cell < 0 || distance < 0) return result;
  int search_distance = distance;
  const bool one_hop =
      config_.network_type == entity::MRRGNetworkType::kOneHop2d;
  if (one_hop) search_distance *= 2;
  if (search_distance == 0) search_distance = 1;
  const int anchor_row = Row(anchor_cell);
  const int anchor_col = Col(anchor_cell);
  for (int dr = -search_distance; dr <= search_distance; dr++) {
    for (int dc = -search_distance; dc <= search_distance; dc++) {
      const int row = anchor_row + dr;
      const int col = anchor_col + dc;
      if (row < 0 || col < 0 || row >= rows_ || col >= cols_) continue;
      const int cell = Cell(row, col);
      if (cell < 0 || cell >= static_cast<int>(context.freedom.size())) {
        continue;
      }
      if (context.freedom[cell] <= 0) continue;
      int diff = 0;
      if (one_hop) {
        diff = (std::abs(dr) + 1) / 2 + (std::abs(dc) + 1) / 2 - 1;
      } else {
        diff = std::abs(dr) + std::abs(dc) - 1;
      }
      if (diff >= 0 && diff < search_distance) {
        result.push_back({context.freedom[cell], cell});
      }
    }
  }
  return result;
}

void YottCoreRepairEngine::SortCPUMappingCellPairs(
    std::vector<std::pair<int, int>>& cells) {
  std::sort(cells.begin(), cells.end());
  cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
}

std::vector<std::pair<int, int>>
YottCoreRepairEngine::IntersectCPUMappingCellPairs(
    std::vector<std::pair<int, int>> lhs,
    std::vector<std::pair<int, int>> rhs) {
  SortCPUMappingCellPairs(lhs);
  SortCPUMappingCellPairs(rhs);
  std::vector<std::pair<int, int>> result;
  std::set_intersection(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(),
                        std::back_inserter(result));
  return result;
}

void YottCoreRepairEngine::PlaceCPUMappingNode(
    int dfg_node, int cell, CPUMappingPlacementContext& context) const {
  PlaceNode(dfg_node, cell, context.state);
  if (dfg_node >= 0 &&
      dfg_node < static_cast<int>(context.mutable_degree.size())) {
    context.mutable_degree[dfg_node]--;
  }
  UpdateCPUMappingFreedomGrid(cell, context.freedom);
}

bool YottCoreRepairEngine::PlaceCPUMappingInitialNode(
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
YottCoreRepairEngine::CPUMappingAdjacencyOffsets() {
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

bool YottCoreRepairEngine::TryCPUMappingAdjacency(
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

// One plan step, shared context, and multi-start trial loop.

void YottCoreRepairEngine::UpdateCPUMappingFreedomGrid(
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

// Traversal plan and YOTT annotation construction.

int YottCoreRepairEngine::FindUnusedEdgeIndex(
    int source, int target, const std::vector<char>& used_edges) const {
  if (source < 0 || source >= static_cast<int>(incident_edge_ids_.size())) {
    return -1;
  }
  // Incident IDs retain DFG edge order, so this matches the first-unused
  // global scan without visiting unrelated edges.
  for (int edge_id : incident_edge_ids_[source]) {
    if (used_edges[edge_id]) continue;
    if (edges_[edge_id].source == source && edges_[edge_id].target == target) {
      return edge_id;
    }
  }
  return -1;
}

int YottCoreRepairEngine::ChooseCPUMappingZigZagNeighbor(
    const std::vector<int>& candidates, bool choose_from_fanout, int mode,
    bool zigzag_take_back, const std::vector<int>& critical_path,
    const std::vector<double>& betweenness) {
  if (candidates.empty()) return -1;
  if (candidates.size() == 1) return candidates.front();

  if (mode == 0) {
    int best = candidates.front();
    for (int i = 1; i < static_cast<int>(candidates.size()); i++) {
      const int candidate = candidates[i];
      const int candidate_degree =
          choose_from_fanout
              ? static_cast<int>(successors_[candidate].size())
              : static_cast<int>(predecessors_[candidate].size());
      const int best_degree =
          choose_from_fanout ? static_cast<int>(successors_[best].size())
                             : static_cast<int>(predecessors_[best].size());
      if (candidate_degree > best_degree) best = candidate;
    }
    return best;
  }

  if (mode == 1) {
    int best = candidates.front();
    for (int i = 1; i < static_cast<int>(candidates.size()); i++) {
      const int candidate = candidates[i];
      if (betweenness[candidate] > betweenness[best]) best = candidate;
    }
    return best;
  }

  if (mode == 2) {
    int best = candidates.front();
    for (int i = 1; i < static_cast<int>(candidates.size()); i++) {
      const int candidate = candidates[i];
      if (critical_path[candidate] > critical_path[best]) best = candidate;
    }
    return best;
  }

  if (mode == 3) {
    return zigzag_take_back ? candidates.back() : candidates.front();
  }

  return candidates[AuthorRandomInt(static_cast<int>(candidates.size()))];
}

void YottCoreRepairEngine::AddCPUMappingCycleAnnotation(std::vector<Step>& plan,
                                                        int step_index,
                                                        int anchor_node,
                                                        int distance) const {
  if (step_index < 0 || step_index >= static_cast<int>(plan.size())) return;
  plan[step_index].annotations.push_back(StepAnnotation{
      StepAnnotationKind::kReconvergence, anchor_node, distance});
}

void YottCoreRepairEngine::ApplyCPUMappingCycleAnnotations(
    std::vector<Step>& plan,
    const std::vector<std::pair<int, int>>& cycle_edges) const {
  for (const auto& [cycle_begin, cycle_end] : cycle_edges) {
    bool found_start = false;
    int count = 0;
    int value1 = -1;
    std::vector<int> walk_indices;
    for (int j = static_cast<int>(plan.size()) - 1; j >= 0; j--) {
      if (cycle_begin == plan[j].target && !found_start) {
        value1 = plan[j].anchor;
        AddCPUMappingCycleAnnotation(plan, j, cycle_end, count);
        count += 1;
        found_start = true;
      } else if (found_start &&
                 (value1 == plan[j].target || cycle_end == plan[j].anchor)) {
        value1 = plan[j].anchor;
        const int value2 = plan[j].target;
        if (value1 != cycle_end && value2 != cycle_end) {
          walk_indices.insert(walk_indices.begin(), j);
          AddCPUMappingCycleAnnotation(plan, j, cycle_end, count);
          count += 1;
        } else {
          for (int k = 0;
               k < count / 2 && k < static_cast<int>(walk_indices.size());
               k++) {
            Step& step = plan[walk_indices[k]];
            for (auto& annotation : step.annotations) {
              if (annotation.kind == StepAnnotationKind::kReconvergence &&
                  annotation.anchor_node == cycle_end &&
                  annotation.distance != k + 1) {
                annotation.distance = k + 1;
              }
            }
          }
          break;
        }
      }
    }
  }
}

std::vector<Step> YottCoreRepairEngine::BuildCPUMappingPlan() {
  std::vector<Step> plan;
  std::vector<char> used_edges(edges_.size(), false);
  std::vector<std::pair<int, int>> cycle_edges;
  std::vector<char> visited_nodes(dfg_.GetNodeNum(), false);
  std::vector<std::vector<int>> local_fanin = predecessors_;
  std::vector<std::vector<int>> local_fanout = successors_;
  std::vector<std::pair<int, int>> stack;
  for (int root : CPUMappingTraversalRoots()) stack.push_back({root, 1});
  int selection_mode = TraversalNeighborMode(true);
  std::vector<int> critical_path;
  std::vector<double> betweenness;
  if (selection_mode == 1) betweenness = BuildBetweennessCentralityScore();
  if (selection_mode == 2) critical_path = BuildCPUMappingInputDistanceScore();

  auto remove_value = [](std::vector<int>& values, int value) {
    values.erase(std::remove(values.begin(), values.end(), value),
                 values.end());
  };

  auto push_remaining = [&]() {
    for (int node = 0; node < dfg_.GetNodeNum(); node++) {
      if (!local_fanin[node].empty() || !local_fanout[node].empty()) {
        stack.push_back({node, 1});
        return true;
      }
    }
    for (int node = 0; node < dfg_.GetNodeNum(); node++) {
      if (!visited_nodes[node]) {
        stack.push_back({node, 1});
        return true;
      }
    }
    return false;
  };

  auto append_step = [&](int from, int to) {
    if (from < 0 || to < 0 || from == to) return;
    if (visited_nodes[to]) cycle_edges.push_back({from, to});
    int edge_id = FindUnusedEdgeIndex(from, to, used_edges);
    if (edge_id < 0) {
      edge_id = FindUnusedEdgeIndex(to, from, used_edges);
    }
    if (edge_id >= 0) used_edges[edge_id] = true;
    plan.push_back(Step{from, to, edge_id, {}});
  };

  if (stack.empty()) push_remaining();
  while (!stack.empty()) {
    const int a = stack.back().first;
    const int direction = stack.back().second;
    stack.pop_back();
    if (a < 0 || a >= dfg_.GetNodeNum()) continue;

    const int fanin = static_cast<int>(local_fanin[a].size());
    const int fanout = static_cast<int>(local_fanout[a].size());
    int b = -1;
    if (direction == 1) {
      if (fanout >= 1) {
        b = ChooseCPUMappingZigZagNeighbor(local_fanout[a], true,
                                           selection_mode, true, critical_path,
                                           betweenness);
        for (int i = 0; i < fanin; i++) stack.push_back({a, 1});
        stack.push_back({b, 0});
        remove_value(local_fanout[a], b);
        remove_value(local_fanin[b], a);
        append_step(a, b);
      } else if (fanin >= 1) {
        b = ChooseCPUMappingZigZagNeighbor(local_fanin[a], false,
                                           selection_mode, true, critical_path,
                                           betweenness);
        stack.push_back({a, 1});
        for (int i = 0; i < fanin; i++) stack.push_back({b, 1});
        remove_value(local_fanin[a], b);
        remove_value(local_fanout[b], a);
        append_step(a, b);
      }
    } else {
      if (fanin >= 1) {
        b = ChooseCPUMappingZigZagNeighbor(local_fanin[a], false,
                                           selection_mode, false, critical_path,
                                           betweenness);
        for (int i = 0; i < fanout; i++) stack.push_back({a, 0});
        stack.push_back({b, 1});
        remove_value(local_fanin[a], b);
        remove_value(local_fanout[b], a);
        append_step(a, b);
      } else if (fanout >= 1) {
        b = ChooseCPUMappingZigZagNeighbor(local_fanout[a], true,
                                           selection_mode, false, critical_path,
                                           betweenness);
        stack.push_back({a, 0});
        for (int i = 0; i < fanout; i++) stack.push_back({b, 0});
        remove_value(local_fanout[a], b);
        remove_value(local_fanin[b], a);
        append_step(a, b);
      }
    }
    visited_nodes[a] = true;
    if (stack.empty()) push_remaining();
  }

  std::vector<int> remaining;
  for (int edge_id = 0; edge_id < static_cast<int>(edges_.size()); edge_id++) {
    if (!used_edges[edge_id]) remaining.push_back(edge_id);
  }
  ShuffleWithAuthorRandom(remaining);
  for (int edge_id : remaining) {
    plan.push_back(
        Step{edges_[edge_id].source, edges_[edge_id].target, edge_id, {}});
  }
  if (UsesYOTTAnnotations()) ApplyCPUMappingCycleAnnotations(plan, cycle_edges);
  return plan;
}

// DFS/BFS baselines traverse DFG edges as an undirected graph so that, like
// zigzag, a Step always means "place target near anchor" regardless of the
// original data-flow direction.
std::vector<Step> YottCoreRepairEngine::BuildGraphSearchPlan(
    bool breadth_first) {
  std::vector<Step> plan;
  std::vector<std::pair<int, int>> cycle_edges;
  std::vector<char> used_edges(edges_.size(), false);
  std::vector<char> discovered(dfg_.GetNodeNum(), false);
  std::vector<std::vector<int>> adjacency = incident_edge_ids_;
  for (auto& edge_ids : adjacency) ShuffleWithAuthorRandom(edge_ids);

  std::vector<int> starts = CPUMappingTraversalRoots();
  std::vector<int> remaining_nodes(dfg_.GetNodeNum());
  std::iota(remaining_nodes.begin(), remaining_nodes.end(), 0);
  ShuffleWithAuthorRandom(remaining_nodes);
  starts.insert(starts.end(), remaining_nodes.begin(), remaining_nodes.end());

  auto other_endpoint = [&](int edge_id, int node) {
    const EdgeInfo& edge = edges_[edge_id];
    return edge.source == node ? edge.target : edge.source;
  };

  auto append_edge = [&](int anchor, int target, int edge_id) {
    if (discovered[target]) cycle_edges.push_back({anchor, target});
    plan.push_back(Step{anchor, target, edge_id, {}});
  };

  for (int start : starts) {
    bool has_unused_edge = false;
    for (int edge_id : adjacency[start]) {
      if (!used_edges[edge_id]) {
        has_unused_edge = true;
        break;
      }
    }
    if (!has_unused_edge) {
      discovered[start] = true;
      continue;
    }

    discovered[start] = true;
    if (breadth_first) {
      std::queue<int> queue;
      queue.push(start);
      while (!queue.empty()) {
        const int anchor = queue.front();
        queue.pop();
        for (int edge_id : adjacency[anchor]) {
          if (used_edges[edge_id]) continue;
          used_edges[edge_id] = true;
          const int target = other_endpoint(edge_id, anchor);
          const bool target_was_discovered = discovered[target];
          append_edge(anchor, target, edge_id);
          if (!target_was_discovered) {
            discovered[target] = true;
            queue.push(target);
          }
        }
      }
      continue;
    }

    std::vector<std::pair<int, int>> stack{{start, 0}};
    while (!stack.empty()) {
      const int anchor = stack.back().first;
      int& next_index = stack.back().second;
      while (next_index < static_cast<int>(adjacency[anchor].size()) &&
             used_edges[adjacency[anchor][next_index]]) {
        next_index++;
      }
      if (next_index == static_cast<int>(adjacency[anchor].size())) {
        stack.pop_back();
        continue;
      }

      const int edge_id = adjacency[anchor][next_index++];
      used_edges[edge_id] = true;
      const int target = other_endpoint(edge_id, anchor);
      const bool target_was_discovered = discovered[target];
      append_edge(anchor, target, edge_id);
      if (!target_was_discovered) {
        discovered[target] = true;
        stack.push_back({target, 0});
      }
    }
  }

  if (plan.size() != edges_.size()) {
    throw std::runtime_error("graph traversal did not visit every DFG edge");
  }
  if (UsesYOTTAnnotations()) ApplyCPUMappingCycleAnnotations(plan, cycle_edges);
  return plan;
}

// This baseline removes all graph-traversal structure: both edge order and
// orientation are independently randomized for every trial.
std::vector<Step> YottCoreRepairEngine::BuildFullyRandomPlan() {
  std::vector<int> edge_ids(edges_.size());
  std::iota(edge_ids.begin(), edge_ids.end(), 0);
  ShuffleWithAuthorRandom(edge_ids);

  std::vector<Step> plan;
  std::vector<std::pair<int, int>> cycle_edges;
  std::vector<char> seen(dfg_.GetNodeNum(), false);
  plan.reserve(edge_ids.size());
  for (int edge_id : edge_ids) {
    int anchor = edges_[edge_id].source;
    int target = edges_[edge_id].target;
    if (AuthorRandomInt(2) != 0) std::swap(anchor, target);
    if (seen[anchor] && seen[target]) cycle_edges.push_back({anchor, target});
    plan.push_back(Step{anchor, target, edge_id, {}});
    seen[anchor] = true;
    seen[target] = true;
  }

  if (plan.size() != edges_.size()) {
    throw std::runtime_error("random traversal did not visit every DFG edge");
  }
  if (UsesYOTTAnnotations()) ApplyCPUMappingCycleAnnotations(plan, cycle_edges);
  return plan;
}

// YOTT Core keeps only the compact traversal pieces:
// random local traversal, reconvergence annotations, annotated tip placement,
// nearest-cell fallback, and best-of-trials selection by PlacementCost().

std::vector<Step> YottCoreRepairEngine::BuildYOTTCorePlan() {
  if (traversal_order_policy_ == "depth_first") {
    return BuildGraphSearchPlan(false);
  }
  if (traversal_order_policy_ == "breadth_first") {
    return BuildGraphSearchPlan(true);
  }
  if (traversal_order_policy_ == "fully_random") {
    return BuildFullyRandomPlan();
  }
  if (traversal_order_policy_ != "zigzag" &&
      traversal_order_policy_ != "default") {
    Log("unknown traversal_order_policy=" + traversal_order_policy_ +
        "; using zigzag");
  }

  const std::string saved_policy = traversal_neighbor_policy_;
  if (traversal_neighbor_policy_ == "default") {
    traversal_neighbor_policy_ = "random";
  }
  std::vector<Step> plan = BuildCPUMappingPlan();
  traversal_neighbor_policy_ = saved_policy;
  return plan;
}

int YottCoreRepairEngine::ChooseYOTTCoreCell(
    const std::vector<std::pair<int, int>>& cells, int dfg_node,
    const CPUMappingPlacementContext& context) {
  std::vector<int> valid_cells;
  valid_cells.reserve(cells.size());
  for (const auto& [unused_freedom, cell] : cells) {
    (void)unused_freedom;
    if (CanPlaceCPUMapping(dfg_node, cell, context.state)) {
      valid_cells.push_back(cell);
    }
  }
  if (valid_cells.empty()) return -1;
  RecordPlacementSwapAttempts(static_cast<long long>(valid_cells.size()));
  std::uniform_int_distribution<int> dist(
      0, static_cast<int>(valid_cells.size()) - 1);
  return valid_cells[dist(rng_)];
}

bool YottCoreRepairEngine::PlaceYOTTCoreStep(
    const Step& step, CPUMappingPlacementContext& context) {
  const int dfg_node = step.target;
  if (context.state.dfg_to_cell[dfg_node] >= 0) return true;

  const int anchor_cell = context.state.dfg_to_cell[step.anchor];
  if (anchor_cell < 0) {
    return PlaceCPUMappingInitialNode(dfg_node, context);
  }

  std::vector<std::pair<int, int>> tips =
      CPUMappingTipCells(anchor_cell, dfg_node, context);
  std::vector<std::pair<int, int>> annotated_tips = tips;
  std::vector<int> placed_annotation_nodes;

  for (const auto& annotation : step.annotations) {
    if (annotation.kind != StepAnnotationKind::kReconvergence) continue;
    if (annotation.distance > 2) continue;
    const int annotated_cell =
        context.state.dfg_to_cell[annotation.anchor_node];
    if (annotated_cell < 0) continue;
    placed_annotation_nodes.push_back(annotation.anchor_node);
    auto cells = CPUMappingCellsWithinAnnotatedDistance(
        annotated_cell, annotation.distance, context);
    annotated_tips = IntersectCPUMappingCellPairs(annotated_tips, cells);
    if (annotated_tips.empty()) break;
  }

  int cell = ChooseYOTTCoreCell(annotated_tips, dfg_node, context);
  if (cell >= 0) {
    PlaceCPUMappingNode(dfg_node, cell, context);
    return true;
  }

  if (!tips.empty()) {
    std::vector<std::pair<int, int>> candidates = tips;
    if (!placed_annotation_nodes.empty()) {
      int best_cost = std::numeric_limits<int>::max();
      std::vector<std::pair<int, int>> closest_tips;
      for (const auto& [available, candidate] : tips) {
        int cost = 0;
        for (int annotated_node : placed_annotation_nodes) {
          const int annotated_cell = context.state.dfg_to_cell[annotated_node];
          if (annotated_cell >= 0)
            cost += DistanceCost(annotated_cell, candidate);
        }
        if (cost < best_cost) {
          best_cost = cost;
          closest_tips.clear();
          closest_tips.push_back({available, candidate});
        } else if (cost == best_cost) {
          closest_tips.push_back({available, candidate});
        }
      }
      if (!closest_tips.empty()) candidates = closest_tips;
    }

    cell = ChooseYOTTCoreCell(candidates, dfg_node, context);
    if (cell >= 0) {
      PlaceCPUMappingNode(dfg_node, cell, context);
      return true;
    }
  }

  return TryCPUMappingAdjacency(dfg_node, anchor_cell, context, true);
}

std::optional<PlacementState> YottCoreRepairEngine::ConstructYOTTCorePlacement(
    const std::vector<Step>& plan) {
  CPUMappingPlacementContext context = MakeCPUMappingContext();

  for (const auto& step : plan) {
    const bool anchor_placed = context.state.dfg_to_cell[step.anchor] >= 0;
    const bool target_placed = context.state.dfg_to_cell[step.target] >= 0;
    if (!anchor_placed && !target_placed) {
      if (!PlaceCPUMappingInitialNode(step.anchor, context)) {
        return std::nullopt;
      }
      PlaceYOTTCoreStep(step, context);
    } else if (anchor_placed && !target_placed) {
      PlaceYOTTCoreStep(step, context);
    } else if (!anchor_placed && target_placed) {
      Step reversed = step;
      reversed.anchor = step.target;
      reversed.target = step.anchor;
      PlaceYOTTCoreStep(reversed, context);
    }
  }

  for (int node = 0; node < dfg_.GetNodeNum(); node++) {
    if (!PlaceCPUMappingInitialNode(node, context)) {
      return std::nullopt;
    }
  }
  return context.state;
}

}  // namespace mapper::yott_core_repair
