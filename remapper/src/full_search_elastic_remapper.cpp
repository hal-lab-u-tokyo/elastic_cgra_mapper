#include <remapper/combination_counter.hpp>
#include <remapper/full_search_elastic_remapper.hpp>
#include <remapper/mapping_concater.hpp>
#include <remapper/mapping_transform_op.hpp>
#include <remapper/remapper.hpp>
#include <remapper/rotater.hpp>

remapper::MappingTransformOp CreateMappingTransformOpFromSearchId(
    const entity::Mapping& mapping,
    const entity::MRRGConfig& target_mrrg_config, int search_id) {
  const entity::MRRGConfig mrrg_config = mapping.GetMRRGConfig();

  int origin_row_search_width = target_mrrg_config.row - mrrg_config.row + 1;
  int origin_column_search_width =
      target_mrrg_config.column - mrrg_config.column + 1;

  const remapper::RotateOp rotation_op =
      static_cast<remapper::RotateOp>(search_id % 4);
  int row_search_width = origin_row_search_width;
  int column_search_width = origin_column_search_width;
  if (rotation_op == remapper::RotateOp::TopIsLeft ||
      rotation_op == remapper::RotateOp::TopIsRight) {
    row_search_width = origin_column_search_width;
    column_search_width = origin_row_search_width;
  }

  const int row_position = (search_id / (4 * column_search_width));
  assert(row_position < row_search_width);

  const int column_position = (search_id / 4) % column_search_width;
  assert(column_position < column_search_width);

  return remapper::MappingTransformOp(row_position, column_position,
                                      rotation_op);
}

std::pair<bool, entity::Mapping> remapper::FullSearchElasticRemapping(
    const std::vector<entity::Mapping>& mapping_vec,
    const entity::MRRGConfig& target_mrrg_config, const int target_parallel_num,
    std::ofstream& log_file) {
  std::vector<int> max_search_id(mapping_vec.size());
  std::vector<std::vector<Eigen::MatrixXi>>
      mapping_id_and_rotation_id_to_matrix;

  for (size_t i = 0; i < mapping_vec.size(); i++) {
    const auto& tmp_mapping_config = mapping_vec[i].GetMRRGConfig();
    const int row_search_width =
        target_mrrg_config.row - tmp_mapping_config.row + 1;
    const int column_search_width =
        target_mrrg_config.column - tmp_mapping_config.column + 1;
    max_search_id[i] = row_search_width * column_search_width * 4 - 1;
  }

  for (size_t mapping_id = 0; mapping_id < mapping_vec.size(); mapping_id++) {
    std::vector<Eigen::MatrixXi> tmp_matrix_vec;
    for (size_t rotate_id = 0; rotate_id < 4; rotate_id++) {
      const auto rotated_mapping = remapper::MappingRotater(
          mapping_vec[mapping_id], static_cast<remapper::RotateOp>(rotate_id));
      const auto rotated_mapping_matrix =
          remapper::CreateMatrixForElastic(rotated_mapping);
      tmp_matrix_vec.push_back(rotated_mapping_matrix);
    }
    mapping_id_and_rotation_id_to_matrix.push_back(tmp_matrix_vec);
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

        int rotate_id = static_cast<int>(transform_op.rotate_op);
        auto tmp_matrix =
            mapping_id_and_rotation_id_to_matrix[selected_mapping_id_vec[i]]
                                                [rotate_id];
        op_num_matrix.block(transform_op.row, transform_op.column,
                            tmp_matrix.rows(), tmp_matrix.cols()) += tmp_matrix;

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