#include <mapper/detail/placement_search_engine.hpp>
#include <stdexcept>
#include <utility>

#include "engine/array/engine.hpp"
#include "engine/context/engine.hpp"
#include "engine/spatial/engine.hpp"

namespace mapper::detail {
// The frontend owns the execution order; the engine owns algorithm operations
// and placement/routing state. Explicit variants share this workflow.
template <typename Engine>
class YotoArrayMapper {
 public:
  template <typename... Args>
  explicit YotoArrayMapper(Args&&... args)
      : engine_(std::forward<Args>(args)...) {}
  MappingResult Execution() {
    const auto start = engine_.StartSearch();
    auto placement = engine_.SearchPlacement(start);
    if (!placement) return engine_.Failure(start);
    return engine_.FinishPlacement(*placement, start);
  }

 private:
  Engine engine_;
};

template <typename Engine>
class YotoRoutedMapper {
 public:
  template <typename... Args>
  explicit YotoRoutedMapper(Args&&... args)
      : engine_(std::forward<Args>(args)...) {}
  MappingResult Execution() {
    const auto start = engine_.StartSearch();
    auto placement = engine_.SearchPlacement(start);
    if (!placement) return engine_.Failure(start);
    if (engine_.PlacementOnly())
      return engine_.FinishPlacementOnly(*placement, start);
    auto routes = engine_.RoutePlacement(*placement);
    if (!routes) return engine_.Failure(start);
    return engine_.FinishRoutedMapping(*placement, *routes, start);
  }

 private:
  Engine engine_;
};
}  // namespace mapper::detail
namespace mapper::detail {
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
      return YotoRoutedMapper<yoto_context::ModuloYOTOEngine>(
                 *dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                 log_file_path, options)
          .Execution();
    case PlacementSearchKind::kModuloPhysicalYOTO:
      return YotoRoutedMapper<yoto_context::ModuloYOTOEngine>(
                 *dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                 log_file_path, options)
          .Execution();
    case PlacementSearchKind::kYOTOWithFallback:
      return YotoRoutedMapper<yoto_context::ModuloYOTOEngine>(
                 *dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                 log_file_path, options)
          .Execution();
    case PlacementSearchKind::kPlacement2DYOTO:
      return YotoRoutedMapper<yoto_spatial::Placement2DYOTOEngine>(
                 *dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                 log_file_path, options)
          .Execution();
    default:
      throw std::invalid_argument("invalid yoto search variant");
  }
}
}  // namespace mapper::detail

namespace mapper::detail {
MappingResult RunYotoArray(entity::DFG& dfg, entity::MRRG& mrrg,
                           Placement2DArrayKind kind,
                           const Placement2DArrayOptions& options) {
  switch (kind) {
    case Placement2DArrayKind::kPaperGuidedYOTO:
      return YotoArrayMapper<yoto_array::Placement2DYOTOEngine>(dfg, mrrg, kind,
                                                                options)
          .Execution();
    case Placement2DArrayKind::kCPUMappingYOTO:
      return YotoArrayMapper<yoto_array::Placement2DYOTOEngine>(dfg, mrrg, kind,
                                                                options)
          .Execution();
    default:
      throw std::invalid_argument("invalid yoto array variant");
  }
}
}  // namespace mapper::detail
