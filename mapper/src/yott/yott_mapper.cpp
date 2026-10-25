#include <mapper/yott/engine/yott_engine.hpp>
#include <mapper/yott/yott_options.hpp>
#include <stdexcept>

namespace mapper::yott {
namespace {
MappingResult Execute(YottEngine& engine) {
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

MappingResult RunYOTTSearch(const std::shared_ptr<entity::DFG>& dfg_ptr,
                            const std::shared_ptr<entity::MRRG>& mrrg_ptr,
                            PlacementSearchKind search_kind,
                            const std::optional<double>& timeout_s,
                            const std::optional<std::string>& log_file_path,
                            const PlacementSearchOptions& options) {
  const double effective_timeout = timeout_s.value_or(1.0);
  // One context is a spatial array; multiple contexts require context-aware
  // mapping.
  if (search_kind == PlacementSearchKind::kAutoYOTT) {
    search_kind = mrrg_ptr->GetMRRGConfig().context_size > 1
                      ? PlacementSearchKind::kYOTT
                      : PlacementSearchKind::kPlacement2DYOTT;
  }
  switch (search_kind) {
    case PlacementSearchKind::kYOTT:
    case PlacementSearchKind::kModuloPhysicalYOTT:
    case PlacementSearchKind::kYOTTWithFallback:
    case PlacementSearchKind::kPlacement2DYOTT:
      break;
    default:
      throw std::invalid_argument("invalid yott search variant");
  }
  YottEngine engine(*dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                    log_file_path, options);
  return Execute(engine);
}

MappingResult RunYottArray(entity::DFG& dfg, entity::MRRG& mrrg,
                           Placement2DArrayKind kind,
                           const Placement2DArrayOptions& options) {
  switch (kind) {
    case mapper::yott::Placement2DArrayKind::kPaperGuidedYOTT:
    case mapper::yott::Placement2DArrayKind::kCPUMappingYOTT:
      break;
    default:
      throw std::invalid_argument("invalid yott array variant");
  }
  YottEngine engine(dfg, mrrg, kind, options);
  return Execute(engine);
}
}  // namespace mapper::yott
