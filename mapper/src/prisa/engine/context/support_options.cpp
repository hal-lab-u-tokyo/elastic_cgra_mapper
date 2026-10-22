#include <mapper/prisa/engine/context/prisa_context_engine.hpp>

namespace mapper::prisa_context {

std::string ModuloPRISAEngine::MapperName() const {
  switch (search_kind_) {
    case PlacementSearchKind::kModuloPhysicalPRISA:
      return "ModuloPhysicalPRISA";
    case PlacementSearchKind::kModuloPhysicalPRISAManhattan:
      return "ModuloPhysicalPRISAManhattan";
  }
  return "unknown";
}

bool ModuloPRISAEngine::IsPRISALike() const {
  return search_kind_ == PlacementSearchKind::kModuloPhysicalPRISA ||
         search_kind_ == PlacementSearchKind::kModuloPhysicalPRISAManhattan ||
         false || false || false;
}

bool ModuloPRISAEngine::UsesPRISASIS() const {
  return search_kind_ == PlacementSearchKind::kModuloPhysicalPRISA ||
         search_kind_ == PlacementSearchKind::kModuloPhysicalPRISAManhattan ||
         false || false;
}

bool ModuloPRISAEngine::UsesManhattanRouting() const {
  return search_kind_ == PlacementSearchKind::kModuloPhysicalPRISAManhattan;
}

bool ModuloPRISAEngine::IsPhysicalThenContextLike() const {
  return false || false ||
         search_kind_ == PlacementSearchKind::kModuloPhysicalPRISA ||
         search_kind_ == PlacementSearchKind::kModuloPhysicalPRISAManhattan;
}

bool ModuloPRISAEngine::UsesPhysicalPlacementStage() const {
  return false || IsPhysicalThenContextLike();
}

bool ModuloPRISAEngine::UsesPhysicalPeExclusivePlacement() const {
  return UsesPhysicalPlacementStage() || false || IsPRISALike();
}

bool ModuloPRISAEngine::UsesSeparatePlacement2DIOCells() const {
  if (!UsesPhysicalPlacementStage()) return false;
  bool has_memory_cell = false;
  bool has_compute_only_cell = false;
  for (int r : placement2d_resources_) {
    const auto property = mrrg_.GetNodeProperty(r);
    has_memory_cell = has_memory_cell || property.is_memory_accessible;
    has_compute_only_cell =
        has_compute_only_cell || !property.is_memory_accessible;
  }
  return has_memory_cell && has_compute_only_cell;
}

bool ModuloPRISAEngine::UsesApproximatePlacementDistance() const {
  return false || false || IsPhysicalThenContextLike() || false ||
         IsPRISALike();
}

mapper::MappingResult ModuloPRISAEngine::MakeFailureResult(
    std::chrono::steady_clock::time_point start) const {
  const double mapping_time_s = SecondsSince(start);
  Log("failure mapper=" + MapperName() +
      " time_s=" + std::to_string(mapping_time_s));
  return mapper::MappingResult(false, entity::Mapping(mrrg_.GetMRRGConfig()),
                               mapping_time_s);
}

entity::Mapping ModuloPRISAEngine::MakePlacementOnlyMapping(
    const PlacementState& placement) const {
  // Keep direct neighbor connections in the emitted mapping, but do not insert
  // route nodes. Paper-style placement evaluation can then read operation
  // locations and direct-edge quality without requiring full routing success.
  std::vector<std::vector<int>> empty_route_edges(dfg_.GetNodeNum());
  return entity::Mapping(mrrg_, dfg_, placement.dfg_to_mrrg, empty_route_edges);
}

}  // namespace mapper::prisa_context
