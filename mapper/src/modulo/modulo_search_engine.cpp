#include <mapper/modulo/modulo_search_engine.hpp>

namespace mapper::detail {

namespace {

PlacementSearchKind ToCommonSearchKind(ModuloPlacementSearchKind search_kind) {
  switch (search_kind) {
    case ModuloPlacementSearchKind::kYOTO:
      return PlacementSearchKind::kYOTO;
    case ModuloPlacementSearchKind::kYOTT:
      return PlacementSearchKind::kYOTT;
    case ModuloPlacementSearchKind::kPhysicalYOTO:
      return PlacementSearchKind::kModuloPhysicalYOTO;
    case ModuloPlacementSearchKind::kPhysicalYOTT:
      return PlacementSearchKind::kModuloPhysicalYOTT;
    case ModuloPlacementSearchKind::kPhysicalPRISA:
      return PlacementSearchKind::kModuloPhysicalPRISA;
    case ModuloPlacementSearchKind::kPhysicalPRISAManhattan:
      return PlacementSearchKind::kModuloPhysicalPRISAManhattan;
    case ModuloPlacementSearchKind::kYOTOWithFallback:
      return PlacementSearchKind::kYOTOWithFallback;
    case ModuloPlacementSearchKind::kYOTTWithFallback:
      return PlacementSearchKind::kYOTTWithFallback;
    case ModuloPlacementSearchKind::kSA:
      return PlacementSearchKind::kSA;
  }
  return PlacementSearchKind::kYOTO;
}

}  // namespace

MappingResult RunModuloPlacementSearchMapper(
    const std::shared_ptr<entity::DFG>& dfg_ptr,
    const std::shared_ptr<entity::MRRG>& mrrg_ptr,
    ModuloPlacementSearchKind search_kind,
    const std::optional<double>& timeout_s,
    const std::optional<std::string>& log_file_path,
    const PlacementSearchOptions& options) {
  return RunPlacementSearchMapper(dfg_ptr, mrrg_ptr,
                                  ToCommonSearchKind(search_kind), timeout_s,
                                  log_file_path, options);
}

}  // namespace mapper::detail
