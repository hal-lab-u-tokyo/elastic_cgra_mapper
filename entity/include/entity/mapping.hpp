#pragma once

#include <entity/architecture.hpp>
#include <entity/dfg.hpp>
#include <entity/mrrg.hpp>
#include <map>
#include <tuple>

namespace entity {
class Mapping {
 public:
  Mapping(MRRG mrrg, DFG dfg);

 private:
  std::unordered_map<int, int> config_id_to_dfg_node_id_;
  std::unordered_map<int, int> config_map_;
  // std::unordered_map<ConfigId, int> config_id_to_dfg_node_id_;
  // std::unordered_map<ConfigId, CGRAConfig> config_map_;
  std::shared_ptr<MRRG> mrrg_ptr_;
  std::shared_ptr<DFG> dfg_ptr_;
};
}  // namespace entity