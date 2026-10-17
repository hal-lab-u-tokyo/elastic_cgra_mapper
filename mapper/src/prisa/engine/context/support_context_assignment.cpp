#include <mapper/prisa/engine/context/prisa_context_engine.hpp>

namespace mapper::prisa_context {

// Physical-placement to modulo-context assignment.

std::optional<int> ModuloPRISAEngine::FindSamePhysicalResourceAtContext(
    int base_resource, int dfg_node_id, int desired_context,
    const PlacementState& assigned) const {
  const int context_size = std::max(1, mrrg_.GetMRRGConfig().context_size);
  const auto base_position = mrrg_.GetNodeProperty(base_resource).position_id;
  for (int delta = 0; delta < context_size; delta++) {
    const int context_id = (desired_context + delta) % context_size;
    for (int resource = 0; resource < mrrg_.GetNodeNum(); resource++) {
      if (assigned.mrrg_to_dfg[resource] != -1) continue;
      const auto property = mrrg_.GetNodeProperty(resource);
      if (property.position_id != base_position) continue;
      if (property.context_id != context_id) continue;
      if (!IsCompatible(dfg_node_id, resource)) continue;
      return resource;
    }
  }
  return std::nullopt;
}

std::optional<PlacementState>
ModuloPRISAEngine::AssignContextsToPhysicalPlacement(
    const PlacementState& physical_placement) const {
  if (!IsPhysicalThenContextLike()) return physical_placement;

  const int context_size = std::max(1, mrrg_.GetMRRGConfig().context_size);
  if (context_size == 1) return physical_placement;

  const std::vector<int> levels = BuildASAPContextLevels();
  PlacementState assigned;
  assigned.dfg_to_mrrg.assign(dfg_.GetNodeNum(), -1);
  assigned.mrrg_to_dfg.assign(mrrg_.GetNodeNum(), -1);

  for (int dfg_node_id = 0; dfg_node_id < dfg_.GetNodeNum(); dfg_node_id++) {
    const int base_resource = physical_placement.dfg_to_mrrg[dfg_node_id];
    if (base_resource < 0) return std::nullopt;
    const int desired_context = levels[dfg_node_id] % context_size;
    auto resource = FindSamePhysicalResourceAtContext(
        base_resource, dfg_node_id, desired_context, assigned);
    if (!resource.has_value()) return std::nullopt;
    assigned.dfg_to_mrrg[dfg_node_id] = resource.value();
    assigned.mrrg_to_dfg[resource.value()] = dfg_node_id;
  }

  return assigned;
}

std::optional<PlacementState>
ModuloPRISAEngine::FinalizePhysicalPlacementIfNeeded(
    const PlacementState& placement) const {
  if (!IsPhysicalThenContextLike()) return placement;
  return AssignContextsToPhysicalPlacement(placement);
}

}  // namespace mapper::prisa_context
