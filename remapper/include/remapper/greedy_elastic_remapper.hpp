#pragma once
#include <entity/mapping.hpp>
#include <fstream>

namespace remapper {
std::pair<bool, entity::Mapping> GreedyElasticRemapping(
      const std::vector<entity::Mapping>& mapping_vec,
      const entity::MRRGConfig& target_mrrg_config,
      const int target_parallel_num, std::ofstream& log_file);
}  