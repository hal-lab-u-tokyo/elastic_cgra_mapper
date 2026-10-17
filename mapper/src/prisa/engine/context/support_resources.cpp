#include <mapper/prisa/engine/context/prisa_context_engine.hpp>

namespace mapper::prisa_context {

Annotation ModuloPRISAEngine::BuildAnnotation() {
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

bool ModuloPRISAEngine::CanOccupyResource(int dfg_node_id, int mrrg_node_id,
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

}  // namespace mapper::prisa_context
