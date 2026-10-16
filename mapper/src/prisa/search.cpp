#include <stdexcept>
#include <mapper/detail/placement_search_engine.hpp>
#include "context/engine.hpp"
#include "array/engine.hpp"
#include "spatial/engine.hpp"
namespace mapper::detail {
MappingResult RunPRISASearch(
    const std::shared_ptr<entity::DFG>& dfg_ptr,
    const std::shared_ptr<entity::MRRG>& mrrg_ptr,
    PlacementSearchKind search_kind, const std::optional<double>& timeout_s,
    const std::optional<std::string>& log_file_path,
    const PlacementSearchOptions& options) {
  const double effective_timeout = timeout_s.value_or(1.0);
  // One context is a spatial array; multiple contexts require context-aware mapping.
  if (search_kind == PlacementSearchKind::kAutoPRISA) {
    search_kind = mrrg_ptr->GetMRRGConfig().context_size > 1
                      ? PlacementSearchKind::kModuloPhysicalPRISA
                      : PlacementSearchKind::kPlacement2DPRISA;
  }
  switch (search_kind) {
    case PlacementSearchKind::kModuloPhysicalPRISA:
      return prisa_context::ModuloPRISAEngine(
                 *dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                 log_file_path, options)
          .Run();
    case PlacementSearchKind::kModuloPhysicalPRISAManhattan:
      return prisa_context::ModuloPRISAEngine(
                 *dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                 log_file_path, options)
          .Run();
    case PlacementSearchKind::kPlacement2DPRISA:
      return prisa_spatial::Placement2DPRISAEngine(
                 *dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                 log_file_path, options)
          .Run();
    case PlacementSearchKind::kPlacement2DPRISANoSIS:
      return prisa_spatial::Placement2DPRISAEngine(
                 *dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                 log_file_path, options)
          .Run();
    case PlacementSearchKind::kPlacement2DCostAwarePRISA:
      return prisa_spatial::Placement2DPRISAEngine(
                 *dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                 log_file_path, options)
          .Run();
    default: throw std::invalid_argument("invalid prisa search variant");
  }
}
}  // namespace mapper::detail
