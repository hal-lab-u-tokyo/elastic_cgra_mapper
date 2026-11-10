#pragma once

#include <mapper/configuration.hpp>
#include <optional>

namespace mapper {

struct SAConfig : AlgorithmConfig {
  SAConfig() { type = "SA"; }
  std::optional<int> seed_count;
  std::optional<int> routing_retry_count;
  std::optional<int> random_seed;
  std::optional<int> max_iterations;
  std::optional<std::string> io_node_policy;
  bool placement_only = false;

  const char* FamilyName() const override { return "SA"; }
  void ReadOptions(const boost::property_tree::ptree& tree) override;
  void WriteOptions(boost::property_tree::ptree& tree) const override;

 protected:
  void ApplyOptions(IMapper& target) const override {
    if (seed_count) target.SetSeedCount(*seed_count);
    if (routing_retry_count) target.SetRoutingRetryCount(*routing_retry_count);
    if (random_seed) target.SetRandomSeed(*random_seed);
    if (max_iterations) target.SetMaxIterations(*max_iterations);
    if (io_node_policy) target.SetIONodePolicy(*io_node_policy);
    target.SetPlacementOnly(placement_only);
  }
};

}  // namespace mapper
