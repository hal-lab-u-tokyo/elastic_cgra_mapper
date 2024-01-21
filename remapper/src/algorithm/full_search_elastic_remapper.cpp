#include <remapper/algorithm/full_search_elastic_remapper.hpp>
#include <remapper/combination_counter.hpp>
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

remapper::RemappingResult remapper::FullSearchElasticRemapping(
    std::vector<remapper::MappingMatrix> mapping_matrix_vec,
    const remapper::CGRAMatrix& cgra_matrix, const int target_parallel_num,
    std::ofstream& log_file) {
  std::vector<int> max_search_id(mapping_matrix_vec.size());
  std::vector<std::vector<Eigen::MatrixXi>>
      mapping_id_and_rotation_id_to_matrix;

  std::vector<std::vector<int>>
      mapping_id_and_restricted_search_id_to_search_id;

  for (size_t mapping_id = 0; mapping_id < mapping_matrix_vec.size();
       mapping_id++) {
    const auto& tmp_mapping_config =
        mapping_matrix_vec[mapping_id].GetMapping().GetMRRGConfig();
    const int row_search_width =
        cgra_matrix.row_size - tmp_mapping_config.row + 1;
    const int column_search_width =
        cgra_matrix.column_size - tmp_mapping_config.column + 1;

    max_search_id[mapping_id] = row_search_width * column_search_width * 4 - 1;
    if (cgra_matrix.GetMRRGConfig().memory_io ==
        entity::MRRGMemoryIOType::kBothEnds) {
      std::vector<int> restricted_search_id;
      for (size_t search_id = 0; search_id < max_search_id[mapping_id];
           search_id++) {
        const auto transform_op = CreateMappingTransformOpFromSearchId(
            mapping_matrix_vec[mapping_id].GetMapping(),
            cgra_matrix.GetMRRGConfig(), search_id);
        bool is_available_mapping = cgra_matrix.IsAvailableRemapping(
            mapping_matrix_vec[mapping_id], transform_op);

        if (is_available_mapping) {
          restricted_search_id.push_back(search_id);
        }
      }
      mapping_id_and_restricted_search_id_to_search_id.push_back(
          restricted_search_id);
      max_search_id[mapping_id] = restricted_search_id.size() - 1;
    }
  }

  for (size_t mapping_id = 0; mapping_id < mapping_matrix_vec.size();
       mapping_id++) {
    std::vector<Eigen::MatrixXi> tmp_matrix_vec;
    for (size_t rotate_id = 0; rotate_id < 4; rotate_id++) {
      const auto rotated_mapping_matrix =
          mapping_matrix_vec[mapping_id].GetRotatedOpNumMatrix(
              static_cast<remapper::RotateOp>(rotate_id));
      tmp_matrix_vec.push_back(rotated_mapping_matrix);
    }
    mapping_id_and_rotation_id_to_matrix.push_back(tmp_matrix_vec);
  }

  remapper::CombinationCounter selected_mapping_combination(
      mapping_matrix_vec.size() - 1, target_parallel_num);
  int mapping_group_id = 0;

  RemappingResult best_remapping_result;
  // select mapping
  while (1) {
    const auto selected_mapping_id_vec =
        selected_mapping_combination.GetCombination();
    std::vector<remapper::MappingMatrix> selected_mapping_matrix_vec(
        target_parallel_num);
    std::vector<int> max_selected_search_id_vec(target_parallel_num);
    for (int i = 0; i < target_parallel_num; i++) {
      selected_mapping_matrix_vec[i] =
          mapping_matrix_vec[selected_mapping_id_vec[i]];
      max_selected_search_id_vec[i] = max_search_id[selected_mapping_id_vec[i]];
    }

    // shift and rotate mapping
    remapper::CombinationCounter selected_search_id_combination(
        max_selected_search_id_vec, selected_mapping_id_vec);

    while (1) {
      std::vector<int> selected_search_id_vec =
          selected_search_id_combination.GetCombination();
      Eigen::MatrixXi op_num_matrix =
          Eigen::MatrixXi::Zero(cgra_matrix.row_size, cgra_matrix.column_size);
      bool over_context_size = false;
      std::vector<remapper::MappingTransformOp> transform_op_vec;
      int last_mapping_num = 0;
      for (int i = 0; i < target_parallel_num; i++) {
        last_mapping_num = i;
        int search_id = selected_search_id_vec[i];
        if (cgra_matrix.GetMRRGConfig().memory_io ==
            entity::MRRGMemoryIOType::kBothEnds) {
          search_id = mapping_id_and_restricted_search_id_to_search_id
              [selected_mapping_id_vec[i]][search_id];
        }
        const auto transform_op = CreateMappingTransformOpFromSearchId(
            selected_mapping_matrix_vec[i].GetMapping(),
            cgra_matrix.GetMRRGConfig(), search_id);

        int rotate_id = static_cast<int>(transform_op.rotate_op);
        auto tmp_matrix =
            mapping_id_and_rotation_id_to_matrix[selected_mapping_id_vec[i]]
                                                [rotate_id];
        op_num_matrix.block(transform_op.row, transform_op.column,
                            tmp_matrix.rows(), tmp_matrix.cols()) += tmp_matrix;

        int max_op_num = op_num_matrix.maxCoeff();
        over_context_size = max_op_num > cgra_matrix.context_size;
        if (over_context_size) break;
        transform_op_vec.push_back(transform_op);
      }

      if (!over_context_size) {
        for (int i = 0; i < transform_op_vec.size(); i++) {
          remapper::OutputToLogFile(
              mapping_matrix_vec[selected_mapping_id_vec[i]]
                  .GetMapping()
                  .GetMRRGConfig(),
              transform_op_vec[i], log_file);
        }
        return remapper::RemappingResult(selected_mapping_id_vec,
                                         transform_op_vec);
      }

      if (best_remapping_result.result_transform_op_vec.size() <
          transform_op_vec.size()) {
        std::vector<int> result_mapping_id_vec(
            selected_mapping_id_vec.begin(),
            selected_mapping_id_vec.begin() + transform_op_vec.size());
        best_remapping_result =
            remapper::RemappingResult(result_mapping_id_vec, transform_op_vec);
      }

      // update search_id
      bool test_all_remapping =
          !(selected_search_id_combination.Next(std::max(last_mapping_num - 1, 0), 1));
      if (test_all_remapping) {
        break;
      }
    };

    // update selected mapping
    bool test_all_mapping_combination = !(selected_mapping_combination.Next());
    if (test_all_mapping_combination) {
      break;
    }

    mapping_group_id++;
  }
  for (int i = 0; i < best_remapping_result.result_transform_op_vec.size();
       i++) {
    remapper::OutputToLogFile(
        mapping_matrix_vec[best_remapping_result.result_mapping_id_vec[i]]
            .GetMapping()
            .GetMRRGConfig(),
        best_remapping_result.result_transform_op_vec[i], log_file);
  }
  return best_remapping_result;
};