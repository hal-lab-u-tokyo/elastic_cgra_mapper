#pragma once

#include <optional>
#include <string>

namespace entity {
struct DFGConfig {
  std::string operation_name_label = "node_id";
};

struct AlgorithmConfig {
  std::string type = "PlacementOnlyILPMapper";
  bool accept_feasible_solution = true;
  bool placement_only = false;
  std::optional<int> max_trials;
  std::optional<int> seed_count;
  std::optional<int> routing_retry_count;
  std::optional<int> random_seed;
  std::optional<int> max_iterations;
};

struct MapperConfig {
  DFGConfig dfg_config;
  AlgorithmConfig algorithm_config;
};
}  // namespace entity
