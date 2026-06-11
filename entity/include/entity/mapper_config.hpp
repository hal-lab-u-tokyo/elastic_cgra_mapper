#pragma once

#include <string>

namespace entity {
struct DFGConfig {
  std::string operation_name_label = "node_id";
};

struct AlgorithmConfig {
  std::string type = "ILPPlacementMapper";
  bool accept_feasible_solution = true;
};

struct MapperConfig {
  DFGConfig dfg_config;
  AlgorithmConfig algorithm_config;
};
}  // namespace entity
