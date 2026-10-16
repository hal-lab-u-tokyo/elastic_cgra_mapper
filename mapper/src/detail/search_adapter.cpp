#include <mapper/detail/search_adapter.hpp>

namespace mapper::detail {

namespace {

PlacementSearchKind ToCommonSearchKind(SearchKind search_kind) {
  switch (search_kind) {
    case SearchKind::kAutoSA: return PlacementSearchKind::kAutoSA;
    case SearchKind::kAutoPRISA: return PlacementSearchKind::kAutoPRISA;
    case SearchKind::kAutoYOTO: return PlacementSearchKind::kAutoYOTO;
    case SearchKind::kAutoYOTT: return PlacementSearchKind::kAutoYOTT;

    case SearchKind::kYOTO:
      return PlacementSearchKind::kPlacement2DYOTO;
    case SearchKind::kYOTT:
      return PlacementSearchKind::kPlacement2DYOTT;
    case SearchKind::kSA:
      return PlacementSearchKind::kPlacement2DSA;
    case SearchKind::kPRISA:
      return PlacementSearchKind::kPlacement2DPRISA;
    case SearchKind::kPRISANoSIS:
      return PlacementSearchKind::kPlacement2DPRISANoSIS;
    case SearchKind::kCostAwarePRISA:
      return PlacementSearchKind::kPlacement2DCostAwarePRISA;
  }
  return PlacementSearchKind::kPlacement2DYOTO;
}

}  // namespace

MappingResult RunSearchMapper(
    const std::shared_ptr<entity::DFG>& dfg_ptr,
    const std::shared_ptr<entity::MRRG>& mrrg_ptr,
    SearchKind search_kind, const std::optional<double>& timeout_s,
    const std::optional<std::string>& log_file_path,
    const PlacementSearchOptions& options) {
  return RunPlacementSearchMapper(dfg_ptr, mrrg_ptr,
                                  ToCommonSearchKind(search_kind), timeout_s,
                                  log_file_path, options);
}

}  // namespace mapper::detail
