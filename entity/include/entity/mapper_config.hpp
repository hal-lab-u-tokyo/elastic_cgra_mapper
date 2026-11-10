#pragma once

#include <string>

namespace entity {
struct DFGConfig {
  std::string operation_name_label = "node_id";
};

enum class AlgorithmType { kILPMapper, kPlacementILPMapper, kYOTTMapper };

struct AlgorithmConfig {
  AlgorithmType algorithm = AlgorithmType::kPlacementILPMapper;
  bool accept_feasible_solution = true;
  bool placement_only = false;
};

struct MapperConfig {
  DFGConfig dfg_config;
  AlgorithmConfig algorithm_config;
};
}  // namespace entity
