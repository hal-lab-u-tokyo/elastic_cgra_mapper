#include <mapper/sa/engine/spatial/sa_spatial_engine.hpp>

namespace mapper::sa_spatial {

std::string Placement2DSAEngine::MapperName() const {
  switch (search_kind_) {
    case PlacementSearchKind::kPlacement2DSA:
      return "Placement2DSA";
  }
  return "unknown";
}

mapper::MappingResult Placement2DSAEngine::MakeFailureResult(
    std::chrono::steady_clock::time_point start) const {
  const double mapping_time_s = SecondsSince(start);
  Log("failure mapper=" + MapperName() +
      " time_s=" + std::to_string(mapping_time_s));
  return mapper::MappingResult(false, entity::Mapping(mrrg_.GetMRRGConfig()),
                               mapping_time_s);
}

entity::Mapping Placement2DSAEngine::MakePlacementOnlyMapping(
    const PlacementState& placement) const {
  // Keep direct neighbor connections in the emitted mapping, but do not insert
  // route nodes. Paper-style placement evaluation can then read operation
  // locations and direct-edge quality without requiring full routing success.
  std::vector<std::vector<int>> empty_route_edges(dfg_.GetNodeNum());
  return entity::Mapping(mrrg_, dfg_, placement.dfg_to_mrrg, empty_route_edges);
}

}  // namespace mapper::sa_spatial
