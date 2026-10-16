#pragma once

#include <entity/mapper_config.hpp>
#include <optional>
#include <string>

namespace mapper {

// Registry names and algorithm-specific runtime options extend the reviewed
// entity configuration without changing its typed model. Runtime dispatch uses
// type; entity::AlgorithmType remains the historical entity classification.
struct AlgorithmConfig : entity::AlgorithmConfig {
  std::string type = "Placement2DILPMapper";
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
  entity::DFGConfig dfg_config;
  AlgorithmConfig algorithm_config;
};
}  // namespace mapper
