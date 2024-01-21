#include "remapper/mapping_concater.hpp"

#include <Eigen/Eigen>
#include <unordered_map>

#include "remapper/transform.hpp"

entity::Mapping remapper::MappingConcater(
    const std::vector<entity::Mapping>& mapping_vec,
    const std::vector<MappingTransformOp>& mapping_transform_op_vec,
    const entity::MRRGConfig& target_mrrg_config) {
  assert(mapping_vec.size() == mapping_transform_op_vec.size());
  Eigen::MatrixXi mapping_matrix =
      Eigen::MatrixXi::Zero(target_mrrg_config.row, target_mrrg_config.column);

  std::unordered_map<entity::PEPositionId, int, entity::HashPEPositionId>
      pe_position_id_to_next_context_num;
  for (int row_id = 0; row_id < target_mrrg_config.row; row_id++) {
    for (int column_id = 0; column_id < target_mrrg_config.column;
         column_id++) {
      pe_position_id_to_next_context_num.emplace(
          entity::PEPositionId(row_id, column_id), 0);
    }
  }

  entity::ConfigMap result_config_map;
  for (int i = 0; i < mapping_vec.size(); i++) {
    const auto& mapping = mapping_vec[i];
    const auto& transform_op = mapping_transform_op_vec[i];

    Eigen::MatrixXi tmp_mapping_matrix = Eigen::MatrixXi::Zero(
        target_mrrg_config.row, target_mrrg_config.column);
    const auto rotated_mapping =
        remapper::MappingRotater(mapping, transform_op.rotate_op);

    const auto ShiftConfigId = [&](entity::ConfigId config_id) {
      const auto shifted_row_id = config_id.row_id + transform_op.row;
      const auto shifted_column_id = config_id.column_id + transform_op.column;
      const auto shifted_context_id =
          config_id.context_id +
          mapping_matrix(shifted_row_id, shifted_column_id);
      return entity::ConfigId(shifted_row_id, shifted_column_id,
                              shifted_context_id);
    };

    for (const auto& id_config_pair : rotated_mapping.GetConfigMap()) {
      if (id_config_pair.second.operation_type == entity::OpType::NOP) continue;
      entity::ConfigId shifted_id = ShiftConfigId(id_config_pair.first);
      assert(shifted_id.context_id < target_mrrg_config.context_size);
      tmp_mapping_matrix(shifted_id.row_id, shifted_id.column_id)++;

      entity::CGRAConfig shifted_config = id_config_pair.second;
      for (int j = 0; j < shifted_config.to_config_id_vec.size(); j++) {
        shifted_config.to_config_id_vec[j] =
            ShiftConfigId(shifted_config.to_config_id_vec[j]);
      }
      for (int j = 0; j < shifted_config.from_config_id_num; j++) {
        shifted_config.from_config_id_vec[j] =
            ShiftConfigId(shifted_config.from_config_id_vec[j]);
      }

      result_config_map.emplace(shifted_id, shifted_config);
    }

    mapping_matrix += tmp_mapping_matrix;
  }

  if (target_mrrg_config.memory_io == entity::MRRGMemoryIOType::kBothEnds) {
    for (const auto& config : result_config_map) {
      if (config.second.operation_type == entity::OpType::STORE ||
          config.second.operation_type == entity::OpType::LOAD ||
          config.second.operation_type == entity::OpType::OUTPUT) {
        assert(config.first.column_id == 0 ||
               config.first.column_id == target_mrrg_config.column - 1);
      }
    }
  }

  return entity::Mapping(target_mrrg_config, result_config_map);
}
