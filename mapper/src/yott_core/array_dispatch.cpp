#include <stdexcept>
#include "array/engine.hpp"
namespace mapper::detail {
MappingResult RunYottCoreArray(
    entity::DFG& dfg, entity::MRRG& mrrg, Placement2DArrayKind kind,
    const Placement2DArrayOptions& options) {
  switch (kind) {
    case Placement2DArrayKind::kCPUMappingYOTTCore:
      return yott_core_array::Placement2DYOTTCoreEngine(
                 dfg, mrrg, kind, options)
          .Run();
    default: throw std::invalid_argument("invalid yott_core array variant");
  }
}
}  // namespace mapper::detail
