#pragma once

#include <entity/mapping.hpp>

namespace mapper {
struct MappingResult {
  MappingResult(bool is_success, const entity::Mapping& mapping,
                double mapping_time_s)
      : is_success(is_success),
        mapping_ptr(std::make_shared<entity::Mapping>(mapping)),
        mapping_time_s(mapping_time_s){};

  bool is_success;
  std::shared_ptr<entity::Mapping> mapping_ptr;
  double mapping_time_s;
};

class IILPMapper {
 public:
  virtual IILPMapper* CreateMapper(
      const std::shared_ptr<entity::DFG> dfg_ptr,
      const std::shared_ptr<entity::MRRG> mrrg_ptr) = 0;
  virtual MappingResult Execution() = 0;
};
}  // namespace mapper