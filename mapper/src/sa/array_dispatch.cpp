#include <stdexcept>
#include "array/engine.hpp"
namespace mapper::detail {
MappingResult RunSaArray(
    entity::DFG& dfg, entity::MRRG& mrrg, Placement2DArrayKind kind,
    const Placement2DArrayOptions& options) {
  switch (kind) {
    case Placement2DArrayKind::kSA:
      return sa_array::Placement2DSAEngine(dfg, mrrg, kind, options)
          .Run();
    default: throw std::invalid_argument("invalid sa array variant");
  }
}
}  // namespace mapper::detail
