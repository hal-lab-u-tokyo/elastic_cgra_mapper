#pragma once

#include <optional>
#include <string>

namespace entity {
struct DFGConfig {
  std::string operation_name_label = "node_id";
};

struct AlgorithmConfig {
  std::string type = "Placement2DILPMapper";
  bool accept_feasible_solution = true;
  bool placement_only = false;
  std::optional<int> max_trials;
  std::optional<int> seed_count;
  std::optional<int> routing_retry_count;
  std::optional<int> random_seed;
  std::optional<int> max_iterations;
  std::optional<int> elite_placement_count;
  std::optional<std::string> io_node_policy;
  std::optional<std::string> trial_seed_policy;
  std::optional<std::string> traversal_order_policy;
  std::optional<std::string> traversal_neighbor_policy;
  std::optional<std::string> candidate_scope_policy;
  std::optional<std::string> candidate_rank_policy;
  std::optional<bool> use_yott_annotations;
  std::optional<bool> trace_trials;
};

struct MapperConfig {
  DFGConfig dfg_config;
  AlgorithmConfig algorithm_config;
};
}  // namespace entity
