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
  Eigen::MatrixXi matrix = Eigen::MatrixXi::Zero(
      mapping.GetMRRGConfig().row, mapping.GetMRRGConfig().column);
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
    case RemappingMode::DP:
      return DPElasticRemapping(mapping_vec, target_mrrg_config,
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
  MappingRectangle(int _id, const Eigen::MatrixXi& _matrix,
                   const entity::Mapping& mapping) {
    row = _matrix.rows();
    column = _matrix.cols();
    config = _matrix.maxCoeff();
    int op_num_without_routing = 0;
    for (const auto& config : mapping.GetConfigMap()) {
      if (config.second.operation_type != entity::OpType::NOP &&
          config.second.operation_type != entity::OpType::ROUTE) {
        op_num_without_routing++;
      }
    }
    op_rate = (double)op_num_without_routing / (row * column * config);
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
    mapping_rectangle_vec.emplace_back(i, mapping_matrix, mapping);
  }
  auto compare = [&](MappingRectangle left, MappingRectangle right) {
    return left.op_rate > right.op_rate;
  };
  std::sort(mapping_rectangle_vec.begin(), mapping_rectangle_vec.end(),
            compare);

  Eigen::MatrixXi target_matrix =
      Eigen::MatrixXi::Zero(target_mrrg_config.row, target_mrrg_config.column);

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
              for (size_t result_id = 0; result_id < result_mapping_vec.size();
                   result_id++) {
                log_file << "----- mapping -----" << std::endl;
                log_file << "row: "
                         << result_mapping_vec[result_id].GetMRRGConfig().row
                         << std::endl;
                log_file << "column: "
                         << result_mapping_vec[result_id].GetMRRGConfig().column
                         << std::endl;
                log_file << "context_size: "
                         << result_mapping_vec[result_id]
                                .GetMRRGConfig()
                                .context_size
                         << std::endl;
                log_file << "row shift: "
                         << result_transform_op_vec[result_id].row << std::endl;
                log_file << "column shift: "
                         << result_transform_op_vec[result_id].column
                         << std::endl;
                log_file << "rotation: "
                         << result_transform_op_vec[result_id].rotate_op
                         << std::endl;
              }

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

std::pair<bool, entity::Mapping> remapper::Remapper::DPElasticRemapping(
    const std::vector<entity::Mapping>& mapping_vec,
    const entity::MRRGConfig& target_mrrg_config, const int target_parallel_num,
    std::ofstream& log_file) {
  std::vector<MappingRectangle> mapping_rectangle_vec;
  for (size_t i = 0; i < mapping_vec.size(); i++) {
    const auto& mapping = mapping_vec[i];
    const auto mapping_matrix = CreateMatrixForElastic(mapping);
    mapping_rectangle_vec.emplace_back(i, mapping_matrix, mapping);
  }
  auto compare = [&](MappingRectangle left, MappingRectangle right) {
    return left.op_rate > right.op_rate;
  };
  std::sort(mapping_rectangle_vec.begin(), mapping_rectangle_vec.end(),
            compare);

  struct MappingIdAndOp {
    MappingIdAndOp(int _id, remapper::MappingTransformOp _op)
        : id(_id), op(_op){};
    int id;
    remapper::MappingTransformOp op;
  };

  std::vector<std::vector<std::vector<u_int16_t>>> dp;
  std::vector<std::vector<std::vector<std::vector<MappingIdAndOp>>>> dp_result;

  // init var for dp
  dp.resize(target_mrrg_config.row + 1);
  dp_result.resize(target_mrrg_config.row + 1);
  for (int row_id = 0; row_id < target_mrrg_config.row + 1; row_id++) {
    dp[row_id].resize(target_mrrg_config.column + 1);
    dp_result[row_id].resize(target_mrrg_config.column + 1);
    for (int col_id = 0; col_id < target_mrrg_config.column + 1; col_id++) {
      dp[row_id][col_id].resize(target_mrrg_config.context_size + 1, 0);
      dp_result[row_id][col_id].resize(target_mrrg_config.context_size + 1);
    }
  }

  for (const auto& mapping_rectangle : mapping_rectangle_vec) {
    const auto& mapping = mapping_vec[mapping_rectangle.id];
    for (int rotate_id = 0; rotate_id < 1; rotate_id++) {
      const auto rotated_mapping = remapper::MappingRotater(
          mapping, static_cast<remapper::RotateOp>(rotate_id));

      const auto rotated_mapping_matrix =
          CreateMatrixForElastic(rotated_mapping);
      if (target_mrrg_config.row < rotated_mapping_matrix.rows() ||
          target_mrrg_config.column < rotated_mapping_matrix.cols() ||
          target_mrrg_config.context_size < rotated_mapping_matrix.maxCoeff())
        continue;

      const auto start_time = clock();
      for (int row_shift = 0; row_shift < target_mrrg_config.row -
                                              rotated_mapping_matrix.rows() + 1;
           row_shift++) {
        for (int col_shift = 0;
             col_shift <
             target_mrrg_config.column - rotated_mapping_matrix.cols() + 1;
             col_shift++) {
          for (int context_shift = 0;
               context_shift < target_mrrg_config.context_size -
                                   rotated_mapping_matrix.maxCoeff() + 1;
               context_shift++) {
            int mapping_row = rotated_mapping_matrix.rows();
            int mapping_column = rotated_mapping_matrix.cols();
            int mapping_context = rotated_mapping_matrix.maxCoeff();

            int dp_row = row_shift + mapping_row;
            int dp_column = col_shift + mapping_column;
            int dp_context = context_shift + mapping_context;

            // 6 pattern test
            int dp_extra_rectangle_size[6][3][3];
            dp_extra_rectangle_size[0][0][0] = row_shift;
            dp_extra_rectangle_size[0][0][1] = dp_column;
            dp_extra_rectangle_size[0][0][2] = mapping_context;
            dp_extra_rectangle_size[0][1][0] = mapping_row;
            dp_extra_rectangle_size[0][1][1] = col_shift;
            dp_extra_rectangle_size[0][1][2] = mapping_context;
            dp_extra_rectangle_size[0][2][0] = dp_row;
            dp_extra_rectangle_size[0][2][1] = dp_column;
            dp_extra_rectangle_size[0][2][2] = context_shift;

            dp_extra_rectangle_size[1][0][0] = row_shift;
            dp_extra_rectangle_size[1][0][1] = mapping_column;
            dp_extra_rectangle_size[1][0][2] = mapping_context;
            dp_extra_rectangle_size[1][1][0] = dp_row;
            dp_extra_rectangle_size[1][1][1] = col_shift;
            dp_extra_rectangle_size[1][1][2] = mapping_context;
            dp_extra_rectangle_size[1][2][0] = dp_row;
            dp_extra_rectangle_size[1][2][1] = dp_column;
            dp_extra_rectangle_size[1][2][2] = context_shift;

            dp_extra_rectangle_size[2][0][0] = row_shift;
            dp_extra_rectangle_size[2][0][1] = mapping_column;
            dp_extra_rectangle_size[2][0][2] = mapping_context;
            dp_extra_rectangle_size[2][1][0] = dp_row;
            dp_extra_rectangle_size[2][1][1] = col_shift;
            dp_extra_rectangle_size[2][1][2] = dp_context;
            dp_extra_rectangle_size[2][2][0] = dp_row;
            dp_extra_rectangle_size[2][2][1] = mapping_column;
            dp_extra_rectangle_size[2][2][2] = context_shift;

            dp_extra_rectangle_size[3][0][0] = row_shift;
            dp_extra_rectangle_size[3][0][1] = mapping_column;
            dp_extra_rectangle_size[3][0][2] = dp_context;
            dp_extra_rectangle_size[3][1][0] = dp_row;
            dp_extra_rectangle_size[3][1][1] = col_shift;
            dp_extra_rectangle_size[3][1][2] = dp_context;
            dp_extra_rectangle_size[3][2][0] = mapping_row;
            dp_extra_rectangle_size[3][2][1] = mapping_column;
            dp_extra_rectangle_size[3][2][2] = context_shift;

            dp_extra_rectangle_size[4][0][0] = row_shift;
            dp_extra_rectangle_size[4][0][1] = dp_column;
            dp_extra_rectangle_size[4][0][2] = dp_context;
            dp_extra_rectangle_size[4][1][0] = mapping_row;
            dp_extra_rectangle_size[4][1][1] = col_shift;
            dp_extra_rectangle_size[4][1][2] = mapping_context;
            dp_extra_rectangle_size[4][2][0] = mapping_row;
            dp_extra_rectangle_size[4][2][1] = dp_column;
            dp_extra_rectangle_size[4][2][2] = context_shift;

            dp_extra_rectangle_size[5][2][0] = row_shift;
            dp_extra_rectangle_size[5][2][1] = dp_column;
            dp_extra_rectangle_size[5][2][2] = dp_context;
            dp_extra_rectangle_size[5][1][0] = mapping_row;
            dp_extra_rectangle_size[5][1][1] = col_shift;
            dp_extra_rectangle_size[5][1][2] = dp_context;
            dp_extra_rectangle_size[5][0][0] = mapping_row;
            dp_extra_rectangle_size[5][0][1] = mapping_column;
            dp_extra_rectangle_size[5][0][2] = context_shift;

            for (int pattern = 0; pattern < 6; pattern++) {
              uint16_t new_dp_value = 1;
              for (int rectangle_id = 0; rectangle_id < 3; rectangle_id++) {
                int rectangle_row =
                    dp_extra_rectangle_size[pattern][rectangle_id][0];
                int rectangle_col =
                    dp_extra_rectangle_size[pattern][rectangle_id][1];
                int rectangle_context =
                    dp_extra_rectangle_size[pattern][rectangle_id][2];
                new_dp_value +=
                    dp[rectangle_row][rectangle_col][rectangle_context];
              }

              if (new_dp_value <= dp[dp_row][dp_column][dp_context]) continue;

              // update dp
              dp[dp_row][dp_column][dp_context] = new_dp_value;
              dp_result[dp_row][dp_column][dp_context].clear();
              for (int rectangle_id = 0; rectangle_id < 3; rectangle_id++) {
                int rectangle_row =
                    dp_extra_rectangle_size[pattern][rectangle_id][0];
                int rectangle_col =
                    dp_extra_rectangle_size[pattern][rectangle_id][1];
                int rectangle_context =
                    dp_extra_rectangle_size[pattern][rectangle_id][2];
                for (const auto& result :
                     dp_result[rectangle_row][rectangle_col]
                              [rectangle_context]) {
                  auto new_result = result;
                  if (rectangle_id == 0) {
                    new_result.op.row += mapping_row;
                  } else if (rectangle_id == 1) {
                    new_result.op.column += mapping_column;
                  }
                  dp_result[dp_row][dp_column][dp_context].push_back(
                      new_result);
                }
              }
              remapper::MappingTransformOp transform_op(
                  0, 0, static_cast<remapper::RotateOp>(rotate_id));
              dp_result[dp_row][dp_column][dp_context].emplace_back(
                  mapping_rectangle.id, transform_op);
            }
          }
        }
      }
      const auto end_time = clock();
      log_file << "mapping search time: "
               << ((double)end_time - start_time) / CLOCKS_PER_SEC << std::endl;
    }

    if (dp[target_mrrg_config.row][target_mrrg_config.column]
          [target_mrrg_config.context_size] >= target_parallel_num) {
      std::vector<entity::Mapping> result_mapping_vec;
      std::vector<remapper::MappingTransformOp> result_transform_op_vec;

      for (const auto& id_and_op :
           dp_result[target_mrrg_config.row][target_mrrg_config.column]
                    [target_mrrg_config.context_size]) {
        result_mapping_vec.push_back(mapping_vec[id_and_op.id]);
        result_transform_op_vec.push_back(id_and_op.op);
        log_file << "----- mapping -----" << std::endl;
        log_file << "row: " << mapping_vec[id_and_op.id].GetMRRGConfig().row
                 << std::endl;
        log_file << "column: "
                 << mapping_vec[id_and_op.id].GetMRRGConfig().column
                 << std::endl;
        log_file << "context_size: "
                 << mapping_vec[id_and_op.id].GetMRRGConfig().context_size
                 << std::endl;
        log_file << "row shift: " << id_and_op.op.row << std::endl;
        log_file << "column shift: " << id_and_op.op.column << std::endl;
        log_file << "rotation: " << id_and_op.op.rotate_op << std::endl;
      }
      const auto result_mapping = remapper::MappingConcater(
          result_mapping_vec, result_transform_op_vec, target_mrrg_config);
      return {true, result_mapping};
    }
  };

  entity::Mapping empty;
  return {false, empty};
}