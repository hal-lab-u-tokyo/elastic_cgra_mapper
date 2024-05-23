#pragma once
#include <Eigen/Eigen>
#include <entity/mapping.hpp>
#include <fstream>
#include <remapper/mapping_transform_op.hpp>
#include <string>

namespace remapper {
enum RemappingMode { FullSearch, Greedy, DP, DPAndFullSearch };

std::string RemappingModeToString(RemappingMode mode);

RemappingMode RemappingModeFromString(const std::string& mode_str);

struct RemappingResult {
  RemappingResult()
      : remapping_time_s(0),
        result_mapping_id_vec(),
        result_transform_op_vec(){};
  RemappingResult(
      const std::vector<int>& result_mapping_id_vec,
      const std::vector<remapper::MappingTransformOp> result_transform_op_vec);

  double remapping_time_s;
  std::vector<int> result_mapping_id_vec;
  std::vector<remapper::MappingTransformOp> result_transform_op_vec;
};

class Remapper {
 public:
  static RemappingResult ElasticRemapping(
      const std::vector<entity::Mapping>& mapping_vec,
      const entity::MRRGConfig& target_mrrg_config,
      const int target_parallel_num, std::ofstream& log_file,
      RemappingMode mode, double timeout_s);
};

void OutputToLogFile(entity::MRRGConfig mapping_mrrg_config,
                     remapper::MappingTransformOp transform_op,
                     std::ofstream& log_file);

}  // namespace remapper
