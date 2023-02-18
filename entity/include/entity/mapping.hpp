#pragma once

#include <entity/architecture.hpp>
#include <entity/dfg.hpp>
#include <entity/mrrg.hpp>
#include <map>
#include <tuple>

namespace entity {
class Mapping {
 public:
  Mapping(MRRG mrrg, DFG dfg, const std::vector<int>& dfg_node_to_mrrg_node,
          const std::vector<std::vector<int>>& dfg_output_to_mrrg_reg);
  Mapping(bool is_succeed);
  bool Succeed() { return is_succeed_; };

 private:
  std::unordered_map<ConfigId, int, entity::HashConfigId>
      config_id_to_dfg_node_id_;
  std::unordered_map<ConfigId, CGRAConfig, entity::HashConfigId> config_map_;
  std::shared_ptr<MRRG> mrrg_ptr_;
  std::shared_ptr<DFG> dfg_ptr_;
  bool is_succeed_;
};
}  // namespace entity