#pragma once

#include <entity/dfg.hpp>
#include <tuple>
#include <vector>

namespace entity {

struct ConfigId {
  int row_id;
  int column_id;
  int context_id;
};

struct CGRAConfig {
  ConfigId from_config_id_vec[2];
  std::vector<ConfigId> to_config_id_vec;
  OpType operation_type;
};

}  // namespace entity