#pragma once
#include <Eigen/Eigen>
#include <entity/mapping.hpp>
#include <fstream>
#include <remapper/mapping_transform_op.hpp>

namespace remapper {
enum RemappingMode { FullSearch, Greedy, DP };

struct RemappingResult {
  RemappingResult() : result_mapping_id_vec(), result_transform_op_vec(){};
  RemappingResult(
      const std::vector<int>& result_mapping_id_vec,
      const std::vector<remapper::MappingTransformOp> result_transform_op_vec);

  std::vector<int> result_mapping_id_vec;
  std::vector<remapper::MappingTransformOp> result_transform_op_vec;
};

class Remapper {
 public:
  static RemappingResult ElasticRemapping(
      const std::vector<entity::Mapping>& mapping_vec,
      const entity::MRRGConfig& target_mrrg_config,
      const int target_parallel_num, std::ofstream& log_file,
      RemappingMode mode);
};
}  // namespace remapper