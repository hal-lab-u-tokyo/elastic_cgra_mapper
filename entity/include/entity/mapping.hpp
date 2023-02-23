#pragma once

#include <entity/architecture.hpp>
#include <entity/dfg.hpp>
#include <entity/mrrg.hpp>
#include <map>
#include <tuple>

namespace entity {
typedef std::unordered_map<ConfigId, CGRAConfig, entity::HashConfigId>
    ConfigMap;

class Mapping {
 public:
  Mapping() : config_map_({}){};
  Mapping(entity::MRRGConfig mrrg_config)
      : config_map_({}), mrrg_config_(mrrg_config){};
  Mapping(MRRG mrrg, DFG dfg, const std::vector<int>& dfg_node_to_mrrg_node,
          const std::vector<std::vector<int>>& dfg_output_to_mrrg_reg);
  Mapping(entity::MRRGConfig mrrg_config, ConfigMap config_map)
      : config_map_(config_map), mrrg_config_(mrrg_config){};
  ConfigMap GetConfigMap() { return config_map_; };

 private:
  entity::MRRGConfig mrrg_config_;
  ConfigMap config_map_;
};
}  // namespace entity