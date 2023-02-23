#pragma once

#include <mapper/mapper.hpp>

namespace mapper {
class GurobiILPMapper : public IILPMapper {
 public:
  GurobiILPMapper() {}
  GurobiILPMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                  const std::shared_ptr<entity::MRRG> mmrg_ptr);
  GurobiILPMapper* CreateMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                           const std::shared_ptr<entity::MRRG> mmrg_ptr);
  std::pair<bool, entity::Mapping> Execution();

 private:
  std::shared_ptr<entity::DFG> dfg_ptr_;
  std::shared_ptr<entity::MRRG> mrrg_ptr_;
};
}  // namespace mapper
