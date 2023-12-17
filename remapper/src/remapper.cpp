#include "remapper/remapper.hpp"

#include <Eigen/Eigen>

#include "remapper/algorithm/dp_elastic_remapper.hpp"
#include "remapper/algorithm/full_search_elastic_remapper.hpp"
#include "remapper/algorithm/greedy_elastic_remapper.hpp"
#include "remapper/algorithm_entity.hpp"
#include "time.h"

remapper::RemappingResult::RemappingResult(
    const std::vector<int>& result_mapping_id_vec,
    const std::vector<remapper::MappingTransformOp> result_transform_op_vec)
    : result_mapping_id_vec(result_mapping_id_vec),
      result_transform_op_vec(result_transform_op_vec) {
  assert(result_mapping_id_vec.size() == result_transform_op_vec.size());
};

remapper::RemappingResult remapper::Remapper::ElasticRemapping(
    const std::vector<entity::Mapping>& mapping_vec,
    const entity::MRRGConfig& target_mrrg_config, const int target_parallel_num,
    std::ofstream& log_file, remapper::RemappingMode mode) {
  std::vector<remapper::MappingMatrix> mapping_matrix_vec;
  for (size_t mapping_id = 0; mapping_id < mapping_vec.size(); ++mapping_id) {
    const auto& mapping = mapping_vec[mapping_id];
    mapping_matrix_vec.emplace_back(mapping, static_cast<int>(mapping_id));
  }
  remapper::CGRAMatrix cgra_matrix(target_mrrg_config);

  switch (mode) {
    case RemappingMode::FullSearch:
      return remapper::FullSearchElasticRemapping(
          mapping_matrix_vec, cgra_matrix, target_parallel_num, log_file);
    case RemappingMode::Greedy:
      return remapper::GreedyElasticRemapping(mapping_matrix_vec, cgra_matrix,
                                              target_parallel_num, log_file);
    case RemappingMode::DP:
      return remapper::DPElasticRemapping(mapping_matrix_vec, cgra_matrix,
                                          target_parallel_num, log_file);
  }
}

void remapper::OutputToLogFile(entity::MRRGConfig mapping_mrrg_config,
                               remapper::MappingTransformOp transform_op,
                               std::ofstream& log_file) {
  log_file << "----- mapping -----" << std::endl;
  log_file << "row: " << mapping_mrrg_config.row << std::endl;
  log_file << "column: " << mapping_mrrg_config.column << std::endl;
  log_file << "context_size: " << mapping_mrrg_config.context_size << std::endl;
  log_file << "row shift: " << transform_op.row << std::endl;
  log_file << "column shift: " << transform_op.column << std::endl;
  log_file << "rotation: " << transform_op.rotate_op << std::endl;
}