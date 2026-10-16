#pragma once

#include <limits>
#include <vector>

namespace mapper::detail::prisa_array {

constexpr double kImpossibleCost = 1.0e12;

struct EdgeInfo {
  int source = -1;
  int target = -1;
};

struct PlacementState {
  std::vector<int> dfg_to_cell;
  std::vector<int> cell_to_dfg;
};

struct PRISAStats {
  std::vector<int> row_weak;
  std::vector<int> column_weak;
  std::vector<int> row_potential;
  std::vector<int> column_potential;
  std::vector<char> edge_weak;
  std::vector<char> edge_potential;
  int weak_edges = 0;
};

struct PlacementQuality {
  double cost = kImpossibleCost;
  int direct_edges = 0;
  int max_distance = 0;
};

struct CostAwarePRISAMetrics {
  double mesh_hop_sum = 0.0;
  int max_mesh_hop = 0;
  int mapped_lp_mesh_hop = 0;
  int direct_edge_count = 0;
};

}  // namespace mapper::detail::prisa_array
