#include <stdexcept>
#include "array/engine.hpp"
namespace mapper::detail {
MappingResult RunYottCoreRepairArray(
    entity::DFG& dfg, entity::MRRG& mrrg, Placement2DArrayKind kind,
    const Placement2DArrayOptions& options) {
  switch (kind) {
    case Placement2DArrayKind::kCPUMappingYOTTCoreRepair:
      return yott_core_repair_array::
          Placement2DYOTTCoreRepairEngine(dfg, mrrg, kind, options)
              .Run();
    default: throw std::invalid_argument("invalid yott_core_repair array variant");
  }
}
}  // namespace mapper::detail
