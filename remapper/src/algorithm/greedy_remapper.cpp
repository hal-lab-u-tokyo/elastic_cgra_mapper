#include <remapper/algorithm/greedy_remapper.hpp>
#include <remapper/mapping_concater.hpp>
#include <remapper/mapping_transform_op.hpp>
#include <remapper/remapper.hpp>
#include <remapper/transform.hpp>

class GreedyRemapperHelper {
 private:
  std::vector<remapper::MappingMatrix> mapping_matrix_vec_;
  remapper::CGRAMatrix cgra_matrix_;
  std::ofstream& log_file_;
  std::unordered_map<int, int> mapping_id_to_index_;

  void SortMappingMatrices() {
    auto compare_parallel_num = [&](const remapper::MappingMatrix& left,
                                    const remapper::MappingMatrix& right) {
      if (left.estimated_reallocate_num == right.estimated_reallocate_num) {
        return left.op_rate > right.op_rate;
      }
      return left.estimated_reallocate_num > right.estimated_reallocate_num;
    };
    auto compare_num_waste_of_memory_io =
        [&](const remapper::MappingMatrix& left,
            const remapper::MappingMatrix& right) {
          return left.num_waste_of_memory_io < right.num_waste_of_memory_io;
        };

    if (cgra_matrix_.GetMRRGConfig().memory_io ==
        entity::MRRGMemoryIOType::kAll) {
      std::sort(mapping_matrix_vec_.begin(), mapping_matrix_vec_.end(),
                compare_parallel_num);
    } else {
      std::sort(mapping_matrix_vec_.begin(), mapping_matrix_vec_.end(),
                compare_num_waste_of_memory_io);
    }

    return;
  };

  void CreateMappingIdToIndexMap() {
    for (size_t index = 0; index < mapping_matrix_vec_.size(); index++) {
      mapping_id_to_index_[mapping_matrix_vec_[index].id] = index;
    }
  };

 public:
  GreedyRemapperHelper(
      const std::vector<remapper::MappingMatrix>& mapping_matrix_vec,
      const remapper::CGRAMatrix& cgra_matrix, std::ofstream& log_file)
      : mapping_matrix_vec_(mapping_matrix_vec),
        cgra_matrix_(cgra_matrix),
        log_file_(log_file) {
    SortMappingMatrices();
    CreateMappingIdToIndexMap();
  }

  void OutputToLog(const std::vector<int>& result_mapping_id_vec,
                   const std::vector<remapper::MappingTransformOp>&
                       result_transform_op_vec) {
    for (size_t result_id = 0; result_id < result_mapping_id_vec.size();
         result_id++) {
      int mapping_id = result_mapping_id_vec[result_id];
      int index = mapping_id_to_index_[mapping_id];
      const auto& mapping_mrrg_config =
          mapping_matrix_vec_[index].GetMapping().GetMRRGConfig();
      remapper::OutputToLogFile(mapping_mrrg_config,
                                result_transform_op_vec[result_id], log_file_);
    }
  }

  std::vector<remapper::MappingMatrix> BulkGetMappingMatrixVec(
      const std::vector<int>& mapping_id_vec) const {
    std::vector<remapper::MappingMatrix> result;
    for (const auto mapping_id : mapping_id_vec) {
      int index = mapping_id_to_index_.at(mapping_id);
      result.emplace_back(mapping_matrix_vec_[index]);
    }
    return result;
  };

  std::vector<remapper::MappingMatrix> GetSortedMappingMatrixVec() const {
    return mapping_matrix_vec_;
  };
};

remapper::RemappingResult remapper::GreedyRemapping(
    std::vector<remapper::MappingMatrix> mapping_matrix_vec,
    const remapper::CGRAMatrix& cgra_matrix, const int target_parallel_num,
    std::ofstream& log_file) {
  GreedyRemapperHelper helper(mapping_matrix_vec, cgra_matrix, log_file);

  std::vector<int> result_mapping_id_vec;
  std::vector<remapper::MappingTransformOp> result_transform_op_vec;

  const std::vector<remapper::MappingMatrix>& sorted_mapping_matrix_vec =
      helper.GetSortedMappingMatrixVec();

  int parallel_num = 0;
  const auto start_time = clock();
  for (const auto& mapping_matrix : sorted_mapping_matrix_vec) {
    for (const auto rotate_op : remapper::kAllRotateOpVec) {
      const auto rotated_mapping_matrix =
          mapping_matrix.GetRotatedOpNumMatrix(rotate_op);
      if (!cgra_matrix.IsAvailableRemapping(
              mapping_matrix, remapper::MappingTransformOp(0, 0, rotate_op))) {
        continue;
      }
      for (int row_shift = 0;
           row_shift < cgra_matrix.row_size - rotated_mapping_matrix.rows() + 1;
           row_shift++) {
        for (int col_shift = 0;
             col_shift <
             cgra_matrix.column_size - rotated_mapping_matrix.cols() + 1;
             col_shift++) {
          const auto transform_op =
              remapper::MappingTransformOp(row_shift, col_shift, rotate_op);
          if (!cgra_matrix.IsAvailableRemapping(mapping_matrix, transform_op))
            continue;

          while (1) {
            std::vector<int> tmp_result_mapping_id_vec = result_mapping_id_vec;
            std::vector<remapper::MappingTransformOp>
                tmp_result_transform_op_vec = result_transform_op_vec;
            tmp_result_mapping_id_vec.push_back(mapping_matrix.id);
            tmp_result_transform_op_vec.push_back(transform_op);
            int mapping_num = cgra_matrix.TryRemapping(
                helper.BulkGetMappingMatrixVec(tmp_result_mapping_id_vec),
                tmp_result_transform_op_vec);

            // failed
            if (mapping_num < tmp_result_mapping_id_vec.size()) {
              break;
            }

            // success
            result_mapping_id_vec.push_back(mapping_matrix.id);
            result_transform_op_vec.push_back(transform_op);
            parallel_num++;

            if (parallel_num == target_parallel_num) {
              helper.OutputToLog(result_mapping_id_vec,
                                 result_transform_op_vec);

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

  helper.OutputToLog(result_mapping_id_vec, result_transform_op_vec);

  return remapper::RemappingResult(result_mapping_id_vec,
                                   result_transform_op_vec);
}
