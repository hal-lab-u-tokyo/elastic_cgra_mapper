#pragma once

#include <mapper/configuration.hpp>
#include <optional>

namespace mapper {

struct YOTTCoreConfig : AlgorithmConfig {
  YOTTCoreConfig() { type = "Placement2DCPUMappingYOTTCoreMapper"; }
  std::optional<int> max_trials;
  std::optional<int> seed_count;
  std::optional<int> random_seed;
  std::optional<std::string> io_node_policy;
  std::optional<std::string> trial_seed_policy;
  std::optional<std::string> traversal_order_policy;
  std::optional<std::string> traversal_neighbor_policy;
  std::optional<bool> use_yott_annotations;
  std::optional<bool> trace_trials;

  const char* FamilyName() const override { return "YOTTCore"; }
  void ReadOptions(const boost::property_tree::ptree& tree) override;
  void WriteOptions(boost::property_tree::ptree& tree) const override;

 protected:
  void ApplyOptions(IMapper& target) const override {
    if (max_trials) target.SetMaxTrials(*max_trials);
    if (seed_count) target.SetSeedCount(*seed_count);
    if (random_seed) target.SetRandomSeed(*random_seed);
    if (io_node_policy) target.SetIONodePolicy(*io_node_policy);
    if (trial_seed_policy) target.SetTrialSeedPolicy(*trial_seed_policy);
    if (traversal_order_policy)
      target.SetTraversalOrderPolicy(*traversal_order_policy);
    if (traversal_neighbor_policy)
      target.SetTraversalNeighborPolicy(*traversal_neighbor_policy);
    if (use_yott_annotations)
      target.SetUseYOTTAnnotations(*use_yott_annotations);
    if (trace_trials) target.SetTraceTrials(*trace_trials);
  }
};

}  // namespace mapper
