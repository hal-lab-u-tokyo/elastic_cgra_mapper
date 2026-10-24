#include <mapper/yoto/engine/yoto_engine.hpp>
#include <mapper/yoto/yoto_options.hpp>
#include <stdexcept>

namespace mapper::yoto {
namespace {
MappingResult Execute(YotoEngine& engine) {
  const auto start = engine.StartSearch();
  auto placement = engine.SearchPlacement(start);
  if (!placement) return engine.Failure(start);
  if (engine.PlacementOnly())
    return engine.FinishPlacementOnly(*placement, start);
  auto routes = engine.RoutePlacement(*placement);
  if (!routes) return engine.Failure(start);
  return engine.FinishRoutedMapping(*placement, *routes, start);
}
}  // namespace

MappingResult RunYOTOSearch(const std::shared_ptr<entity::DFG>& dfg_ptr,
                            const std::shared_ptr<entity::MRRG>& mrrg_ptr,
                            PlacementSearchKind search_kind,
                            const std::optional<double>& timeout_s,
                            const std::optional<std::string>& log_file_path,
                            const PlacementSearchOptions& options) {
  const double effective_timeout = timeout_s.value_or(1.0);
  // One context is a spatial array; multiple contexts require context-aware
  // mapping.
  if (search_kind == PlacementSearchKind::kAutoYOTO) {
    search_kind = mrrg_ptr->GetMRRGConfig().context_size > 1
                      ? PlacementSearchKind::kYOTO
                      : PlacementSearchKind::kPlacement2DYOTO;
  }
  switch (search_kind) {
    case PlacementSearchKind::kYOTO:
    case PlacementSearchKind::kModuloPhysicalYOTO:
    case PlacementSearchKind::kYOTOWithFallback:
    case PlacementSearchKind::kPlacement2DYOTO:
      break;
    default:
      throw std::invalid_argument("invalid yoto search variant");
  }
  YotoEngine engine(*dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                    log_file_path, options);
  return Execute(engine);
}

MappingResult RunYotoArray(entity::DFG& dfg, entity::MRRG& mrrg,
                           Placement2DArrayKind kind,
                           const Placement2DArrayOptions& options) {
  switch (kind) {
    case mapper::yoto::Placement2DArrayKind::kPaperGuidedYOTO:
    case mapper::yoto::Placement2DArrayKind::kCPUMappingYOTO:
      break;
    default:
      throw std::invalid_argument("invalid yoto array variant");
  }
  YotoEngine engine(dfg, mrrg, kind, options);
  return Execute(engine);
}
}  // namespace mapper::yoto
