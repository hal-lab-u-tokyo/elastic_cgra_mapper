#include <mapper/placement2d/placement2d_search_engine.hpp>

namespace mapper::detail {

namespace {

PlacementSearchKind ToCommonSearchKind(Placement2DSearchKind search_kind) {
  switch (search_kind) {
    case Placement2DSearchKind::kYOTO:
      return PlacementSearchKind::kPlacement2DYOTO;
    case Placement2DSearchKind::kYOTT:
      return PlacementSearchKind::kPlacement2DYOTT;
    case Placement2DSearchKind::kSA:
      return PlacementSearchKind::kPlacement2DSA;
  }
  return PlacementSearchKind::kPlacement2DYOTO;
}

}  // namespace

MappingResult RunPlacement2DSearchMapper(
    const std::shared_ptr<entity::DFG>& dfg_ptr,
    const std::shared_ptr<entity::MRRG>& mrrg_ptr,
    Placement2DSearchKind search_kind,
    const std::optional<double>& timeout_s,
    const std::optional<std::string>& log_file_path,
    const PlacementSearchOptions& options) {
  return RunPlacementSearchMapper(dfg_ptr, mrrg_ptr,
                                  ToCommonSearchKind(search_kind), timeout_s,
                                  log_file_path, options);
}

}  // namespace mapper::detail
