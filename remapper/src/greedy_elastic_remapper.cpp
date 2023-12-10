#include <remapper/greedy_elastic_remapper.hpp>
#include <remapper/mapping_concater.hpp>
#include <remapper/mapping_transform_op.hpp>
#include <remapper/remapper.hpp>
#include <remapper/rotater.hpp>

std::pair<bool, entity::Mapping> remapper::GreedyElasticRemapping(
    const std::vector<entity::Mapping>& mapping_vec,
    const entity::MRRGConfig& target_mrrg_config, const int target_parallel_num,
    std::ofstream& log_file) {
  std::vector<remapper::MappingRectangle> mapping_rectangle_vec;
  for (size_t i = 0; i < mapping_vec.size(); i++) {
    const auto& mapping = mapping_vec[i];
    const auto mapping_matrix = remapper::CreateMatrixForElastic(mapping);
    mapping_rectangle_vec.emplace_back(i, mapping_matrix, mapping);
  }
  auto compare_op_rate = [&](remapper::MappingRectangle left,
                             remapper::MappingRectangle right) {
    return left.op_rate > right.op_rate;
  };
  auto compare_num_waste_of_memory_io = [&](remapper::MappingRectangle left,
                                            remapper::MappingRectangle right) {
    return left.num_waste_of_memory_io < right.num_waste_of_memory_io;
  };
  if (target_mrrg_config.memory_io == entity::MRRGMemoryIOType::kAll) {
    std::sort(mapping_rectangle_vec.begin(), mapping_rectangle_vec.end(),
              compare_op_rate);
  } else {
    std::sort(mapping_rectangle_vec.begin(), mapping_rectangle_vec.end(),
              compare_num_waste_of_memory_io);
  }

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
          remapper::CreateMatrixForElastic(rotated_mapping);
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
          if (!remapper::IsAvailableRemapping(rotated_mapping, row_shift, col_shift,
                                    target_mrrg_config)) {
            continue;
          }
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