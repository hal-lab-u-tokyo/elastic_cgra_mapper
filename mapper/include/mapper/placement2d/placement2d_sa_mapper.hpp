#pragma once

#include <mapper/placement2d/placement2d_array_mapper_base.hpp>
#include <mapper/placement2d/placement2d_search_mapper_base.hpp>
#include <memory>

namespace mapper {

class Placement2DSAMapper : public Placement2DSearchMapperBase {
 public:
  Placement2DSAMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                      const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DSearchMapperBase(detail::Placement2DSearchKind::kSA, dfg_ptr,
                                    mrrg_ptr) {}
};

class Placement2DArraySAMapper : public Placement2DArrayMapperBase {
 public:
  Placement2DArraySAMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                           const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DArrayMapperBase(Placement2DArrayKind::kSA, dfg_ptr,
                                   mrrg_ptr) {}
};

}  // namespace mapper
