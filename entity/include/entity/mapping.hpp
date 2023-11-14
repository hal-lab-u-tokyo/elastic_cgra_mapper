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
  MRRGConfig GetMRRGConfig() const { return mrrg_config_; };
  ConfigMap GetConfigMap() const { return config_map_; };
  CGRAConfig GetConfig(ConfigId config_id) const {
    if (config_map_.count(config_id) > 0) {
      return config_map_.at(config_id);
    } else {
      return CGRAConfig(OpType::NOP, OpTypeToString(OpType::NOP));
    }
  };
  size_t GetOpNum() const;

 private:
  MRRGConfig mrrg_config_;
  ConfigMap config_map_;
};
}  // namespace entity