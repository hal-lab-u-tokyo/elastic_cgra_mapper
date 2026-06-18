#include "remapper/transform.hpp"

entity::ConfigId remapper::RotateConfigId(
    const entity::ConfigId& config_id,
    const entity::MRRGConfig& target_mrrg_config,
    const remapper::RotateOp& rotate_op) {
  int rotated_row_id, rotated_column_id;
  switch (rotate_op) {
    case remapper::RotateOp::kTopIsRight:
      rotated_row_id = config_id.column_id;
      rotated_column_id = target_mrrg_config.row - 1 - config_id.row_id;
      break;

    case remapper::RotateOp::kTopIsBottom:
      rotated_row_id = target_mrrg_config.row - 1 - config_id.row_id;
      rotated_column_id = target_mrrg_config.column - 1 - config_id.column_id;
      break;

    case remapper::RotateOp::kTopIsLeft:
      rotated_row_id = target_mrrg_config.column - 1 - config_id.column_id;
      rotated_column_id = config_id.row_id;
      break;

    case remapper::RotateOp::kTopIsTop:
      rotated_row_id = config_id.row_id;
      rotated_column_id = config_id.column_id;
      break;

    case remapper::RotateOp::kTopIsRightMirror:
      rotated_row_id = config_id.column_id;
      rotated_column_id = config_id.row_id;
      break;

    case remapper::RotateOp::kTopIsBottomMirror:
      rotated_row_id = target_mrrg_config.row - 1 - config_id.row_id;
      rotated_column_id = config_id.column_id;
      break;

    case remapper::RotateOp::kTopIsLeftMirror:
      rotated_row_id = target_mrrg_config.column - 1 - config_id.column_id;
      rotated_column_id = target_mrrg_config.row - 1 - config_id.row_id;
      break;

    case remapper::RotateOp::kTopIsTopMirror:
      rotated_row_id = config_id.row_id;
      rotated_column_id = target_mrrg_config.column - 1 - config_id.column_id;
      break;
  }

  return entity::ConfigId(rotated_row_id, rotated_column_id,
                          config_id.context_id);
}

entity::MRRGConfig RotateMRRGConfig(const entity::MRRGConfig& mrrg_config,
                                    const remapper::RotateOp& rotate_op) {
  entity::MRRGConfig rotated_mrrg_config = mrrg_config;

  switch (rotate_op) {
    case remapper::RotateOp::kTopIsRight:
      rotated_mrrg_config.column = mrrg_config.row;
      rotated_mrrg_config.row = mrrg_config.column;
      break;

    case remapper::RotateOp::kTopIsBottom:
      rotated_mrrg_config.column = mrrg_config.column;
      rotated_mrrg_config.row = mrrg_config.row;
      break;

    case remapper::RotateOp::kTopIsLeft:
      rotated_mrrg_config.column = mrrg_config.row;
      rotated_mrrg_config.row = mrrg_config.column;
      break;

    case remapper::RotateOp::kTopIsTop:
      rotated_mrrg_config.column = mrrg_config.column;
      rotated_mrrg_config.row = mrrg_config.row;
      break;

    case remapper::RotateOp::kTopIsRightMirror:
      rotated_mrrg_config.column = mrrg_config.row;
      rotated_mrrg_config.row = mrrg_config.column;
      break;

    case remapper::RotateOp::kTopIsBottomMirror:
      rotated_mrrg_config.column = mrrg_config.column;
      rotated_mrrg_config.row = mrrg_config.row;
      break;

    case remapper::RotateOp::kTopIsLeftMirror:
      rotated_mrrg_config.column = mrrg_config.row;
      rotated_mrrg_config.row = mrrg_config.column;
      break;

    case remapper::RotateOp::kTopIsTopMirror:
      rotated_mrrg_config.column = mrrg_config.column;
      rotated_mrrg_config.row = mrrg_config.row;
      break;
  }

  return rotated_mrrg_config;
}

entity::Mapping remapper::MappingRotater(const entity::Mapping& mapping,
                                         const remapper::RotateOp& rotate_op) {
  entity::ConfigMap rotated_config_map;
  const auto& rotated_mrrg_config =
      RotateMRRGConfig(mapping.GetMRRGConfig(), rotate_op);

  for (const auto& cgra_config : mapping.GetConfigMap()) {
    entity::ConfigId rotated_config_id = remapper::RotateConfigId(
        cgra_config.first, mapping.GetMRRGConfig(), rotate_op);
    entity::CGRAConfig rotated_cgra_config = cgra_config.second;
    rotated_cgra_config.to_config_id_vec.clear();
    for (const auto& to_config_id : cgra_config.second.to_config_id_vec) {
      rotated_cgra_config.to_config_id_vec.push_back(remapper::RotateConfigId(
          to_config_id, mapping.GetMRRGConfig(), rotate_op));
    }

    rotated_cgra_config.from_config_id_vec.clear();
    for (const auto& from_config_id : cgra_config.second.from_config_id_vec) {
      rotated_cgra_config.from_config_id_vec.push_back(remapper::RotateConfigId(
          from_config_id, mapping.GetMRRGConfig(), rotate_op));
    }

    rotated_config_map.emplace(rotated_config_id, rotated_cgra_config);
  }

  return entity::Mapping(rotated_mrrg_config, rotated_config_map);
}

remapper::RotateOp remapper::Rotate180(const remapper::RotateOp& tmp) {
  if (tmp == remapper::RotateOp::kTopIsTop)
    return remapper::RotateOp::kTopIsBottom;
  if (tmp == remapper::RotateOp::kTopIsBottom)
    return remapper::RotateOp::kTopIsTop;
  if (tmp == remapper::RotateOp::kTopIsRight)
    return remapper::RotateOp::kTopIsLeft;
  if (tmp == remapper::RotateOp::kTopIsLeft)
    return remapper::RotateOp::kTopIsRight;
  if (tmp == remapper::RotateOp::kTopIsTopMirror)
    return remapper::RotateOp::kTopIsBottomMirror;
  if (tmp == remapper::RotateOp::kTopIsBottomMirror)
    return remapper::RotateOp::kTopIsTopMirror;
  if (tmp == remapper::RotateOp::kTopIsRightMirror)
    return remapper::RotateOp::kTopIsLeftMirror;
  if (tmp == remapper::RotateOp::kTopIsLeftMirror)
    return remapper::RotateOp::kTopIsRightMirror;
}
