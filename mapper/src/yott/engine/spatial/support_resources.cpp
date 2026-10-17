#include <mapper/yott/engine/spatial/yott_spatial_engine.hpp>

namespace mapper::yott_spatial {

Annotation Placement2DYOTTEngine::BuildAnnotation() {
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

bool Placement2DYOTTEngine::CanOccupyResource(
    int dfg_node_id, int mrrg_node_id, const PlacementState& state) const {
  if (state.mrrg_to_dfg[mrrg_node_id] != -1) return false;
  if (true && mrrg_.GetNodeProperty(mrrg_node_id).context_id != 0) {
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
  if (true && IsPhysicalPositionOccupied(mrrg_node_id, state)) {
    return false;
  }
  return true;
}

std::vector<int> Placement2DYOTTEngine::ClosestCompatibleResources(
    int dfg_node_id, int anchor_mrrg_node, const PlacementState& state) const {
  if (anchor_mrrg_node < 0) return CompatibleResources(dfg_node_id, state);
  std::vector<int> result;
  int best_distance = kInfDistance;
  for (int r = 0; r < mrrg_.GetNodeNum(); r++) {
    if (!CanOccupyResource(dfg_node_id, r, state)) continue;
    const int distance = true ? PhysicalDistance(anchor_mrrg_node, r)
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

int Placement2DYOTTEngine::ResourceFreedom(int mrrg_node_id,
                                           const PlacementState& state) const {
  int result = 0;
  std::set<std::pair<int, int>> counted_positions;
  for (int r = 0; r < mrrg_.GetNodeNum(); r++) {
    if (true && IsPhysicalPositionOccupied(r, state)) {
      continue;
    }
    if (!true && state.mrrg_to_dfg[r] != -1) {
      continue;
    }
    if (!SupportsAnyDFGOperation(r)) continue;
    {
      if (SamePhysicalPosition(mrrg_node_id, r)) continue;
      if (PhysicalDistance(mrrg_node_id, r) != 1) continue;
      const auto position = mrrg_.GetNodeProperty(r).position_id;
      if (counted_positions.insert(position).second) result++;
    }
  }
  return result;
}

}  // namespace mapper::yott_spatial
