#pragma once

#include <entity/mapping.hpp>
#include <entity/mrrg.hpp>

namespace entity {
class Database {
 public:
  void AddMapping(const Mapping& mapping) { mapping_vec_.push_back(mapping); }
  const std::vector<Mapping>& GetMappings() const { return mapping_vec_; }
  int GetMinMappingOpNum() const;
  void LimitMappingNum(int mapping_num_limit);

 private:
  std::vector<Mapping> mapping_vec_;
  MRRGConfig mrrg_config_;
};
}  // namespace entity
