#include <stdexcept>
#include "array/engine.hpp"
namespace mapper::detail {
MappingResult RunPrisaArray(
    entity::DFG& dfg, entity::MRRG& mrrg, Placement2DArrayKind kind,
    const Placement2DArrayOptions& options) {
  switch (kind) {
    case Placement2DArrayKind::kPRISA:
      return prisa_array::Placement2DPRISAEngine(dfg, mrrg, kind,
                                                             options)
          .Run();
    case Placement2DArrayKind::kPRISANoSIS:
      return prisa_array::Placement2DPRISAEngine(dfg, mrrg, kind,
                                                             options)
          .Run();
    case Placement2DArrayKind::kCostAwarePRISA:
      return prisa_array::Placement2DPRISAEngine(dfg, mrrg, kind,
                                                             options)
          .Run();
    default: throw std::invalid_argument("invalid prisa array variant");
  }
}
}  // namespace mapper::detail
