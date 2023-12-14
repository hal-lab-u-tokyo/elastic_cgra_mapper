#pragma once
#include <Eigen/Eigen>
#include <entity/mapping.hpp>
#include <fstream>
#include <remapper/mapping_transform_op.hpp>

namespace remapper {
enum RemappingMode { FullSearch, Greedy, DP };

class Remapper {
 public:
  static std::pair<bool, entity::Mapping> ElasticRemapping(
      const std::vector<entity::Mapping>& mapping_vec,
      const entity::MRRGConfig& target_mrrg_config,
      const int target_parallel_num, std::ofstream& log_file,
      RemappingMode mode);
};

Eigen::MatrixXi CreateMatrixForElastic(const entity::Mapping& mapping);
Eigen::MatrixXi CreateMatrixForElastic(
    const entity::Mapping& mapping,
    const entity::MRRGConfig& target_mrrg_config,
    const remapper::MappingTransformOp transform_op);

struct MappingRectangle {
  int row;
  int column;
  int config;
  double op_rate;
  double num_waste_of_memory_io;
  int id;

  MappingRectangle(int _id, const Eigen::MatrixXi& _matrix,
                   const entity::Mapping& mapping);
};

bool IsAvailableRemapping(const entity::Mapping& mapping, int row_shift,
                          int column_shift,
                          const entity::MRRGConfig& target_mrrg_config);
}  // namespace remapper