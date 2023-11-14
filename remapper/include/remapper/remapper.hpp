#pragma once
#include <entity/mapping.hpp>

namespace remapper {
class Remapper {
 public:
  static std::pair<bool, entity::Mapping> ElasticRemapping(
      const std::vector<entity::Mapping>& mapping_vec,
      const entity::MRRGConfig& target_mrrg_config,
      const int target_parallel_num);
};
}