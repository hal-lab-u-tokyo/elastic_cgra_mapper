#pragma once

#include <mapper/configuration.hpp>
#include <optional>

namespace mapper {

struct YOTOConfig : AlgorithmConfig {
  YOTOConfig() { type = "YOTO"; }
  std::optional<int> max_trials;
  std::optional<int> seed_count;
  std::optional<int> routing_retry_count;
  std::optional<int> random_seed;
  std::optional<std::string> io_node_policy;
  std::optional<std::string> trial_seed_policy;
  std::optional<std::string> traversal_neighbor_policy;
  std::optional<std::string> candidate_scope_policy;
  std::optional<std::string> candidate_rank_policy;
  std::optional<bool> use_yott_annotations;
  std::optional<bool> trace_trials;
  bool placement_only = false;

  const char* FamilyName() const override { return "YOTO"; }
  void ReadOptions(const boost::property_tree::ptree& tree) override;
  void WriteOptions(boost::property_tree::ptree& tree) const override;

 protected:
  void ApplyOptions(IMapper& target) const override {
    if (max_trials) target.SetMaxTrials(*max_trials);
    if (seed_count) target.SetSeedCount(*seed_count);
    if (routing_retry_count) target.SetRoutingRetryCount(*routing_retry_count);
    if (random_seed) target.SetRandomSeed(*random_seed);
    if (io_node_policy) target.SetIONodePolicy(*io_node_policy);
    if (trial_seed_policy) target.SetTrialSeedPolicy(*trial_seed_policy);
    if (traversal_neighbor_policy)
      target.SetTraversalNeighborPolicy(*traversal_neighbor_policy);
    if (candidate_scope_policy)
      target.SetCandidateScopePolicy(*candidate_scope_policy);
    if (candidate_rank_policy)
      target.SetCandidateRankPolicy(*candidate_rank_policy);
    if (use_yott_annotations)
      target.SetUseYOTTAnnotations(*use_yott_annotations);
    if (trace_trials) target.SetTraceTrials(*trace_trials);
    target.SetPlacementOnly(placement_only);
  }
};

}  // namespace mapper
