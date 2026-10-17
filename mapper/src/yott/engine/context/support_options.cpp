#include <mapper/yott/engine/context/yott_context_engine.hpp>

namespace mapper::yott_context {

std::string ModuloYOTTEngine::MapperName() const {
  switch (search_kind_) {
    case PlacementSearchKind::kYOTT:
      return "YOTT";
    case PlacementSearchKind::kModuloPhysicalYOTT:
      return "ModuloPhysicalYOTT";
    case PlacementSearchKind::kYOTTWithFallback:
      return "YOTTWithFallback";
  }
  return "unknown";
}

bool ModuloYOTTEngine::IsYOTTLike() const {
  return search_kind_ == PlacementSearchKind::kYOTT ||
         search_kind_ == PlacementSearchKind::kYOTTWithFallback ||
         search_kind_ == PlacementSearchKind::kModuloPhysicalYOTT || false;
}

bool ModuloYOTTEngine::IsPhysicalThenContextLike() const {
  return false || search_kind_ == PlacementSearchKind::kModuloPhysicalYOTT ||
         false || false;
}

bool ModuloYOTTEngine::UsesPhysicalPlacementStage() const {
  return false || IsPhysicalThenContextLike();
}

bool ModuloYOTTEngine::UsesPaperTraversalPlan() const {
  return false || search_kind_ == PlacementSearchKind::kYOTT ||
         IsPhysicalThenContextLike() || false ||
         search_kind_ == PlacementSearchKind::kYOTTWithFallback || false;
}

bool ModuloYOTTEngine::UsesModuloFallbackCandidates() const {
  return false || search_kind_ == PlacementSearchKind::kYOTTWithFallback;
}

bool ModuloYOTTEngine::UsesPhysicalPeExclusivePlacement() const {
  return UsesPhysicalPlacementStage() || false || false;
}

bool ModuloYOTTEngine::UsesSeparatePlacement2DIOCells() const {
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

bool ModuloYOTTEngine::UsesApproximatePlacementDistance() const {
  return false || false || IsPhysicalThenContextLike() || false || false;
}

bool ModuloYOTTEngine::NeedsAllPairsDistances() const {
  return !UsesApproximatePlacementDistance();
}

mapper::MappingResult ModuloYOTTEngine::MakeFailureResult(
    std::chrono::steady_clock::time_point start) const {
  const double mapping_time_s = SecondsSince(start);
  Log("failure mapper=" + MapperName() +
      " time_s=" + std::to_string(mapping_time_s));
  return mapper::MappingResult(false, entity::Mapping(mrrg_.GetMRRGConfig()),
                               mapping_time_s);
}

entity::Mapping ModuloYOTTEngine::MakePlacementOnlyMapping(
    const PlacementState& placement) const {
  // Keep direct neighbor connections in the emitted mapping, but do not insert
  // route nodes. Paper-style placement evaluation can then read operation
  // locations and direct-edge quality without requiring full routing success.
  std::vector<std::vector<int>> empty_route_edges(dfg_.GetNodeNum());
  return entity::Mapping(mrrg_, dfg_, placement.dfg_to_mrrg, empty_route_edges);
}

}  // namespace mapper::yott_context
