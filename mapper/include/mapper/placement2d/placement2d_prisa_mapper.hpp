#pragma once

#include <mapper/placement2d/placement2d_array_mapper_base.hpp>
#include <mapper/placement2d/placement2d_search_mapper_base.hpp>
#include <memory>

namespace mapper {

class Placement2DPRISAMapper : public Placement2DSearchMapperBase {
 public:
  Placement2DPRISAMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                         const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DSearchMapperBase(detail::Placement2DSearchKind::kPRISA,
                                    dfg_ptr, mrrg_ptr) {}
};

class Placement2DPRISANoSISMapper : public Placement2DSearchMapperBase {
 public:
  Placement2DPRISANoSISMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                              const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DSearchMapperBase(detail::Placement2DSearchKind::kPRISANoSIS,
                                    dfg_ptr, mrrg_ptr) {}
};

class Placement2DCostAwarePRISAMapper : public Placement2DSearchMapperBase {
 public:
  Placement2DCostAwarePRISAMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                                  const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DSearchMapperBase(
            detail::Placement2DSearchKind::kCostAwarePRISA, dfg_ptr, mrrg_ptr) {
  }
};

class Placement2DArrayPRISAMapper : public Placement2DArrayMapperBase {
 public:
  Placement2DArrayPRISAMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                              const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DArrayMapperBase(Placement2DArrayKind::kPRISA, dfg_ptr,
                                   mrrg_ptr) {}
};

class Placement2DArrayPRISANoSISMapper : public Placement2DArrayMapperBase {
 public:
  Placement2DArrayPRISANoSISMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                                   const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DArrayMapperBase(Placement2DArrayKind::kPRISANoSIS, dfg_ptr,
                                   mrrg_ptr) {}
};

class Placement2DArrayCostAwarePRISAMapper : public Placement2DArrayMapperBase {
 public:
  Placement2DArrayCostAwarePRISAMapper(
      const std::shared_ptr<entity::DFG> dfg_ptr,
      const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DArrayMapperBase(Placement2DArrayKind::kCostAwarePRISA,
                                   dfg_ptr, mrrg_ptr) {}
};

}  // namespace mapper
