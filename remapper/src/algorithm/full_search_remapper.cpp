#include <chrono>
#include <remapper/algorithm/full_search_remapper.hpp>
#include <remapper/combination_counter.hpp>
#include <remapper/mapping_concater.hpp>
#include <remapper/mapping_transform_op.hpp>
#include <remapper/remapper.hpp>
#include <remapper/transform.hpp>
#include <tuple>

struct MappingTransformElement {
  MappingTransformElement(int _mapping_id,
                          const remapper::MappingMatrix& _mapping_matrix,
                          const remapper::CGRAMatrix& cgra_matrix)
      : mapping_id(_mapping_id), mapping_matrix(_mapping_matrix) {
    GetTransformInfo(cgra_matrix);
  }

  void GetTransformInfo(const remapper::CGRAMatrix& cgra_matrix) {
    entity::MRRGConfig tmp_mapping_config =
        mapping_matrix.GetMapping().GetMRRGConfig();
    size_t max_cgra_size =
        std::max(cgra_matrix.row_size, cgra_matrix.column_size);
    const int row_search_width = max_cgra_size - tmp_mapping_config.row + 1;
    const int column_search_width =
        max_cgra_size - tmp_mapping_config.column + 1;

    max_search_id =
        row_search_width * column_search_width * remapper::kRotateOpNum - 1;

    for (size_t search_id = 0; search_id < max_search_id; search_id++) {
      remapper::MappingTransformOp transform_op(
          search_id / (column_search_width * remapper::kRotateOpNum),
          (search_id / remapper::kRotateOpNum) % column_search_width,
          static_cast<remapper::RotateOp>(search_id % remapper::kRotateOpNum));
      bool is_available_mapping =
          cgra_matrix.IsAvailableRemapping(mapping_matrix, transform_op);

      if (is_available_mapping) {
        restricted_search_id_to_transform_op.push_back(transform_op);
      }
    }

    max_search_id = restricted_search_id_to_transform_op.size() - 1;
  }

  int mapping_id;
  int max_search_id;
  remapper::MappingMatrix mapping_matrix;
  std::vector<remapper::MappingTransformOp>
      restricted_search_id_to_transform_op;
};

class FullSearchHelper {
 private:
  std::vector<remapper::MappingMatrix> mapping_matrix_vec_;
  remapper::CGRAMatrix cgra_matrix_;
  std::vector<MappingTransformElement> mapping_transform_element_vec_;

 public:
  FullSearchHelper(
      const std::vector<remapper::MappingMatrix>& mapping_matrix_vec,
      const remapper::CGRAMatrix& cgra_matrix)
      : mapping_matrix_vec_(mapping_matrix_vec), cgra_matrix_(cgra_matrix) {
    for (size_t mapping_id = 0; mapping_id < mapping_matrix_vec.size();
         mapping_id++) {
      mapping_transform_element_vec_.emplace_back(
          mapping_id, mapping_matrix_vec[mapping_id], cgra_matrix);
    }
  }

  std::vector<remapper::MappingMatrix> BulkGetMappingMatrixVec(
      const std::vector<int>& mapping_id_vec) const {
    std::vector<remapper::MappingMatrix> mapping_matrix_vec_;
    for (const auto mapping_id : mapping_id_vec) {
      mapping_matrix_vec_.push_back(mapping_matrix_vec_[mapping_id]);
    }
    return mapping_matrix_vec_;
  };

  std::vector<int> BulkGetMaxSearchIdVec(
      const std::vector<int>& mapping_id_vec) const {
    std::vector<int> max_search_id_vec;
    for (const auto mapping_id : mapping_id_vec) {
      max_search_id_vec.push_back(
          mapping_transform_element_vec_[mapping_id].max_search_id);
    }
    return max_search_id_vec;
  };

  std::vector<remapper::MappingTransformOp> GetTransformOpVec(
      const std::vector<int>& mapping_id_vec,
      const std::vector<int>& search_id_vec) const {
    std::vector<remapper::MappingTransformOp> transform_op_vec;
    for (size_t i = 0; i < mapping_id_vec.size(); i++) {
      const auto& mapping_transform_element =
          mapping_transform_element_vec_[mapping_id_vec[i]];
      transform_op_vec.push_back(
          mapping_transform_element
              .restricted_search_id_to_transform_op[search_id_vec[i]]);
    }
    return transform_op_vec;
  };
};

remapper::RemappingResult remapper::FullSearchRemapping(
    std::vector<remapper::MappingMatrix> mapping_matrix_vec,
    const remapper::CGRAMatrix& cgra_matrix, const int target_parallel_num,
    std::ofstream& log_file, double timeout_s) {
  FullSearchHelper helper(mapping_matrix_vec, cgra_matrix);

  const auto start_time = std::chrono::system_clock::now();

  remapper::CombinationCounter selected_mapping_combination(
      mapping_matrix_vec.size() - 1, target_parallel_num);
  int mapping_group_id = 0;

  RemappingResult best_remapping_result;
  // select mapping
  while (1) {
    const std::vector<int> selected_mapping_id_vec =
        selected_mapping_combination.GetCombination();
    const std::vector<remapper::MappingMatrix> selected_mapping_matrix_vec =
        helper.BulkGetMappingMatrixVec(selected_mapping_id_vec);
    std::vector<int> max_selected_search_id_vec =
        helper.BulkGetMaxSearchIdVec(selected_mapping_id_vec);

    remapper::CombinationCounter selected_search_id_combination(
        max_selected_search_id_vec, selected_mapping_id_vec);

    while (1) {
      std::vector<int> selected_search_id_vec =
          selected_search_id_combination.GetCombination();
      std::vector<remapper::MappingTransformOp> transform_op_vec =
          helper.GetTransformOpVec(selected_mapping_id_vec,
                                   selected_search_id_vec);
      int mapping_num = cgra_matrix.TryRemapping(selected_mapping_matrix_vec,
                                                 transform_op_vec);

      if (mapping_num == target_parallel_num) {
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

      const auto tmp_time = std::chrono::system_clock::now();
      const auto elapsed_time =
          std::chrono::duration_cast<std::chrono::milliseconds>(tmp_time -
                                                                start_time)
              .count() /
          1000.0;
      if (elapsed_time > timeout_s) {
        return best_remapping_result;
      }

      // update search_id
      bool test_all_remapping = !(
          selected_search_id_combination.Next(std::max(mapping_num - 1, 0), 1));
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
