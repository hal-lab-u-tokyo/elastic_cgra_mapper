#pragma once

#include "entity/mapper_config.hpp"

namespace mapper {
struct YOTTConfig : public entity::AlgorithmConfig {
  int max_trials;
  int seed_count;
  int routing_retry_count;
  int random_seed;
  int max_iterations;
  int elite_placement_count;
  std::string io_node_policy;
  std::string trial_seed_policy;
  std::string traversal_order_policy;
  std::string traversal_neighbor_policy;
  std::string candidate_scope_policy;
  std::string candidate_rank_policy;
  bool use_yott_annotations;
  bool trace_trials;
};
}  // namespace mapper
