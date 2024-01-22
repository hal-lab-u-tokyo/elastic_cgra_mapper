#include <remapper/algorithm/dp_and_full_search_elastic_remapper.hpp>
#include <remapper/algorithm/dp_elastic_remapper.hpp>
#include <remapper/algorithm/full_search_elastic_remapper.hpp>
#include <remapper/mapping_concater.hpp>
#include <remapper/transform.hpp>

struct NewMappingDBElement {
  NewMappingDBElement(){};
  NewMappingDBElement(
      int _mapping_id, std::vector<int> _element_mapping_id,
      std::vector<remapper::MappingTransformOp> _element_mapping_transform_op,
      const entity::MRRGConfig& mrrg_config) {
    assert(_element_mapping_id.size() == 2);
    assert(_element_mapping_transform_op.size() == 2);

    mapping_id = _mapping_id;
    element_mapping_id = _element_mapping_id;
    element_mapping_transform_op = _element_mapping_transform_op;
  }

  int mapping_id;
  std::vector<int> element_mapping_id;
  std::vector<remapper::MappingTransformOp> element_mapping_transform_op;
  entity::MRRGConfig mrrg_config;
};

remapper::RemappingResult remapper::DPAndFullSearchElasticRemapping(
    std::vector<remapper::MappingMatrix> mapping_matrix_vec,
    const remapper::CGRAMatrix& cgra_matrix, const int target_parallel_num,
    std::ofstream& log_file) {
  double min_op_rate = 1;
  for (const auto mapping : mapping_matrix_vec) {
    min_op_rate = std::min(mapping.op_rate, min_op_rate);
  }

  std::unordered_map<int, NewMappingDBElement> id_to_new_db_element_map;

  for (size_t i = 0; i < mapping_matrix_vec.size(); i++) {
    for (size_t j = i; j < mapping_matrix_vec.size(); j++) {
      const auto mapping_i = mapping_matrix_vec[i];
      const auto mapping_j = mapping_matrix_vec[j];

      const auto mapping_i_min_size =
          std::min(mapping_i.row_size, mapping_i.column_size);
      const auto mapping_i_max_size =
          std::max(mapping_i.row_size, mapping_i.column_size);
      const auto mapping_j_min_size =
          std::min(mapping_j.row_size, mapping_j.column_size);
      const auto mapping_j_max_size =
          std::max(mapping_j.row_size, mapping_j.column_size);

      const auto min_size_to_search =
          std::max(mapping_i_min_size, mapping_j_min_size);
      const auto max_size_to_search = mapping_i_max_size + mapping_j_max_size;
      const auto min_context_size_to_search =
          std::max(mapping_i.context_size, mapping_j.context_size);
      const auto max_context_size_to_search =
          std::min(mapping_i.context_size + mapping_j.context_size,
                   cgra_matrix.context_size);

      for (int row = min_size_to_search; row < max_size_to_search; row++) {
        for (int col = min_size_to_search; col < max_size_to_search; col++) {
          for (int context = min_context_size_to_search;
               context < max_context_size_to_search; context++) {
            if (row > cgra_matrix.row_size || col > cgra_matrix.column_size) {
              continue;
            }
            
            // mapping i available
            if ((row < mapping_i.row_size || col < mapping_i.column_size) && (row < mapping_i.column_size || col < mapping_i.row_size)) {
              continue;
            }

            // mapping j avalilable
            if ((row < mapping_j.row_size || col < mapping_j.column_size) && (row < mapping_j.column_size || col < mapping_j.row_size)) {
              continue;
            }

            if (cgra_matrix.GetMRRGConfig().memory_io ==
                    entity::MRRGMemoryIOType::kAll &&
                row < col) {
              continue;
            }

            double estimated_op_rate =
                mapping_i.op_rate * 2 * mapping_i.row_size *
                mapping_i.column_size * mapping_i.context_size /
                (row * col * context);
            if (estimated_op_rate < min_op_rate) {
              continue;
            }

            std::vector<remapper::MappingMatrix>
                mapping_vec_to_create_new_element = {mapping_i, mapping_j};
            entity::MRRGConfig mrrg_config_to_create_new_element =
                cgra_matrix.GetMRRGConfig();
            if (mrrg_config_to_create_new_element.memory_io ==
                    entity::MRRGMemoryIOType::kBothEnds &&
                col < mrrg_config_to_create_new_element.column) {
              mrrg_config_to_create_new_element.memory_io ==
                  entity::MRRGMemoryIOType::kOneEnd;
            }

            mrrg_config_to_create_new_element.row = row;
            mrrg_config_to_create_new_element.column = col;
            mrrg_config_to_create_new_element.context_size = context;

            remapper::CGRAMatrix cgra_matrix_to_create_new_element(
                mrrg_config_to_create_new_element);
            const auto remapping_result = remapper::FullSearchElasticRemapping(
                mapping_vec_to_create_new_element,
                cgra_matrix_to_create_new_element, 2, log_file);
            if (remapping_result.result_mapping_id_vec.size() != 2) {
              continue;
            }
            if (i != j && remapping_result.result_mapping_id_vec[0] ==
                              remapping_result.result_mapping_id_vec[1]) {
              continue;
            }
            std::vector<entity::Mapping> result_mapping_vec;
            for (const auto& mapping_id :
                 remapping_result.result_mapping_id_vec) {
              result_mapping_vec.push_back(
                  mapping_vec_to_create_new_element[mapping_id].GetMapping());
            }
            const entity::Mapping result_mapping = remapper::MappingConcater(
                result_mapping_vec, remapping_result.result_transform_op_vec,
                mrrg_config_to_create_new_element);
            int result_mapping_id = mapping_matrix_vec.size();
            const remapper::MappingMatrix result_mapping_matrix =
                remapper::MappingMatrix(result_mapping, result_mapping_id,
                                        mrrg_config_to_create_new_element, 2);
            mapping_matrix_vec.push_back(result_mapping_matrix);

            NewMappingDBElement new_mapping_db_element(
                result_mapping_id, remapping_result.result_mapping_id_vec,
                remapping_result.result_transform_op_vec,
                mrrg_config_to_create_new_element);

            id_to_new_db_element_map.emplace(result_mapping_id,
                                             new_mapping_db_element);
          }
        }
      }
    }
  }

  const auto dp_remapping_result = remapper::DPElasticRemapping(
      mapping_matrix_vec, cgra_matrix, target_parallel_num, log_file);

  remapper::RemappingResult modified_remepping_result;
  for (int i = 0; i < dp_remapping_result.result_mapping_id_vec.size(); i++) {
    const auto mapping_id = dp_remapping_result.result_mapping_id_vec[i];
    const auto transform_op = dp_remapping_result.result_transform_op_vec[i];

    if (id_to_new_db_element_map.count(mapping_id) == 0) {
      modified_remepping_result.result_mapping_id_vec.push_back(mapping_id);
      modified_remepping_result.result_transform_op_vec.push_back(transform_op);
    } else {
      const auto db_element = id_to_new_db_element_map[mapping_id];
      for (int j = 0; j < 2; j++) {
        // add mapping
        const int element_mapping_id = db_element.element_mapping_id[j];
        const auto element_mapping_transform_op =
            db_element.element_mapping_transform_op[j];
        modified_remepping_result.result_mapping_id_vec.push_back(
            element_mapping_id);

        // add transform op
        const auto mapping_matrix = mapping_matrix_vec[element_mapping_id];
        remapper::RotateOp new_rotate_op = remapper::CombineRotateOp(
            element_mapping_transform_op.rotate_op, transform_op.rotate_op);
        entity::ConfigId config1(0, 0, 0);
        entity::ConfigId config2(mapping_matrix.row_size - 1,
                                 mapping_matrix.column_size - 1, 0);

        const entity::ConfigId config_transform(
            element_mapping_transform_op.row,
            element_mapping_transform_op.column, 0);
        auto transformed_config1 =
            remapper::RotateConfigId(config1, db_element.mrrg_config,
                                     element_mapping_transform_op.rotate_op) +
            config_transform;
        transformed_config1 = remapper::RotateConfigId(
            transformed_config1, cgra_matrix.GetMRRGConfig(),
            transform_op.rotate_op);
        auto transformed_config2 =
            remapper::RotateConfigId(config2, db_element.mrrg_config,
                                     element_mapping_transform_op.rotate_op) +
            config_transform;
        transformed_config2 = remapper::RotateConfigId(
            transformed_config2, cgra_matrix.GetMRRGConfig(),
            transform_op.rotate_op);

        remapper::MappingTransformOp new_transform_op;
        new_transform_op.rotate_op = new_rotate_op;
        new_transform_op.row =
            std::min(transformed_config1.row_id, transformed_config2.row_id) +
            transform_op.row;
        new_transform_op.column = std::min(transformed_config1.column_id,
                                           transformed_config2.column_id) +
                                  transform_op.column;
        modified_remepping_result.result_transform_op_vec.push_back(
            new_transform_op);
      }
    }
  }

  return modified_remepping_result;
}