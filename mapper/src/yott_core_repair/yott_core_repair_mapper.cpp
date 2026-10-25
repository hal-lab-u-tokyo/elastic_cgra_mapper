#include <mapper/yott_core_repair/engine/yott_core_repair_engine.hpp>
#include <stdexcept>

namespace mapper::yott_core_repair {
MappingResult RunYottCoreRepairArray(entity::DFG& dfg, entity::MRRG& mrrg,
                                     Placement2DArrayKind kind,
                                     const Placement2DArrayOptions& options) {
  if (kind != Placement2DArrayKind::kCPUMappingYOTTCoreRepair)
    throw std::invalid_argument("invalid yott_core_repair variant");
  YottCoreRepairEngine engine(dfg, mrrg, kind, options);
  const auto start = engine.StartSearch();
  auto placement = engine.SearchPlacement(start);
  if (!placement) return engine.Failure(start);
  return engine.FinishPlacement(*placement, start);
}
}  // namespace mapper::yott_core_repair
