#include "remapper/rotater.hpp"

entity::ConfigId RotateConfigId(const entity::ConfigId& config_id,
                                const entity::MRRGConfig& target_mrrg_config,
                                const remapper::RotateOp& rotate_op) {
  int rotated_row_id, rotated_column_id;
  switch (rotate_op) {
    case remapper::RotateOp::TopIsRight:
      rotated_row_id = config_id.column_id;
      rotated_column_id = target_mrrg_config.row - 1 - config_id.row_id;
      break;

    case remapper::RotateOp::TopIsBottom:
      rotated_row_id = target_mrrg_config.row - 1 - config_id.row_id;
      rotated_column_id = target_mrrg_config.column - 1 - config_id.column_id;
      break;

    case remapper::RotateOp::TopIsLeft:
      rotated_row_id = target_mrrg_config.row - 1 - config_id.column_id;
      rotated_column_id = config_id.row_id;
      break;

    case remapper::RotateOp::TopIsTop:
      rotated_row_id = config_id.row_id;
      rotated_column_id = config_id.column_id;
      break;
  }

  return entity::ConfigId(rotated_row_id, rotated_column_id,
                          config_id.context_id);
}

entity::MRRGConfig RotateMRRGConfig(const entity::MRRGConfig& mrrg_config,
                                    const remapper::RotateOp& rotate_op) {
  entity::MRRGConfig rotated_mrrg_config = mrrg_config;
  rotated_mrrg_config.column = mrrg_config.column;
  rotated_mrrg_config.row = mrrg_config.row;
}

entity::Mapping remapper::MappingRotater(const entity::Mapping& mapping,
                                         const remapper::RotateOp& rotate_op) {
  entity::ConfigMap rotated_config_map;
  const auto& rotated_mrrg_config =
      RotateMRRGConfig(mapping.GetMRRGConfig(), rotate_op);

  for (const auto& cgra_config : mapping.GetConfigMap()) {
    entity::ConfigId rotated_config_id =
        RotateConfigId(cgra_config.first, rotated_mrrg_config, rotate_op);
    entity::CGRAConfig rotated_cgra_config = cgra_config.second;
    rotated_cgra_config.to_config_id_vec.clear();
    for (const auto& to_config_id : cgra_config.second.to_config_id_vec) {
      rotated_cgra_config.to_config_id_vec.push_back(
          RotateConfigId(to_config_id, rotated_mrrg_config, rotate_op));
    }

    for (int i = 0; i < cgra_config.second.from_config_id_num; i++) {
      rotated_cgra_config.from_config_id_vec[i] =
          RotateConfigId(cgra_config.second.from_config_id_vec[i],
                         rotated_mrrg_config, rotate_op);
    }

    rotated_config_map.emplace(rotated_config_id, rotated_cgra_config);
  }

  return entity::Mapping(rotated_mrrg_config, rotated_config_map);
}