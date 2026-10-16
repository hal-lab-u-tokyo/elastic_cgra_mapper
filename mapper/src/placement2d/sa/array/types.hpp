#pragma once

#include <limits>
#include <vector>

namespace mapper::detail::placement2d_sa_array {

constexpr double kImpossibleCost = 1.0e12;

struct EdgeInfo {
  int source = -1;
  int target = -1;
};

struct PlacementState {
  std::vector<int> dfg_to_cell;
  std::vector<int> cell_to_dfg;
};

}  // namespace mapper::detail::placement2d_sa_array
