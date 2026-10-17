#include <mapper/yoto/engine/context/yoto_context_engine.hpp>

namespace mapper::yoto_context {

std::string ModuloYOTOEngine::MapperName() const {
  switch (search_kind_) {
    case PlacementSearchKind::kYOTO:
      return "YOTO";
    case PlacementSearchKind::kModuloPhysicalYOTO:
      return "ModuloPhysicalYOTO";
    case PlacementSearchKind::kYOTOWithFallback:
      return "YOTOWithFallback";
  }
  return "unknown";
}

bool ModuloYOTOEngine::IsPhysicalThenContextLike() const {
  return search_kind_ == PlacementSearchKind::kModuloPhysicalYOTO || false ||
         false || false;
}

bool ModuloYOTOEngine::UsesPhysicalPlacementStage() const {
  return false || IsPhysicalThenContextLike();
}

bool ModuloYOTOEngine::UsesPaperTraversalPlan() const {
  return search_kind_ == PlacementSearchKind::kYOTO || false ||
         IsPhysicalThenContextLike() ||
         search_kind_ == PlacementSearchKind::kYOTOWithFallback || false ||
         false;
}

bool ModuloYOTOEngine::UsesModuloFallbackCandidates() const {
  return search_kind_ == PlacementSearchKind::kYOTOWithFallback || false;
}

bool ModuloYOTOEngine::UsesPhysicalPeExclusivePlacement() const {
  return UsesPhysicalPlacementStage() || false || false;
}

bool ModuloYOTOEngine::UsesSeparatePlacement2DIOCells() const {
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

bool ModuloYOTOEngine::UsesApproximatePlacementDistance() const {
  return false || false || IsPhysicalThenContextLike() || false || false;
}

bool ModuloYOTOEngine::NeedsAllPairsDistances() const {
  return !UsesApproximatePlacementDistance();
}

mapper::MappingResult ModuloYOTOEngine::MakeFailureResult(
    std::chrono::steady_clock::time_point start) const {
  const double mapping_time_s = SecondsSince(start);
  Log("failure mapper=" + MapperName() +
      " time_s=" + std::to_string(mapping_time_s));
  return mapper::MappingResult(false, entity::Mapping(mrrg_.GetMRRGConfig()),
                               mapping_time_s);
}

entity::Mapping ModuloYOTOEngine::MakePlacementOnlyMapping(
    const PlacementState& placement) const {
  // Keep direct neighbor connections in the emitted mapping, but do not insert
  // route nodes. Paper-style placement evaluation can then read operation
  // locations and direct-edge quality without requiring full routing success.
  std::vector<std::vector<int>> empty_route_edges(dfg_.GetNodeNum());
  return entity::Mapping(mrrg_, dfg_, placement.dfg_to_mrrg, empty_route_edges);
}

}  // namespace mapper::yoto_context
