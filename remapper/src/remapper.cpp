#include "remapper/remapper.hpp"

#include <Eigen/Eigen>

#include "remapper/combination_counter.hpp"
#include "remapper/mapping_concater.hpp"
#include "remapper/mapping_transform_op.hpp"
#include "remapper/rotater.hpp"
#include "time.h"

remapper::MappingTransformOp CreateMappingTransformOpFromSearchId(
    const entity::Mapping& mapping,
    const entity::MRRGConfig& target_mrrg_config, int search_id) {
  const entity::MRRGConfig mrrg_config = mapping.GetMRRGConfig();

  const int row_search_width = target_mrrg_config.row - mrrg_config.row + 1;
  const int column_search_width =
      target_mrrg_config.column - mrrg_config.column + 1;

  const int row_position = (search_id / (4 * column_search_width));
  assert(row_position < row_search_width);

  const int column_position = (search_id / 4) % column_search_width;
  const remapper::RotateOp rotation_op =
      static_cast<remapper::RotateOp>(search_id % 4);

  return remapper::MappingTransformOp(row_position, column_position,
                                      rotation_op);
}

Eigen::MatrixXi CreateMatrixForElastic(
    const entity::Mapping& mapping,
    const entity::MRRGConfig& target_mrrg_config,
    const remapper::MappingTransformOp transform_op) {
  Eigen::MatrixXi matrix =
      Eigen::MatrixXi::Zero(target_mrrg_config.row, target_mrrg_config.column);
  const entity::Mapping rotated_mapping =
      remapper::MappingRotater(mapping, transform_op.rotate_op);

  for (int i = 0; i < rotated_mapping.GetMRRGConfig().row; i++) {
    for (int j = 0; j < rotated_mapping.GetMRRGConfig().column; j++) {
      int op_count = 0;
      for (int k = 0; k < rotated_mapping.GetMRRGConfig().context_size; k++) {
        const entity::ConfigId config_id(i, j, k);
        const auto config = mapping.GetConfig(config_id);
        if (config.operation_type != entity::OpType::NOP) {
          op_count++;
        } else {
          break;
        }
      }
      matrix(i + transform_op.row, j + transform_op.column) = op_count;
    }
  }

  return matrix;
}

std::pair<bool, entity::Mapping> remapper::Remapper::ElasticRemapping(
    const std::vector<entity::Mapping>& mapping_vec,
    const entity::MRRGConfig& target_mrrg_config, const int target_parallel_num,
    std::ofstream& log_file) {
  std::vector<int> max_search_id(mapping_vec.size());

  for (size_t i = 0; i < mapping_vec.size(); i++) {
    const auto& tmp_mapping_config = mapping_vec[i].GetMRRGConfig();
    const int row_search_width =
        target_mrrg_config.row - tmp_mapping_config.row + 1;
    const int column_search_width =
        target_mrrg_config.column - tmp_mapping_config.column + 1;
    max_search_id[i] = row_search_width * column_search_width * 4 - 1;
  }

  remapper::CombinationCounter selected_mapping_combination(
      mapping_vec.size() - 1, target_parallel_num);
  int mapping_group_id = 0;
  // select mapping
  while (1) {
    const auto selected_mapping_id_vec =
        selected_mapping_combination.GetCombination();
    std::vector<entity::Mapping> selected_mapping_vec(target_parallel_num);
    std::vector<int> max_selected_search_id_vec(target_parallel_num);
    for (int i = 0; i < target_parallel_num; i++) {
      selected_mapping_vec[i] = mapping_vec[selected_mapping_id_vec[i]];
      max_selected_search_id_vec[i] = max_search_id[selected_mapping_id_vec[i]];
    }
    log_file << "mapping group id: " << mapping_group_id << std::endl;

    // shift and rotate mapping
    remapper::CombinationCounter selected_search_id_combination(
        max_selected_search_id_vec, selected_mapping_id_vec);

    const auto start_time = clock();
    while (1) {
      std::vector<int> selected_search_id_vec =
          selected_search_id_combination.GetCombination();
      Eigen::MatrixXi op_num_matrix = Eigen::MatrixXi::Zero(
          target_mrrg_config.row, target_mrrg_config.column);
      std::vector<remapper::MappingTransformOp> transform_op_vec(
          target_parallel_num);
      bool over_context_size = false;
      int last_mapping_num = 0;
      for (int i = 0; i < target_parallel_num; i++) {
        last_mapping_num = i;
        const auto transform_op = CreateMappingTransformOpFromSearchId(
            selected_mapping_vec[i], target_mrrg_config,
            selected_search_id_vec[i]);
        transform_op_vec[i] = transform_op;
        op_num_matrix += CreateMatrixForElastic(
            selected_mapping_vec[i], target_mrrg_config, transform_op);

        int max_op_num = op_num_matrix.maxCoeff();
        over_context_size = max_op_num > target_mrrg_config.context_size;
        if (over_context_size) break;
      }

      if (!over_context_size) {
        const auto result_mapping = remapper::MappingConcater(
            selected_mapping_vec, transform_op_vec, target_mrrg_config);
        return {true, result_mapping};
      }

      // update search_id
      bool test_all_remapping =
          !(selected_search_id_combination.Next(last_mapping_num, 1));
      if (test_all_remapping) {
        break;
      }
    };
    const auto end_time = clock();
    log_file << "mapping group search time: "
             << ((double)end_time - start_time) / CLOCKS_PER_SEC << std::endl;

    // update selected mapping
    bool test_all_mapping_combination = !(selected_mapping_combination.Next());
    if (test_all_mapping_combination) {
      break;
    }

    mapping_group_id++;
  }
  entity::Mapping empty;
  return {false, empty};
};