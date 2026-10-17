#include <stdexcept>
#include <utility>

#include "engine/array/engine.hpp"

namespace mapper::detail {
// The frontend owns the execution order; the engine owns algorithm operations
// and placement/routing state. Explicit variants share this workflow.
template <typename Engine>
class YottCoreRepairArrayMapper {
 public:
  template <typename... Args>
  explicit YottCoreRepairArrayMapper(Args&&... args)
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
}  // namespace mapper::detail
namespace mapper::detail {
MappingResult RunYottCoreRepairArray(entity::DFG& dfg, entity::MRRG& mrrg,
                                     Placement2DArrayKind kind,
                                     const Placement2DArrayOptions& options) {
  switch (kind) {
    case Placement2DArrayKind::kCPUMappingYOTTCoreRepair:
      return YottCoreRepairArrayMapper<
                 yott_core_repair_array::Placement2DYOTTCoreRepairEngine>(
                 dfg, mrrg, kind, options)
          .Execution();
    default:
      throw std::invalid_argument("invalid yott_core_repair array variant");
  }
}
}  // namespace mapper::detail
