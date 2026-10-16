#pragma once

#include <mapper/detail/placement2d_array_mapper_base.hpp>
#include <mapper/detail/search_mapper_base.hpp>
#include <memory>

namespace mapper {

class Placement2DSAMapper : public SearchMapperBase {
 public:
  Placement2DSAMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                      const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : SearchMapperBase(detail::SearchKind::kSA, dfg_ptr,
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
