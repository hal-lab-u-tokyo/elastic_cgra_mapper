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

Eigen::MatrixXi CreateMatrixForElastic(const entity::Mapping& mapping) {
  int min_row_id = mapping.GetMRRGConfig().row,
      min_column_id = mapping.GetMRRGConfig().column;
  int max_row_id = 0, max_column_id = 0;
  Eigen::MatrixXi matrix = Eigen::MatrixXi::Zero(mapping.GetMRRGConfig().row,
                         mapping.GetMRRGConfig().column);
  for (int row_id = 0; row_id < mapping.GetMRRGConfig().row; row_id++) {
    for (int column_id = 0; column_id < mapping.GetMRRGConfig().column;
         column_id++) {
      int op_count = 0;
      for (int context_id = 0;
           context_id < mapping.GetMRRGConfig().context_size; context_id++) {
        const entity::ConfigId config_id(row_id, column_id, context_id);
        const auto config = mapping.GetConfig(config_id);
        if (config.operation_type != entity::OpType::NOP) {
          op_count++;
        }
      }
      if (op_count > 0) {
        min_row_id = std::min(row_id, min_row_id);
        max_row_id = std::max(row_id, max_row_id);
        min_column_id = std::min(column_id, min_column_id);
        max_column_id = std::max(column_id, max_column_id);
      }
      matrix(row_id, column_id) = op_count;
    }
  }

  const int row_size = max_row_id - min_row_id + 1;
  const int column_size = max_column_id - min_column_id + 1;
  return matrix.block(min_row_id, min_column_id, row_size, column_size);
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
    std::ofstream& log_file, remapper::RemappingMode mode) {
  switch (mode) {
    case RemappingMode::FullSearch:
      return FullSearchElasticRemapping(mapping_vec, target_mrrg_config,
                                        target_parallel_num, log_file);
    case RemappingMode::Naive:
      return NaiveElasticRemapping(mapping_vec, target_mrrg_config,
                                   target_parallel_num, log_file);
  }
}

std::pair<bool, entity::Mapping> remapper::Remapper::FullSearchElasticRemapping(
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

struct MappingRectangle {
  MappingRectangle(int _id, const Eigen::MatrixXi& _matrix) {
    row = _matrix.rows();
    column = _matrix.cols();
    config = _matrix.maxCoeff();
    op_rate = (double)_matrix.sum() / (row * column * config);
    id = _id;
  }
  int row;
  int column;
  int config;
  double op_rate;
  int id;
};

std::pair<bool, entity::Mapping> remapper::Remapper::NaiveElasticRemapping(
    const std::vector<entity::Mapping>& mapping_vec,
    const entity::MRRGConfig& target_mrrg_config, const int target_parallel_num,
    std::ofstream& log_file) {
  std::vector<MappingRectangle> mapping_rectangle_vec;
  for (size_t i = 0; i < mapping_vec.size(); i++) {
    const auto& mapping = mapping_vec[i];
    const auto mapping_matrix = CreateMatrixForElastic(mapping);
    mapping_rectangle_vec.emplace_back(i, mapping_matrix);
  }
  auto compare = [&](MappingRectangle left, MappingRectangle right) {
    return left.op_rate > right.op_rate;
  };
  std::sort(mapping_rectangle_vec.begin(), mapping_rectangle_vec.end(),
            compare);

  Eigen::MatrixXi target_matrix = Eigen::MatrixXi::Zero(target_mrrg_config.row,
                                target_mrrg_config.column);

  std::vector<entity::Mapping> result_mapping_vec;
  std::vector<remapper::MappingTransformOp> result_transform_op_vec;

  int parallel_num = 0;
  for (const auto& mapping_rectangle : mapping_rectangle_vec) {
    const auto& mapping = mapping_vec[mapping_rectangle.id];
    for (int rotate_id = 0; rotate_id < 4; rotate_id++) {
      const auto rotated_mapping = remapper::MappingRotater(
          mapping, static_cast<remapper::RotateOp>(rotate_id));

      const auto rotated_mapping_matrix =
          CreateMatrixForElastic(rotated_mapping);
      if (target_matrix.rows() < rotated_mapping_matrix.rows() ||
          target_matrix.cols() < rotated_mapping_matrix.cols())
        continue;
      const auto start_time = clock();
      for (int row_shift = 0;
           row_shift < target_matrix.rows() - rotated_mapping_matrix.rows() + 1;
           row_shift++) {
        for (int col_shift = 0;
             col_shift <
             target_matrix.cols() - rotated_mapping_matrix.cols() + 1;
             col_shift++) {
          while (1) {
            Eigen::MatrixXi added_matrix = target_matrix;
            added_matrix.block(
                row_shift, col_shift, rotated_mapping_matrix.rows(),
                rotated_mapping_matrix.cols()) += rotated_mapping_matrix;
            // failed
            if (added_matrix.maxCoeff() > target_mrrg_config.context_size)
              break;

            // success
            target_matrix = added_matrix;
            result_mapping_vec.push_back(mapping);
            result_transform_op_vec.emplace_back(
                row_shift, col_shift,
                static_cast<remapper::RotateOp>(rotate_id));
            parallel_num++;

            if (parallel_num == target_parallel_num) {
              const auto result_mapping = remapper::MappingConcater(
                  result_mapping_vec, result_transform_op_vec,
                  target_mrrg_config);
              return {true, result_mapping};
            }
          }
        }
      }

      const auto end_time = clock();
      log_file << "mapping search time: "
               << ((double)end_time - start_time) / CLOCKS_PER_SEC << std::endl;
    }
  };

  entity::Mapping empty;
  return {false, empty};
}