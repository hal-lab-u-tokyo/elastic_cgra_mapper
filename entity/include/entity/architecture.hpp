#pragma once

#include <entity/dfg.hpp>
#include <tuple>
#include <vector>

namespace entity {

struct ConfigId {
  int row_id;
  int column_id;
  int context_id;

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