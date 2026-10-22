#include <mapper/prisa/engine/spatial/prisa_spatial_engine.hpp>

namespace mapper::prisa_spatial {

std::string Placement2DPRISAEngine::MapperName() const {
  switch (search_kind_) {
    case PlacementSearchKind::kPlacement2DPRISA:
      return "Placement2DPRISA";
    case PlacementSearchKind::kPlacement2DPRISANoSIS:
      return "Placement2DPRISANoSIS";
    case PlacementSearchKind::kPlacement2DCostAwarePRISA:
      return "Placement2DCostAwarePRISA";
  }
  return "unknown";
}

bool Placement2DPRISAEngine::IsPRISALike() const {
  return false || false ||
         search_kind_ == PlacementSearchKind::kPlacement2DPRISA ||
         search_kind_ == PlacementSearchKind::kPlacement2DPRISANoSIS ||
         search_kind_ == PlacementSearchKind::kPlacement2DCostAwarePRISA;
}

bool Placement2DPRISAEngine::UsesPRISASIS() const {
  return false || false ||
         search_kind_ == PlacementSearchKind::kPlacement2DPRISA ||
         search_kind_ == PlacementSearchKind::kPlacement2DCostAwarePRISA;
}

bool Placement2DPRISAEngine::UsesCostAwarePRISA() const {
  return search_kind_ == PlacementSearchKind::kPlacement2DCostAwarePRISA;
}

bool Placement2DPRISAEngine::UsesPhysicalPeExclusivePlacement() const {
  return false || false || IsPRISALike();
}

bool Placement2DPRISAEngine::UsesSeparatePlacement2DIOCells() const {
  if (!false) return false;
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

bool Placement2DPRISAEngine::UsesApproximatePlacementDistance() const {
  return false || false || false || false || IsPRISALike();
}

mapper::MappingResult Placement2DPRISAEngine::MakeFailureResult(
    std::chrono::steady_clock::time_point start) const {
  const double mapping_time_s = SecondsSince(start);
  Log("failure mapper=" + MapperName() +
      " time_s=" + std::to_string(mapping_time_s));
  return mapper::MappingResult(false, entity::Mapping(mrrg_.GetMRRGConfig()),
                               mapping_time_s);
}

entity::Mapping Placement2DPRISAEngine::MakePlacementOnlyMapping(
    const PlacementState& placement) const {
  // Keep direct neighbor connections in the emitted mapping, but do not insert
  // route nodes. Paper-style placement evaluation can then read operation
  // locations and direct-edge quality without requiring full routing success.
  std::vector<std::vector<int>> empty_route_edges(dfg_.GetNodeNum());
  return entity::Mapping(mrrg_, dfg_, placement.dfg_to_mrrg, empty_route_edges);
}

}  // namespace mapper::prisa_spatial
