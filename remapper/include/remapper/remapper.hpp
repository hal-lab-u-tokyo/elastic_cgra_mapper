#pragma once
#include <entity/mapping.hpp>
#include <fstream>

namespace remapper {
enum RemappingMode { FullSearch, Greedy, DP };

class Remapper {
 public:
  static std::pair<bool, entity::Mapping> ElasticRemapping(
      const std::vector<entity::Mapping>& mapping_vec,
      const entity::MRRGConfig& target_mrrg_config,
      const int target_parallel_num, std::ofstream& log_file,
      RemappingMode mode);

 private:
  static std::pair<bool, entity::Mapping> FullSearchElasticRemapping(
      const std::vector<entity::Mapping>& mapping_vec,
      const entity::MRRGConfig& target_mrrg_config,
      const int target_parallel_num, std::ofstream& log_file);
  static std::pair<bool, entity::Mapping> GreedyElasticRemapping(
      const std::vector<entity::Mapping>& mapping_vec,
      const entity::MRRGConfig& target_mrrg_config,
      const int target_parallel_num, std::ofstream& log_file);
  static std::pair<bool, entity::Mapping> DPElasticRemapping(
      const std::vector<entity::Mapping>& mapping_vec,
      const entity::MRRGConfig& target_mrrg_config,
      const int target_parallel_num, std::ofstream& log_file);
};
}  // namespace remapper