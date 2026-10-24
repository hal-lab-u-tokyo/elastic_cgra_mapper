#include <mapper/prisa/engine/prisa_engine.hpp>
#include <mapper/prisa/prisa_options.hpp>
#include <stdexcept>

namespace mapper::prisa {
namespace {
MappingResult Execute(PrisaEngine& engine) {
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

MappingResult RunPRISASearch(const std::shared_ptr<entity::DFG>& dfg_ptr,
                             const std::shared_ptr<entity::MRRG>& mrrg_ptr,
                             PlacementSearchKind search_kind,
                             const std::optional<double>& timeout_s,
                             const std::optional<std::string>& log_file_path,
                             const PlacementSearchOptions& options) {
  const double effective_timeout = timeout_s.value_or(1.0);
  // One context is a spatial array; multiple contexts require context-aware
  // mapping.
  if (search_kind == PlacementSearchKind::kAutoPRISA) {
    search_kind = mrrg_ptr->GetMRRGConfig().context_size > 1
                      ? PlacementSearchKind::kModuloPhysicalPRISA
                      : PlacementSearchKind::kPlacement2DPRISA;
  }
  switch (search_kind) {
    case PlacementSearchKind::kModuloPhysicalPRISA:
    case PlacementSearchKind::kModuloPhysicalPRISAManhattan:
    case PlacementSearchKind::kPlacement2DPRISA:
    case PlacementSearchKind::kPlacement2DPRISANoSIS:
    case PlacementSearchKind::kPlacement2DCostAwarePRISA:
      break;
    default:
      throw std::invalid_argument("invalid prisa search variant");
  }
  PrisaEngine engine(*dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                     log_file_path, options);
  return Execute(engine);
}

MappingResult RunPrisaArray(entity::DFG& dfg, entity::MRRG& mrrg,
                            Placement2DArrayKind kind,
                            const Placement2DArrayOptions& options) {
  switch (kind) {
    case mapper::prisa::Placement2DArrayKind::kPRISA:
    case mapper::prisa::Placement2DArrayKind::kPRISANoSIS:
    case mapper::prisa::Placement2DArrayKind::kCostAwarePRISA:
      break;
    default:
      throw std::invalid_argument("invalid prisa array variant");
  }
  PrisaEngine engine(dfg, mrrg, kind, options);
  return Execute(engine);
}
}  // namespace mapper::prisa
