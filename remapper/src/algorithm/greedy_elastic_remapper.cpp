#include <remapper/algorithm/greedy_elastic_remapper.hpp>
#include <remapper/mapping_concater.hpp>
#include <remapper/mapping_transform_op.hpp>
#include <remapper/remapper.hpp>
#include <remapper/rotater.hpp>

remapper::RemappingResult remapper::GreedyElasticRemapping(
    const std::vector<remapper::MappingMatrix> mapping_matrix_vec,
    const remapper::CGRAMatrix& cgra_matrix, const int target_parallel_num,
    std::ofstream& log_file) {
  auto compare_op_rate = [&](remapper::MappingMatrix left,
                             remapper::MappingMatrix right) {
    return left.op_rate > right.op_rate;
  };
  auto compare_num_waste_of_memory_io = [&](remapper::MappingMatrix left,
                                            remapper::MappingMatrix right) {
    return left.num_waste_of_memory_io < right.num_waste_of_memory_io;
  };
  if (cgra_matrix.GetMRRGConfig().memory_io == entity::MRRGMemoryIOType::kAll) {
    std::sort(mapping_matrix_vec.begin(), mapping_matrix_vec.end(),
              compare_op_rate);
  } else {
    std::sort(mapping_matrix_vec.begin(), mapping_matrix_vec.end(),
              compare_num_waste_of_memory_io);
  }

  Eigen::MatrixXi target_matrix = Eigen::MatrixXi::Zero(
      cgra_matrix.GetMRRGConfig().row, cgra_matrix.GetMRRGConfig().column);

  std::vector<int> result_mapping_id_vec;
  std::vector<remapper::MappingTransformOp> result_transform_op_vec;

  int parallel_num = 0;
  for (const auto& mapping_matrix : mapping_matrix_vec) {
    for (int rotate_id = 0; rotate_id < 4; rotate_id++) {
      const auto rotated_mapping_matrix = mapping_matrix.GetRotatedOpNumMatrix(
          static_cast<remapper::RotateOp>(rotate_id));
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
          const auto transform_op = remapper::MappingTransformOp(
              row_shift, col_shift, static_cast<remapper::RotateOp>(rotate_id));
          if (!cgra_matrix.IsAvailableRemapping(mapping_matrix, transform_op))
            continue;

          while (1) {
            Eigen::MatrixXi added_matrix = target_matrix;
            added_matrix.block(
                row_shift, col_shift, rotated_mapping_matrix.rows(),
                rotated_mapping_matrix.cols()) += rotated_mapping_matrix;
            // failed
            if (added_matrix.maxCoeff() >
                cgra_matrix.GetMRRGConfig().context_size)
              break;

            // success
            target_matrix = added_matrix;
            result_mapping_id_vec.push_back(mapping_matrix.id);
            result_transform_op_vec.emplace_back(
                row_shift, col_shift,
                static_cast<remapper::RotateOp>(rotate_id));
            parallel_num++;

            if (parallel_num == target_parallel_num) {
              for (size_t result_id = 0;
                   result_id < result_mapping_id_vec.size(); result_id++) {
                int mapping_id = result_mapping_id_vec[result_id];
                const auto& mapping_mrrg_config =
                    mapping_matrix_vec[mapping_id].GetMapping().GetMRRGConfig();
                log_file << "----- mapping -----" << std::endl;
                log_file << "row: " << mapping_mrrg_config.row << std::endl;
                log_file << "column: " << mapping_mrrg_config.column
                         << std::endl;
                log_file << "context_size: " << mapping_mrrg_config.context_size
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

              return remapper::RemappingResult(result_mapping_id_vec,
                                               result_transform_op_vec);
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
  return remapper::RemappingResult();
}