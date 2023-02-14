#pragma once

#include <entity/mrrg.hpp>
#include <tuple>
#include <vector>

namespace entity {

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
};

struct HashConfigId {
 public:
  size_t operator()(const ConfigId& config_id) const {
    return config_id.row_id * 1000 + config_id.column_id * 1000 +
           config_id.context_id * 1000;
  }
};

struct CGRAConfig {
  ConfigId from_config_id_vec[2];
  std::vector<ConfigId> to_config_id_vec;
  OpType operation_type;
};

}  // namespace entity