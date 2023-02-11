#pragma once

#include <entity/mapping.hpp>

namespace mapper {
class IILPMapper {
 public:
  virtual IILPMapper* CreateMapper(
      const std::shared_ptr<entity::DFG> dfg_ptr,
      const std::shared_ptr<entity::MRRG> mrrg_ptr) = 0;
  virtual entity::Mapping Execution() = 0;
};
}  // namespace mapper