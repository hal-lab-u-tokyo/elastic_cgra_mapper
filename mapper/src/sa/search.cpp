#include <stdexcept>
#include <mapper/detail/placement_search_engine.hpp>
#include "context/engine.hpp"
#include "array/engine.hpp"
#include "spatial/engine.hpp"
namespace mapper::detail {
MappingResult RunSASearch(
    const std::shared_ptr<entity::DFG>& dfg_ptr,
    const std::shared_ptr<entity::MRRG>& mrrg_ptr,
    PlacementSearchKind search_kind, const std::optional<double>& timeout_s,
    const std::optional<std::string>& log_file_path,
    const PlacementSearchOptions& options) {
  const double effective_timeout = timeout_s.value_or(1.0);
  // One context is a spatial array; multiple contexts require context-aware mapping.
  if (search_kind == PlacementSearchKind::kAutoSA) {
    search_kind = mrrg_ptr->GetMRRGConfig().context_size > 1
                      ? PlacementSearchKind::kSA
                      : PlacementSearchKind::kPlacement2DSA;
  }
  switch (search_kind) {
    case PlacementSearchKind::kSA:
      return sa_context::ModuloSAEngine(*dfg_ptr, *mrrg_ptr, search_kind,
                                              effective_timeout, log_file_path,
                                              options)
          .Run();
    case PlacementSearchKind::kPlacement2DSA:
      return sa_spatial::Placement2DSAEngine(
                 *dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                 log_file_path, options)
          .Run();
    default: throw std::invalid_argument("invalid sa search variant");
  }
}
}  // namespace mapper::detail
