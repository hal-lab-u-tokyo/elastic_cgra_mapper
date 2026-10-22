#include <mapper/yott_core/engine/array/yott_core_array_engine.hpp>
#include <stdexcept>
#include <utility>

namespace mapper::yott_core {
// The frontend owns the execution order; the engine owns algorithm operations
// and placement/routing state. Explicit variants share this workflow.
template <typename Engine>
class YottCoreArrayMapper {
 public:
  template <typename... Args>
  explicit YottCoreArrayMapper(Args&&... args)
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
}  // namespace mapper::yott_core
namespace mapper::yott_core {
MappingResult RunYottCoreArray(entity::DFG& dfg, entity::MRRG& mrrg,
                               Placement2DArrayKind kind,
                               const Placement2DArrayOptions& options) {
  switch (kind) {
    case mapper::yott_core::Placement2DArrayKind::kCPUMappingYOTTCore:
      return YottCoreArrayMapper<yott_core_array::Placement2DYOTTCoreEngine>(
                 dfg, mrrg, kind, options)
          .Execution();
    default:
      throw std::invalid_argument("invalid yott_core array variant");
  }
}
}  // namespace mapper::yott_core
