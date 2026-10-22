#include <mapper/yott/engine/array/yott_array_engine.hpp>
#include <mapper/yott/engine/context/yott_context_engine.hpp>
#include <mapper/yott/engine/spatial/yott_spatial_engine.hpp>
#include <mapper/yott/yott_options.hpp>
#include <stdexcept>
#include <utility>

namespace mapper::yott {
// The frontend owns the execution order; the engine owns algorithm operations
// and placement/routing state. Explicit variants share this workflow.
template <typename Engine>
class YottArrayMapper {
 public:
  template <typename... Args>
  explicit YottArrayMapper(Args&&... args)
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
class YottRoutedMapper {
 public:
  template <typename... Args>
  explicit YottRoutedMapper(Args&&... args)
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
}  // namespace mapper::yott
namespace mapper::yott {
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
      return YottRoutedMapper<yott_context::ModuloYOTTEngine>(
                 *dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                 log_file_path, options)
          .Execution();
    case PlacementSearchKind::kModuloPhysicalYOTT:
      return YottRoutedMapper<yott_context::ModuloYOTTEngine>(
                 *dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                 log_file_path, options)
          .Execution();
    case PlacementSearchKind::kYOTTWithFallback:
      return YottRoutedMapper<yott_context::ModuloYOTTEngine>(
                 *dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                 log_file_path, options)
          .Execution();
    case PlacementSearchKind::kPlacement2DYOTT:
      return YottRoutedMapper<yott_spatial::Placement2DYOTTEngine>(
                 *dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                 log_file_path, options)
          .Execution();
    default:
      throw std::invalid_argument("invalid yott search variant");
  }
}
}  // namespace mapper::yott

namespace mapper::yott {
MappingResult RunYottArray(entity::DFG& dfg, entity::MRRG& mrrg,
                           Placement2DArrayKind kind,
                           const Placement2DArrayOptions& options) {
  switch (kind) {
    case mapper::yott::Placement2DArrayKind::kPaperGuidedYOTT:
      return YottArrayMapper<yott_array::Placement2DYOTTEngine>(dfg, mrrg, kind,
                                                                options)
          .Execution();
    case mapper::yott::Placement2DArrayKind::kCPUMappingYOTT:
      return YottArrayMapper<yott_array::Placement2DYOTTEngine>(dfg, mrrg, kind,
                                                                options)
          .Execution();
    default:
      throw std::invalid_argument("invalid yott array variant");
  }
}
}  // namespace mapper::yott
