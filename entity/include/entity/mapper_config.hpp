#pragma once

#include <string>

namespace entity {
struct DFGConfig {
  std::string operation_name_label = "node_id";
};

enum class AlgorithmType { kILPMapper, kPlacementILPMapper };

struct AlgorithmConfig {
  AlgorithmType algorithm = AlgorithmType::kPlacementILPMapper;
};

struct MapperConfig {
  DFGConfig dfg_config;
  AlgorithmConfig algorithm_config;
};
}  // namespace entity
