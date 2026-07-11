#pragma once

#include <mapper/placement2d/placement2d_array_mapper_base.hpp>
#include <memory>

namespace mapper {

// YOTTCore Repair:
// construct compact YOTTCore placements, repair their bottleneck edges, then
// accept only local moves that improve the complete FIFO-tail profile.
class Placement2DCPUMappingYOTTCoreRepairMapper
    : public Placement2DArrayMapperBase {
 public:
  Placement2DCPUMappingYOTTCoreRepairMapper(
      const std::shared_ptr<entity::DFG> dfg_ptr,
      const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DArrayMapperBase(
            Placement2DArrayKind::kCPUMappingYOTTCoreRepair, dfg_ptr,
            mrrg_ptr) {}
};

}  // namespace mapper
