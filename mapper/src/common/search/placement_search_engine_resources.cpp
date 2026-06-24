#include "placement_search_engine_internal.hpp"

namespace mapper::detail::placement_search {

void PlacementSearchEngine::BuildMRRGCache() {
  const int node_num = mrrg_.GetNodeNum();
  placement2d_resources_.clear();
  placement2d_resources_.reserve(node_num);
  std::set<std::pair<int, int>> seen_positions;
  for (int r = 0; r < node_num; r++) {
    const auto property = mrrg_.GetNodeProperty(r);
    if (property.context_id != 0) continue;
    if (!SupportsAnyDFGOperation(r)) continue;
    if (!seen_positions.insert(property.position_id).second) continue;
    placement2d_resources_.push_back(r);
  }
  std::sort(placement2d_resources_.begin(), placement2d_resources_.end(),
            [&](int a, int b) {
              const auto pa = mrrg_.GetNodeProperty(a).position_id;
              const auto pb = mrrg_.GetNodeProperty(b).position_id;
              if (pa.first != pb.first) return pa.first < pb.first;
              // Paper-style 2D placement labels resources in row-major
              // order before applying PRISA's distance-matrix/SIS logic.
              return pa.second < pb.second;
            });
  placement2d_resource_order_index_.assign(node_num, -1);
  for (int i = 0; i < static_cast<int>(placement2d_resources_.size()); i++) {
    placement2d_resource_order_index_[placement2d_resources_[i]] = i;
  }

  prisa_resources_ = placement2d_resources_;
  int min_row = std::numeric_limits<int>::max();
  for (int r : prisa_resources_) {
    min_row = std::min(min_row, mrrg_.GetNodeProperty(r).position_id.first);
  }
  std::sort(prisa_resources_.begin(), prisa_resources_.end(),
            [&](int a, int b) {
              const auto pa = mrrg_.GetNodeProperty(a).position_id;
              const auto pb = mrrg_.GetNodeProperty(b).position_id;
              if (pa.first != pb.first) return pa.first < pb.first;
              // PRISA's low-bandwidth labeling assumes nearby labels are
              // nearby resources. A serpentine path avoids row-major jumps
              // from the end of one row to the start of the next row.
              const bool reverse_row = ((pa.first - min_row) % 2) != 0;
              return reverse_row ? pa.second > pb.second
                                 : pa.second < pb.second;
            });
  prisa_resource_order_index_.assign(node_num, -1);
  for (int i = 0; i < static_cast<int>(prisa_resources_.size()); i++) {
    prisa_resource_order_index_[prisa_resources_[i]] = i;
  }
}

void PlacementSearchEngine::BuildAllPairsDistances() {
  const int node_num = mrrg_.GetNodeNum();
  distance_.assign(node_num, std::vector<int>(node_num, kInfDistance));
  for (int source = 0; source < node_num; source++) {
    std::queue<int> q;
    distance_[source][source] = 0;
    q.push(source);
    while (!q.empty()) {
      int u = q.front();
      q.pop();
      for (int v : mrrg_.GetAdjacentNodeIdVec(u)) {
        if (distance_[source][v] != kInfDistance) continue;
        distance_[source][v] = distance_[source][u] + 1;
        q.push(v);
      }
    }
  }
}

Annotation PlacementSearchEngine::BuildAnnotation() {
  Annotation annotation;
  const int n = dfg_.GetNodeNum();
  annotation.in_degree.assign(n, 0);
  annotation.out_degree.assign(n, 0);
  annotation.degree.assign(n, 0);

  for (int v = 0; v < n; v++) {
    annotation.in_degree[v] = static_cast<int>(predecessors_[v].size());
    annotation.out_degree[v] = static_cast<int>(successors_[v].size());
    annotation.degree[v] = annotation.in_degree[v] + annotation.out_degree[v];
  }
  return annotation;
}

bool PlacementSearchEngine::HasTimedOut(std::chrono::steady_clock::time_point start,
                 double reserve_s) const {
  return SecondsSince(start) + reserve_s >= timeout_s_;
}

bool PlacementSearchEngine::IsCompatible(int dfg_node_id, int mrrg_node_id) const {
  const entity::OpType op = dfg_.GetNodeProperty(dfg_node_id).op;
  return SupportsOperation(mrrg_.GetNodeProperty(mrrg_node_id), op);
}

bool PlacementSearchEngine::SamePhysicalPosition(int lhs_mrrg_node, int rhs_mrrg_node) const {
  return mrrg_.GetNodeProperty(lhs_mrrg_node).position_id ==
         mrrg_.GetNodeProperty(rhs_mrrg_node).position_id;
}

bool PlacementSearchEngine::IsPhysicalPositionOccupied(int mrrg_node_id,
                                const PlacementState& state) const {
  for (int r = 0; r < mrrg_.GetNodeNum(); r++) {
    if (state.mrrg_to_dfg[r] == -1) continue;
    if (SamePhysicalPosition(mrrg_node_id, r)) return true;
  }
  return false;
}

bool PlacementSearchEngine::CanOccupyResource(int dfg_node_id, int mrrg_node_id,
                       const PlacementState& state) const {
  if (state.mrrg_to_dfg[mrrg_node_id] != -1) return false;
  if (UsesPhysicalPlacementStage() &&
      mrrg_.GetNodeProperty(mrrg_node_id).context_id != 0) {
    return false;
  }
  if (!IsCompatible(dfg_node_id, mrrg_node_id)) return false;
  if (UsesSeparatePlacement2DIOCells()) {
    const bool is_io_node = IsIONode(dfg_node_id);
    const bool is_memory_cell =
        mrrg_.GetNodeProperty(mrrg_node_id).is_memory_accessible;
    if (is_io_node && !is_memory_cell) return false;
  }
  // Physical-placement variants first choose one physical PE per DFG node.
  // ModuloPhysical* mappers assign contexts only after this 2D placement.
  if (UsesPhysicalPeExclusivePlacement() &&
      IsPhysicalPositionOccupied(mrrg_node_id, state)) {
    return false;
  }
  return true;
}

std::vector<int> PlacementSearchEngine::CompatibleResources(int dfg_node_id,
                                     const PlacementState& state) const {
  std::vector<int> result;
  result.reserve(mrrg_.GetNodeNum());
  for (int r = 0; r < mrrg_.GetNodeNum(); r++) {
    if (!CanOccupyResource(dfg_node_id, r, state)) continue;
    result.push_back(r);
  }
  return result;
}

std::vector<int> PlacementSearchEngine::ClosestCompatibleResources(int dfg_node_id,
                                            int anchor_mrrg_node,
                                            const PlacementState& state) const {
  if (anchor_mrrg_node < 0) return CompatibleResources(dfg_node_id, state);
  std::vector<int> result;
  int best_distance = kInfDistance;
  for (int r = 0; r < mrrg_.GetNodeNum(); r++) {
    if (!CanOccupyResource(dfg_node_id, r, state)) continue;
    const int distance = UsesPhysicalPlacementStage()
                             ? PhysicalDistance(anchor_mrrg_node, r)
                             : ResourceDistance(anchor_mrrg_node, r);
    if (distance > best_distance) continue;
    if (distance < best_distance) {
      result.clear();
      best_distance = distance;
    }
    result.push_back(r);
  }
  return result;
}

bool PlacementSearchEngine::IsIONode(int dfg_node_id) const {
  const entity::OpType op = dfg_.GetNodeProperty(dfg_node_id).op;
  return entity::IsMemoryAccessOperation(op);
}

bool PlacementSearchEngine::SupportsAnyDFGOperation(int mrrg_node_id) const {
  const auto property = mrrg_.GetNodeProperty(mrrg_node_id);
  for (entity::OpType op : property.supported_operations) {
    if (entity::IsDFGOp(op) && op != entity::OpType::ROUTE &&
        op != entity::OpType::NOP) {
      return true;
    }
  }
  return false;
}

int PlacementSearchEngine::ResourceDistance(int from_mrrg_node, int to_mrrg_node) const {
  if (from_mrrg_node < 0 || to_mrrg_node < 0) return kInfDistance;
  return std::min(DirectedResourceDistance(from_mrrg_node, to_mrrg_node),
                  DirectedResourceDistance(to_mrrg_node, from_mrrg_node));
}

int PlacementSearchEngine::DirectedResourceDistance(int from_mrrg_node, int to_mrrg_node) const {
  if (from_mrrg_node < 0 || to_mrrg_node < 0) return kInfDistance;
  if (!distance_.empty()) return distance_[from_mrrg_node][to_mrrg_node];
  return ApproximateDirectedResourceDistance(from_mrrg_node, to_mrrg_node);
}

int PlacementSearchEngine::SpatialStepDistance(int from_mrrg_node, int to_mrrg_node) const {
  if (from_mrrg_node < 0 || to_mrrg_node < 0) return kInfDistance;
  const auto from = mrrg_.GetNodeProperty(from_mrrg_node).position_id;
  const auto to = mrrg_.GetNodeProperty(to_mrrg_node).position_id;
  const int row_distance = std::abs(from.first - to.first);
  const int column_distance = std::abs(from.second - to.second);
  const auto config = mrrg_.GetMRRGConfig();
  if (config.network_type == entity::MRRGNetworkType::kOneHopAxis2) {
    return (row_distance + 1) / 2 + (column_distance + 1) / 2;
  }
  if (config.network_type == entity::MRRGNetworkType::kDiagonal) {
    return std::max(row_distance, column_distance);
  }
  return row_distance + column_distance;
}

int PlacementSearchEngine::ApproximateDirectedResourceDistance(int from_mrrg_node,
                                        int to_mrrg_node) const {
  if (from_mrrg_node == to_mrrg_node) return 0;
  const auto from = mrrg_.GetNodeProperty(from_mrrg_node);
  const auto to = mrrg_.GetNodeProperty(to_mrrg_node);
  const int context_size = std::max(1, from.context_size);
  const int context_delta =
      (to.context_id - from.context_id + context_size) % context_size;
  const int spatial_steps = SpatialStepDistance(from_mrrg_node, to_mrrg_node);

  if (mrrg_.GetMRRGConfig().cgra_type == entity::MRRGCGRAType::kElastic) {
    if (spatial_steps > 0) return std::max(1, spatial_steps);
    return context_delta == 0 ? 0 : context_delta;
  }

  int steps = std::max(spatial_steps, context_delta);
  if (steps == 0) steps = context_size;
  while (steps % context_size != context_delta) {
    steps++;
  }
  return steps;
}

int PlacementSearchEngine::PhysicalDistance(int from_mrrg_node, int to_mrrg_node) const {
  if (from_mrrg_node < 0 || to_mrrg_node < 0) return kInfDistance;
  return std::max(1, SpatialStepDistance(from_mrrg_node, to_mrrg_node));
}

int PlacementSearchEngine::ResourceFreedom(int mrrg_node_id, const PlacementState& state) const {
  int result = 0;
  std::set<std::pair<int, int>> counted_positions;
  for (int r = 0; r < mrrg_.GetNodeNum(); r++) {
    if (UsesPhysicalPeExclusivePlacement() &&
        IsPhysicalPositionOccupied(r, state)) {
      continue;
    }
    if (!UsesPhysicalPeExclusivePlacement() && state.mrrg_to_dfg[r] != -1) {
      continue;
    }
    if (!SupportsAnyDFGOperation(r)) continue;
    if (UsesPhysicalPeExclusivePlacement()) {
      if (SamePhysicalPosition(mrrg_node_id, r)) continue;
      if (PhysicalDistance(mrrg_node_id, r) != 1) continue;
      const auto position = mrrg_.GetNodeProperty(r).position_id;
      if (counted_positions.insert(position).second) result++;
    } else if (ResourceDistance(mrrg_node_id, r) == 1) {
      result++;
    }
  }
  return result;
}

int PlacementSearchEngine::BorderDistance(int mrrg_node_id) const {
  const auto property = mrrg_.GetNodeProperty(mrrg_node_id);
  const int row = property.position_id.first;
  const int column = property.position_id.second;
  const auto config = mrrg_.GetMRRGConfig();
  return std::min(std::min(row, config.row - 1 - row),
                  std::min(column, config.column - 1 - column));
}

bool PlacementSearchEngine::IsCornerResource(int mrrg_node_id) const {
  const auto property = mrrg_.GetNodeProperty(mrrg_node_id);
  const auto config = mrrg_.GetMRRGConfig();
  const bool row_on_border =
      property.position_id.first == 0 ||
      property.position_id.first == config.row - 1;
  const bool column_on_border =
      property.position_id.second == 0 ||
      property.position_id.second == config.column - 1;
  return row_on_border && column_on_border;
}

double PlacementSearchEngine::IOPlacementScore(int dfg_node_id, int mrrg_node_id) const {
  const auto property = mrrg_.GetNodeProperty(mrrg_node_id);
  double score = BorderDistance(mrrg_node_id);
  if (entity::IsMemoryAccessOperation(dfg_.GetNodeProperty(dfg_node_id).op) ||
      dfg_.GetNodeProperty(dfg_node_id).op == entity::OpType::OUTPUT) {
    score += property.is_memory_accessible ? -8.0 : 40.0;
  } else {
    score += property.is_memory_accessible ? -1.0 : 0.0;
  }
  return score;
}

double PlacementSearchEngine::Placement2DInitialPlacementScore(int dfg_node_id,
                                        int mrrg_node_id) const {
  if (IsIONode(dfg_node_id)) {
    double score = IOPlacementScore(dfg_node_id, mrrg_node_id);
    if (IsCornerResource(mrrg_node_id)) score += 4.0;
    return score;
  }
  double score = 0.0;
  if (BorderDistance(mrrg_node_id) == 0) score += 50.0;
  if (UsesSeparatePlacement2DIOCells() &&
      mrrg_.GetNodeProperty(mrrg_node_id).is_memory_accessible) {
    score += 100.0;
  }
  return score;
}

}  // namespace mapper::detail::placement_search
