#include <mapper/sa/engine/sa_engine.hpp>
#include <mapper/sa/sa_options.hpp>
#include <stdexcept>

namespace mapper::sa {
namespace {
MappingResult Execute(SaEngine& engine) {
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

MappingResult RunSASearch(const std::shared_ptr<entity::DFG>& dfg,
                          const std::shared_ptr<entity::MRRG>& mrrg,
                          PlacementSearchKind kind,
                          const std::optional<double>& timeout,
                          const std::optional<std::string>& log,
                          const PlacementSearchOptions& options) {
  if (kind == PlacementSearchKind::kAutoSA) {
    kind = mrrg->GetMRRGConfig().context_size > 1
               ? PlacementSearchKind::kSA
               : PlacementSearchKind::kPlacement2DSA;
  }
  if (kind != PlacementSearchKind::kSA &&
      kind != PlacementSearchKind::kPlacement2DSA)
    throw std::invalid_argument("invalid sa search variant");
  SaEngine engine(*dfg, *mrrg, kind, timeout.value_or(1.0), log, options);
  return Execute(engine);
}

MappingResult RunSaArray(entity::DFG& dfg, entity::MRRG& mrrg,
                         Placement2DArrayKind kind,
                         const Placement2DArrayOptions& options) {
  if (kind != Placement2DArrayKind::kSA)
    throw std::invalid_argument("invalid sa array variant");
  SaEngine engine(dfg, mrrg, kind, options);
  return Execute(engine);
}
}  // namespace mapper::sa
