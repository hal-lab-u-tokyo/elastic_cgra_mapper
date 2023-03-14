#pragma once

#include <entity/mrrg.hpp>
#include <iostream>
#include <tuple>
#include <vector>

namespace entity {

struct PEPositionId {
  int row_id;
  int column_id;

  PEPositionId(int _row_id, int _column_id)
      : row_id(_row_id), column_id(_column_id) {}

  bool operator==(const PEPositionId& position_id) const {
    return row_id == position_id.row_id && column_id == position_id.column_id;
  }
};

struct HashPEPositionId {
 public:
  size_t operator()(const PEPositionId& config_id) const {
    return config_id.row_id * 1000 + config_id.column_id * 1000;
  }
};

struct ConfigId {
  int row_id;
  int column_id;
  int context_id;

  ConfigId() : row_id(-1), column_id(-1), context_id(-1){};

  ConfigId(int row_id_, int column_id_, int context_id_)
      : row_id(row_id_), column_id(column_id_), context_id(context_id_){};

  ConfigId(entity::MRRGNodeProperty mrrg_node_property) {
    row_id = mrrg_node_property.position_id.first;
    column_id = mrrg_node_property.position_id.second;
    context_id = mrrg_node_property.context_id;
  }

  bool operator==(const ConfigId& config_id) const {
    return row_id == config_id.row_id && column_id == config_id.column_id &&
           context_id == config_id.context_id;
  }

  PEPositionId GetPositionId() { return PEPositionId(row_id, column_id); }
};

struct HashConfigId {
 public:
  size_t operator()(const ConfigId& config_id) const {
    return config_id.row_id * 1000 + config_id.column_id * 1000 +
           config_id.context_id * 1000;
  }
};

struct CGRAConfig {
 public:
  int from_config_id_num;
  int const_value = 0;
  std::vector<ConfigId> to_config_id_vec;
  OpType operation_type;
  std::string operation_name;
  ConfigId from_config_id_vec[2];

  CGRAConfig()
      : from_config_id_num(0),
        operation_type(entity::OpType::NOP),
        to_config_id_vec({}){};
  CGRAConfig(entity::OpType op_type, std::string op_name)
      : from_config_id_num(0),
        operation_type(op_type),
        operation_name(op_name) {}

  static CGRAConfig GenerateInitialCGRAConfig() {
    CGRAConfig result;
    return result;
  }

  void AddFromConfig(ConfigId from_config_id, OpType op_type,
                     std::string op_name) {
    from_config_id_vec[from_config_id_num] = from_config_id;
    from_config_id_num++;
    if (from_config_id_num >= 3) {
      std::cerr << "from config id > 3" << std::endl;
      abort();
    }
    operation_type = op_type;
    operation_name = op_name;

    return;
  }

  void AddToConfig(ConfigId to_config_id, OpType op_type, std::string op_name) {
    to_config_id_vec.emplace_back(to_config_id);
    operation_type = op_type;
    operation_name = op_name;

    return;
  }

  void SetConstValue(int _const_value) {
    const_value = _const_value;
    return;
  }
};

}  // namespace entity