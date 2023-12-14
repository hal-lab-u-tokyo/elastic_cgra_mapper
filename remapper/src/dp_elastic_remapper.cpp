#include <remapper/dp_elastic_remapper.hpp>
#include <remapper/mapping_concater.hpp>
#include <remapper/mapping_transform_op.hpp>
#include <remapper/remapper.hpp>
#include <remapper/rotater.hpp>

std::pair<bool, entity::Mapping> remapper::DPElasticRemapping(
    const std::vector<entity::Mapping>& mapping_vec,
    const entity::MRRGConfig& target_mrrg_config, const int target_parallel_num,
    std::ofstream& log_file) {
  std::vector<remapper::MappingRectangle> mapping_rectangle_vec;
  for (size_t i = 0; i < mapping_vec.size(); i++) {
    const auto& mapping = mapping_vec[i];
    const auto mapping_matrix = remapper::CreateMatrixForElastic(mapping);
    mapping_rectangle_vec.emplace_back(i, mapping_matrix, mapping);
  }
  auto compare = [&](remapper::MappingRectangle left,
                     remapper::MappingRectangle right) {
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
          remapper::CreateMatrixForElastic(rotated_mapping);
      if (target_mrrg_config.row < rotated_mapping_matrix.rows() ||
          target_mrrg_config.column < rotated_mapping_matrix.cols() ||
          target_mrrg_config.context_size < rotated_mapping_matrix.maxCoeff())
        continue;

      const auto start_time = clock();
      if (!remapper::IsAvailableRemapping(rotated_mapping, 0, 0, target_mrrg_config)) {
        continue;
      }
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
                if (target_mrrg_config.memory_io ==
                        entity::MRRGMemoryIOType::kBothEnds &&
                    dp_column != target_mrrg_config.column &&
                    rectangle_id == 1) {
                  continue;
                }
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
                if (target_mrrg_config.memory_io ==
                        entity::MRRGMemoryIOType::kBothEnds &&
                    dp_column != target_mrrg_config.column &&
                    rectangle_id == 1) {
                  continue;
                }
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
                  if (target_mrrg_config.memory_io ==
                      entity::MRRGMemoryIOType::kAll) {
                    if (rectangle_id == 0) {
                      new_result.op.row += mapping_row;
                    } else if (rectangle_id == 1) {
                      new_result.op.column += mapping_column;
                    }
                  } else if (target_mrrg_config.memory_io ==
                             entity::MRRGMemoryIOType::kBothEnds) {
                    if (rectangle_id == 0) {
                      new_result.op.row += mapping_row;
                    } else if (dp_column == target_mrrg_config.column &&
                               rectangle_id == 1) {
                      const auto& tmp_rotated_mapping =
                          remapper::MappingRotater(mapping_vec[result.id],
                                                   result.op.rotate_op);
                      new_result.op.row = rectangle_row - 1 - result.op.row;
                      new_result.op.column =
                          rectangle_col - 1 - result.op.column + mapping_column;
                      new_result.op.row =
                          new_result.op.row -
                          tmp_rotated_mapping.GetMRRGConfig().row + 1;
                      new_result.op.column =
                          new_result.op.column -
                          tmp_rotated_mapping.GetMRRGConfig().column + 1;
                      new_result.op.rotate_op = Rotate180(result.op.rotate_op);
                    }
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