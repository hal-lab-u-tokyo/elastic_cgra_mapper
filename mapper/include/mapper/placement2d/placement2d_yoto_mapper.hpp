#pragma once

#include <mapper/placement2d/placement2d_array_mapper_base.hpp>
#include <mapper/placement2d/placement2d_search_mapper_base.hpp>
#include <memory>

namespace mapper {

class Placement2DYOTOMapper : public Placement2DSearchMapperBase {
 public:
  Placement2DYOTOMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                        const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DSearchMapperBase(detail::Placement2DSearchKind::kYOTO,
                                    dfg_ptr, mrrg_ptr) {}
};

class Placement2DPaperGuidedArrayYOTOMapper
    : public Placement2DArrayMapperBase {
 public:
  Placement2DPaperGuidedArrayYOTOMapper(
      const std::shared_ptr<entity::DFG> dfg_ptr,
      const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DArrayMapperBase(Placement2DArrayKind::kPaperGuidedYOTO,
                                   dfg_ptr, mrrg_ptr) {}
};

class Placement2DCPUMappingYOTOMapper : public Placement2DArrayMapperBase {
 public:
  Placement2DCPUMappingYOTOMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                                  const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DArrayMapperBase(Placement2DArrayKind::kCPUMappingYOTO,
                                   dfg_ptr, mrrg_ptr) {}
};

}  // namespace mapper
