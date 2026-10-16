#pragma once

#include <mapper/placement2d/placement2d_array_mapper_base.hpp>
#include <mapper/placement2d/placement2d_search_mapper_base.hpp>
#include <memory>

namespace mapper {

class Placement2DYOTTMapper : public Placement2DSearchMapperBase {
 public:
  Placement2DYOTTMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                        const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DSearchMapperBase(detail::Placement2DSearchKind::kYOTT,
                                    dfg_ptr, mrrg_ptr) {}
};

class Placement2DPaperGuidedArrayYOTTMapper
    : public Placement2DArrayMapperBase {
 public:
  Placement2DPaperGuidedArrayYOTTMapper(
      const std::shared_ptr<entity::DFG> dfg_ptr,
      const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DArrayMapperBase(Placement2DArrayKind::kPaperGuidedYOTT,
                                   dfg_ptr, mrrg_ptr) {}
};

class Placement2DCPUMappingYOTTMapper : public Placement2DArrayMapperBase {
 public:
  Placement2DCPUMappingYOTTMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                                  const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DArrayMapperBase(Placement2DArrayKind::kCPUMappingYOTT,
                                   dfg_ptr, mrrg_ptr) {}
};

}  // namespace mapper
