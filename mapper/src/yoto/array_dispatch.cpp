#include <stdexcept>
#include "array/engine.hpp"
namespace mapper::detail {
MappingResult RunYotoArray(
    entity::DFG& dfg, entity::MRRG& mrrg, Placement2DArrayKind kind,
    const Placement2DArrayOptions& options) {
  switch (kind) {
    case Placement2DArrayKind::kPaperGuidedYOTO:
      return yoto_array::Placement2DYOTOEngine(dfg, mrrg, kind,
                                                           options)
          .Run();
    case Placement2DArrayKind::kCPUMappingYOTO:
      return yoto_array::Placement2DYOTOEngine(dfg, mrrg, kind,
                                                           options)
          .Run();
    default: throw std::invalid_argument("invalid yoto array variant");
  }
}
}  // namespace mapper::detail
