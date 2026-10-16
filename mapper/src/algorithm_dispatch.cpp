#include <mapper/detail/placement2d_array_mapper_base.hpp>
#include <mapper/detail/placement_search_engine.hpp>
#include <stdexcept>


namespace mapper::detail {

MappingResult RunPlacement2DArrayEngine(
    entity::DFG& dfg, entity::MRRG& mrrg, Placement2DArrayKind kind,
    const Placement2DArrayOptions& options) {
  switch (kind) {
    case Placement2DArrayKind::kSA:
      return RunSaArray(dfg, mrrg, kind, options);
    case Placement2DArrayKind::kPRISA:
    case Placement2DArrayKind::kPRISANoSIS:
    case Placement2DArrayKind::kCostAwarePRISA:
      return RunPrisaArray(dfg, mrrg, kind, options);
    case Placement2DArrayKind::kPaperGuidedYOTO:
    case Placement2DArrayKind::kCPUMappingYOTO:
      return RunYotoArray(dfg, mrrg, kind, options);
    case Placement2DArrayKind::kPaperGuidedYOTT:
    case Placement2DArrayKind::kCPUMappingYOTT:
      return RunYottArray(dfg, mrrg, kind, options);
    case Placement2DArrayKind::kCPUMappingYOTTCore:
      return RunYottCoreArray(dfg, mrrg, kind, options);
    case Placement2DArrayKind::kCPUMappingYOTTCoreRepair:
      return RunYottCoreRepairArray(dfg, mrrg, kind, options);
  }
  throw std::invalid_argument("unknown array algorithm");
}

MappingResult RunPlacementSearchMapper(
    const std::shared_ptr<entity::DFG>& dfg_ptr,
    const std::shared_ptr<entity::MRRG>& mrrg_ptr,
    PlacementSearchKind search_kind, const std::optional<double>& timeout_s,
    const std::optional<std::string>& log_file_path,
    const PlacementSearchOptions& options) {
  switch (search_kind) {
    case PlacementSearchKind::kAutoSA:
    case PlacementSearchKind::kSA:
    case PlacementSearchKind::kPlacement2DSA:
      return RunSASearch(dfg_ptr, mrrg_ptr, search_kind, timeout_s, log_file_path, options);
    case PlacementSearchKind::kAutoPRISA:
    case PlacementSearchKind::kModuloPhysicalPRISA:
    case PlacementSearchKind::kModuloPhysicalPRISAManhattan:
    case PlacementSearchKind::kPlacement2DPRISA:
    case PlacementSearchKind::kPlacement2DPRISANoSIS:
    case PlacementSearchKind::kPlacement2DCostAwarePRISA:
      return RunPRISASearch(dfg_ptr, mrrg_ptr, search_kind, timeout_s, log_file_path, options);
    case PlacementSearchKind::kAutoYOTO:
    case PlacementSearchKind::kYOTO:
    case PlacementSearchKind::kModuloPhysicalYOTO:
    case PlacementSearchKind::kYOTOWithFallback:
    case PlacementSearchKind::kPlacement2DYOTO:
      return RunYOTOSearch(dfg_ptr, mrrg_ptr, search_kind, timeout_s, log_file_path, options);
    case PlacementSearchKind::kAutoYOTT:
    case PlacementSearchKind::kYOTT:
    case PlacementSearchKind::kModuloPhysicalYOTT:
    case PlacementSearchKind::kYOTTWithFallback:
    case PlacementSearchKind::kPlacement2DYOTT:
      return RunYOTTSearch(dfg_ptr, mrrg_ptr, search_kind, timeout_s, log_file_path, options);
  }
  throw std::invalid_argument("unknown search algorithm");
}

}  // namespace mapper::detail
