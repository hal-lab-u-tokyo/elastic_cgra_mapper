#include "remapper/transform.hpp"

entity::ConfigId remapper::RotateConfigId(
    const entity::ConfigId& config_id,
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
      rotated_row_id = target_mrrg_config.column - 1 - config_id.column_id;
      rotated_column_id = config_id.row_id;
      break;

    case remapper::RotateOp::TopIsTop:
      rotated_row_id = config_id.row_id;
      rotated_column_id = config_id.column_id;
      break;

    case remapper::RotateOp::TopIsRightMirror:
      rotated_row_id = config_id.column_id;
      rotated_column_id = config_id.row_id;
      break;

    case remapper::RotateOp::TopIsBottomMirror:
      rotated_row_id = target_mrrg_config.row - 1 - config_id.row_id;
      rotated_column_id = config_id.column_id;
      break;

    case remapper::RotateOp::TopIsLeftMirror:
      rotated_row_id = target_mrrg_config.column - 1 - config_id.column_id;
      rotated_column_id = target_mrrg_config.row - 1 - config_id.row_id;
      break;

    case remapper::RotateOp::TopIsTopMirror:
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
    case remapper::RotateOp::TopIsRight:
      rotated_mrrg_config.column = mrrg_config.row;
      rotated_mrrg_config.row = mrrg_config.column;
      break;

    case remapper::RotateOp::TopIsBottom:
      rotated_mrrg_config.column = mrrg_config.column;
      rotated_mrrg_config.row = mrrg_config.row;
      break;

    case remapper::RotateOp::TopIsLeft:
      rotated_mrrg_config.column = mrrg_config.row;
      rotated_mrrg_config.row = mrrg_config.column;
      break;

    case remapper::RotateOp::TopIsTop:
      rotated_mrrg_config.column = mrrg_config.column;
      rotated_mrrg_config.row = mrrg_config.row;
      break;

    case remapper::RotateOp::TopIsRightMirror:
      rotated_mrrg_config.column = mrrg_config.row;
      rotated_mrrg_config.row = mrrg_config.column;
      break;

    case remapper::RotateOp::TopIsBottomMirror:
      rotated_mrrg_config.column = mrrg_config.column;
      rotated_mrrg_config.row = mrrg_config.row;
      break;

    case remapper::RotateOp::TopIsLeftMirror:
      rotated_mrrg_config.column = mrrg_config.row;
      rotated_mrrg_config.row = mrrg_config.column;
      break;

    case remapper::RotateOp::TopIsTopMirror:
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

    for (int i = 0; i < cgra_config.second.from_config_id_num; i++) {
      rotated_cgra_config.from_config_id_vec[i] =
          remapper::RotateConfigId(cgra_config.second.from_config_id_vec[i],
                                   mapping.GetMRRGConfig(), rotate_op);
    }

    rotated_config_map.emplace(rotated_config_id, rotated_cgra_config);
  }

  return entity::Mapping(rotated_mrrg_config, rotated_config_map);
}

remapper::RotateOp remapper::Rotate180(const remapper::RotateOp& tmp) {
  if (tmp == remapper::RotateOp::TopIsTop)
    return remapper::RotateOp::TopIsBottom;
  if (tmp == remapper::RotateOp::TopIsBottom)
    return remapper::RotateOp::TopIsTop;
  if (tmp == remapper::RotateOp::TopIsRight)
    return remapper::RotateOp::TopIsLeft;
  if (tmp == remapper::RotateOp::TopIsLeft)
    return remapper::RotateOp::TopIsRight;
  if (tmp == remapper::RotateOp::TopIsTopMirror)
    return remapper::RotateOp::TopIsBottomMirror;
  if (tmp == remapper::RotateOp::TopIsBottomMirror)
    return remapper::RotateOp::TopIsTopMirror;
  if (tmp == remapper::RotateOp::TopIsRightMirror)
    return remapper::RotateOp::TopIsLeftMirror;
  if (tmp == remapper::RotateOp::TopIsLeftMirror)
    return remapper::RotateOp::TopIsRightMirror;
  else {
    std::cout << "Error: Rotate180" << std::endl;
    assert(false);
  }
}
