#include <stdexcept>
#include "array/engine.hpp"
namespace mapper::detail {
MappingResult RunYottArray(
    entity::DFG& dfg, entity::MRRG& mrrg, Placement2DArrayKind kind,
    const Placement2DArrayOptions& options) {
  switch (kind) {
    case Placement2DArrayKind::kPaperGuidedYOTT:
      return yott_array::Placement2DYOTTEngine(dfg, mrrg, kind,
                                                           options)
          .Run();
    case Placement2DArrayKind::kCPUMappingYOTT:
      return yott_array::Placement2DYOTTEngine(dfg, mrrg, kind,
                                                           options)
          .Run();
    default: throw std::invalid_argument("invalid yott array variant");
  }
}
}  // namespace mapper::detail
