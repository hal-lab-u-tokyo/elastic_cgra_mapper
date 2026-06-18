#include "remapper/remapper.hpp"

#include <Eigen/Eigen>
#include <chrono>

#include "remapper/algorithm/dp_and_full_search_elastic_remapper.hpp"
#include "remapper/algorithm/dp_remapper.hpp"
#include "remapper/algorithm/full_search_remapper.hpp"
#include "remapper/algorithm/greedy_remapper.hpp"
#include "remapper/algorithm_entity.hpp"
#include "time.h"

std::string remapper::RemappingModeToString(RemappingMode mode) {
  switch (mode) {
    case RemappingMode::kFullSearch:
      return "full_search";
    case RemappingMode::kGreedy:
      return "greedy";
    case RemappingMode::kDP:
      return "dp";
    case RemappingMode::kDPAndFullSearch:
      return "dp_and_full_search";
  }
}

remapper::RemappingMode remapper::RemappingModeFromString(
    const std::string& mode_str) {
  if (mode_str == "full_search") {
    return RemappingMode::kFullSearch;
  } else if (mode_str == "greedy") {
    return RemappingMode::kGreedy;
  } else if (mode_str == "dp") {
    return RemappingMode::kDP;
  } else if (mode_str == "dp_and_full_search") {
    return RemappingMode::kDPAndFullSearch;
  } else {
    assert(false);
  }
}

remapper::RemappingResult::RemappingResult(
    const std::vector<int>& result_mapping_id_vec,
    const std::vector<remapper::MappingTransformOp> result_transform_op_vec)
    : result_mapping_id_vec(result_mapping_id_vec),
      result_transform_op_vec(result_transform_op_vec) {
  remapping_time_s = -1;
  assert(result_mapping_id_vec.size() == result_transform_op_vec.size());
};

remapper::RemappingResult remapper::Remapper::Remapping(
    const std::vector<entity::Mapping>& mapping_vec,
    const entity::MRRGConfig& target_mrrg_config, const int target_parallel_num,
    std::ofstream& log_file, remapper::RemappingMode mode, double timeout_s) {
  std::vector<remapper::MappingMatrix> mapping_matrix_vec;
  for (size_t mapping_id = 0; mapping_id < mapping_vec.size(); ++mapping_id) {
    const auto& mapping = mapping_vec[mapping_id];
    mapping_matrix_vec.emplace_back(mapping, static_cast<int>(mapping_id),
                                    target_mrrg_config);
  }
  remapper::CGRAMatrix cgra_matrix(target_mrrg_config);

  RemappingResult result;
  const auto start_time = std::chrono::system_clock::now();
  switch (mode) {
    case RemappingMode::kFullSearch:
      result = remapper::FullSearchRemapping(mapping_matrix_vec, cgra_matrix,
                                             target_parallel_num, log_file,
                                             timeout_s);
      break;
    case RemappingMode::kGreedy:
      result = remapper::GreedyRemapping(mapping_matrix_vec, cgra_matrix,
                                         target_parallel_num, log_file);
      break;
    case RemappingMode::kDP:
      result = remapper::DPRemapping(mapping_matrix_vec, cgra_matrix,
                                     target_parallel_num, log_file);
      break;
    case RemappingMode::kDPAndFullSearch:
      result = remapper::DPAndFullSearchElasticRemapping(
          mapping_matrix_vec, cgra_matrix, target_parallel_num, log_file);
      break;
  }
  const auto end_time = std::chrono::system_clock::now();
  result.remapping_time_s =
      static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
                              end_time - start_time)
                              .count()) /
      1000.0;

  return result;
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
  log_file << "rotation: " << static_cast<int>(transform_op.rotate_op)
           << std::endl;
}
