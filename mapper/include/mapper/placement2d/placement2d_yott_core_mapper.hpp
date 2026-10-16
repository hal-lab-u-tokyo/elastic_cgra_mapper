#pragma once

#include <mapper/placement2d/placement2d_array_mapper_base.hpp>

#include <memory>

namespace mapper {

// Minimal YOTT-derived baseline:
// random local traversal, YOTT reconvergence annotations, tip candidates,
// nearest fallback, and best-of-trials placement-cost selection.
class Placement2DCPUMappingYOTTCoreMapper : public Placement2DArrayMapperBase {
 public:
  Placement2DCPUMappingYOTTCoreMapper(
      const std::shared_ptr<entity::DFG> dfg_ptr,
      const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DArrayMapperBase(
            Placement2DArrayKind::kCPUMappingYOTTCore, dfg_ptr,
            mrrg_ptr) {}
};

}  // namespace mapper
