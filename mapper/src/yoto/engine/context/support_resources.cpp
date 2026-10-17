#include <mapper/yoto/engine/context/yoto_context_engine.hpp>

namespace mapper::yoto_context {

void ModuloYOTOEngine::BuildAllPairsDistances() {
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

Annotation ModuloYOTOEngine::BuildAnnotation() {
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

bool ModuloYOTOEngine::CanOccupyResource(int dfg_node_id, int mrrg_node_id,
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

std::vector<int> ModuloYOTOEngine::ClosestCompatibleResources(
    int dfg_node_id, int anchor_mrrg_node, const PlacementState& state) const {
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

int ModuloYOTOEngine::ResourceFreedom(int mrrg_node_id,
                                      const PlacementState& state) const {
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

}  // namespace mapper::yoto_context
