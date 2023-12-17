#include <remapper/algorithm/dp_elastic_remapper.hpp>
#include <remapper/mapping_concater.hpp>
#include <remapper/mapping_transform_op.hpp>
#include <remapper/remapper.hpp>
#include <remapper/rotater.hpp>

std::vector<std::vector<Eigen::Vector3d>> GetDPSplitedRectangleSize(
    const Eigen::Vector3d& dp_size, const Eigen::Vector3d& mapping_size) {
  std::vector<std::vector<Eigen::Vector3d>> dp_splited_rectangle_size;
  const auto shift_size = dp_size - mapping_size;

  std::vector<Eigen::Vector3d> pattern1;
  pattern1.emplace_back(shift_size.x(), dp_size.y(), mapping_size.z());
  pattern1.emplace_back(mapping_size.x(), shift_size.y(), mapping_size.z());
  pattern1.emplace_back(dp_size.x(), dp_size.y(), shift_size.z());
  dp_splited_rectangle_size.emplace_back(pattern1);

  std::vector<Eigen::Vector3d> pattern2;
  pattern2.emplace_back(shift_size.x(), mapping_size.y(), mapping_size.z());
  pattern2.emplace_back(dp_size.x(), shift_size.y(), mapping_size.z());
  pattern2.emplace_back(dp_size.x(), dp_size.y(), shift_size.z());
  dp_splited_rectangle_size.emplace_back(pattern2);

  std::vector<Eigen::Vector3d> pattern3;
  pattern3.emplace_back(shift_size.x(), mapping_size.y(), mapping_size.z());
  pattern3.emplace_back(dp_size.x(), shift_size.y(), dp_size.z());
  pattern3.emplace_back(dp_size.x(), mapping_size.y(), shift_size.z());
  dp_splited_rectangle_size.emplace_back(pattern3);

  std::vector<Eigen::Vector3d> pattern4;
  pattern4.emplace_back(shift_size.x(), mapping_size.y(), dp_size.z());
  pattern4.emplace_back(dp_size.x(), shift_size.y(), dp_size.z());
  pattern4.emplace_back(mapping_size.x(), mapping_size.y(), shift_size.z());
  dp_splited_rectangle_size.emplace_back(pattern4);

  std::vector<Eigen::Vector3d> pattern5;
  pattern5.emplace_back(shift_size.x(), dp_size.y(), dp_size.z());
  pattern5.emplace_back(mapping_size.x(), shift_size.y(), mapping_size.z());
  pattern5.emplace_back(mapping_size.x(), dp_size.y(), shift_size.z());
  dp_splited_rectangle_size.emplace_back(pattern5);

  std::vector<Eigen::Vector3d> pattern6;
  pattern6.emplace_back(shift_size.x(), dp_size.y(), dp_size.z());
  pattern6.emplace_back(mapping_size.x(), shift_size.y(), dp_size.z());
  pattern6.emplace_back(mapping_size.x(), mapping_size.y(), shift_size.z());
  dp_splited_rectangle_size.emplace_back(pattern6);

  return dp_splited_rectangle_size;
}

remapper::RemappingResult remapper::DPElasticRemapping(
    std::vector<remapper::MappingMatrix> mapping_matrix_vec,
    const remapper::CGRAMatrix& cgra_matrix, const int target_parallel_num,
    std::ofstream& log_file) {
  auto compare = [&](const remapper::MappingMatrix& left,
                     const remapper::MappingMatrix& right) {
    return left.op_rate > right.op_rate;
  };
  std::sort(mapping_matrix_vec.begin(), mapping_matrix_vec.end(), compare);
  std::unordered_map<int, int> mapping_id_to_index;
  for (int i = 0; i < mapping_matrix_vec.size(); i++) {
    mapping_id_to_index[mapping_matrix_vec[i].id] = i;
  }

  struct MappingIdAndOp {
    MappingIdAndOp(int _id, remapper::MappingTransformOp _op)
        : id(_id), op(_op){};
    int id;
    remapper::MappingTransformOp op;
  };

  std::vector<std::vector<std::vector<u_int16_t>>> dp;
  std::vector<std::vector<std::vector<std::vector<MappingIdAndOp>>>> dp_result;

  // init var for dp
  dp.resize(cgra_matrix.row_size + 1);
  dp_result.resize(cgra_matrix.row_size + 1);
  for (int row_id = 0; row_id < cgra_matrix.row_size + 1; row_id++) {
    dp[row_id].resize(cgra_matrix.column_size + 1);
    dp_result[row_id].resize(cgra_matrix.column_size + 1);
    for (int col_id = 0; col_id < cgra_matrix.column_size + 1; col_id++) {
      dp[row_id][col_id].resize(cgra_matrix.context_size + 1, 0);
      dp_result[row_id][col_id].resize(cgra_matrix.context_size + 1);
    }
  }

  for (const auto& mapping_matrix : mapping_matrix_vec) {
    for (int rotate_id = 0; rotate_id < 1; rotate_id++) {
      const auto start_time = clock();
      const auto rotated_transform_op = remapper::MappingTransformOp(
          0, 0, static_cast<remapper::RotateOp>(rotate_id));
      if (!cgra_matrix.IsAvailableRemapping(mapping_matrix,
                                            rotated_transform_op)) {
        continue;
      }
      const auto rotated_mapping_matrix =
          mapping_matrix.GetRotatedOpNumMatrix(rotated_transform_op.rotate_op);
      for (int row_shift = 0;
           row_shift < cgra_matrix.row_size - rotated_mapping_matrix.rows() + 1;
           row_shift++) {
        for (int col_shift = 0;
             col_shift <
             cgra_matrix.column_size - rotated_mapping_matrix.cols() + 1;
             col_shift++) {
          for (int context_shift = 0;
               context_shift <
               cgra_matrix.context_size - rotated_mapping_matrix.maxCoeff() + 1;
               context_shift++) {
            const auto mapping_size = Eigen::Vector3d(
                rotated_mapping_matrix.rows(), rotated_mapping_matrix.cols(),
                rotated_mapping_matrix.maxCoeff());
            const auto shift_size =
                Eigen::Vector3d(row_shift, col_shift, context_shift);
            const auto dp_size = mapping_size + shift_size;

            // 6 pattern test
            std::vector<std::vector<Eigen::Vector3d>>
                dp_splited_rectangle_size =
                    GetDPSplitedRectangleSize(dp_size, mapping_size);

            for (int pattern = 0; pattern < 6; pattern++) {
              uint16_t new_dp_value = 1;
              for (int rectangle_id = 0; rectangle_id < 3; rectangle_id++) {
                if (cgra_matrix.GetMRRGConfig().memory_io ==
                        entity::MRRGMemoryIOType::kBothEnds &&
                    dp_size.y() != cgra_matrix.column_size &&
                    rectangle_id == 1) {
                  continue;
                }
                const auto rectangle_size =
                    dp_splited_rectangle_size[pattern][rectangle_id];
                new_dp_value += dp[rectangle_size.x()][rectangle_size.y()]
                                  [rectangle_size.z()];
              }

              if (new_dp_value <= dp[dp_size.x()][dp_size.y()][dp_size.z()]) {
                continue;
              }

              // update dp
              dp[dp_size.x()][dp_size.y()][dp_size.z()] = new_dp_value;
              dp_result[dp_size.x()][dp_size.y()][dp_size.z()].clear();
              for (int rectangle_id = 0; rectangle_id < 3; rectangle_id++) {
                if (cgra_matrix.GetMRRGConfig().memory_io ==
                        entity::MRRGMemoryIOType::kBothEnds &&
                    dp_size.y() != cgra_matrix.column_size &&
                    rectangle_id == 1) {
                  continue;
                }
                int rectangle_row =
                    dp_splited_rectangle_size[pattern][rectangle_id][0];
                int rectangle_col =
                    dp_splited_rectangle_size[pattern][rectangle_id][1];
                int rectangle_context =
                    dp_splited_rectangle_size[pattern][rectangle_id][2];
                for (const auto& result :
                     dp_result[rectangle_row][rectangle_col]
                              [rectangle_context]) {
                  auto new_result = result;
                  if (cgra_matrix.GetMRRGConfig().memory_io ==
                      entity::MRRGMemoryIOType::kAll) {
                    if (rectangle_id == 0) {
                      new_result.op.row += mapping_size.x();
                    } else if (rectangle_id == 1) {
                      new_result.op.column += mapping_size.y();
                    }
                  } else if (cgra_matrix.GetMRRGConfig().memory_io ==
                             entity::MRRGMemoryIOType::kBothEnds) {
                    if (rectangle_id == 0) {
                      new_result.op.row += mapping_size.x();
                    } else if (dp_size.y() == cgra_matrix.column_size &&
                               rectangle_id == 1) {
                      int index = mapping_id_to_index[result.id];
                      const auto& tmp_rotated_mapping_matrix =
                          mapping_matrix_vec[index].GetRotatedMemoryOpNumMatrix(
                              result.op.rotate_op);
                      new_result.op.row = rectangle_row - 1 - result.op.row;
                      new_result.op.column = rectangle_col - 1 -
                                             result.op.column +
                                             mapping_size.y();
                      new_result.op.row = new_result.op.row -
                                          tmp_rotated_mapping_matrix.rows() + 1;
                      new_result.op.column = new_result.op.column -
                                             tmp_rotated_mapping_matrix.cols() +
                                             1;
                      new_result.op.rotate_op = Rotate180(result.op.rotate_op);
                    }
                  }

                  dp_result[dp_size.x()][dp_size.y()][dp_size.z()].push_back(
                      new_result);
                }
              }
              remapper::MappingTransformOp transform_op(
                  0, 0, static_cast<remapper::RotateOp>(rotate_id));
              dp_result[dp_size.x()][dp_size.y()][dp_size.z()].emplace_back(
                  mapping_matrix.id, transform_op);
            }
          }
        }
      }
      const auto end_time = clock();
      log_file << "mapping search time: "
               << ((double)end_time - start_time) / CLOCKS_PER_SEC << std::endl;
    }

    if (dp[cgra_matrix.row_size][cgra_matrix.column_size]
          [cgra_matrix.context_size] >= target_parallel_num) {
      std::vector<int> result_mapping_id_vec;
      std::vector<remapper::MappingTransformOp> result_transform_op_vec;

      for (const auto& id_and_op :
           dp_result[cgra_matrix.row_size][cgra_matrix.column_size]
                    [cgra_matrix.context_size]) {
        const auto index = mapping_id_to_index[id_and_op.id];
        const auto& mapping = mapping_matrix_vec[index].GetMapping();
        result_mapping_id_vec.push_back(id_and_op.id);
        result_transform_op_vec.push_back(id_and_op.op);
        remapper::OutputToLogFile(mapping.GetMRRGConfig(), id_and_op.op,
                                  log_file);
      }
      return remapper::RemappingResult(result_mapping_id_vec,
                                       result_transform_op_vec);
    }
  };

  std::vector<int> result_mapping_id_vec;
  std::vector<remapper::MappingTransformOp> result_transform_op_vec;

  for (const auto& id_and_op :
       dp_result[cgra_matrix.row_size][cgra_matrix.column_size]
                [cgra_matrix.context_size]) {
    const auto index = mapping_id_to_index[id_and_op.id];
    const auto& mapping = mapping_matrix_vec[index].GetMapping();
    result_mapping_id_vec.push_back(id_and_op.id);
    result_transform_op_vec.push_back(id_and_op.op);
    remapper::OutputToLogFile(mapping.GetMRRGConfig(), id_and_op.op, log_file);
  }
  return remapper::RemappingResult(result_mapping_id_vec,
                                   result_transform_op_vec);
}