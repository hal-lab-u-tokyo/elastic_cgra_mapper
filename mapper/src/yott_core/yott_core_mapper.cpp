#include <mapper/yott_core/engine/yott_core_engine.hpp>
#include <stdexcept>

namespace mapper::yott_core {
MappingResult RunYottCoreArray(entity::DFG& dfg, entity::MRRG& mrrg,
                               Placement2DArrayKind kind,
                               const Placement2DArrayOptions& options) {
  if (kind != Placement2DArrayKind::kCPUMappingYOTTCore)
    throw std::invalid_argument("invalid yott_core variant");
  YottCoreEngine engine(dfg, mrrg, kind, options);
  const auto start = engine.StartSearch();
  auto placement = engine.SearchPlacement(start);
  if (!placement) return engine.Failure(start);
  return engine.FinishPlacement(*placement, start);
}
}  // namespace mapper::yott_core
