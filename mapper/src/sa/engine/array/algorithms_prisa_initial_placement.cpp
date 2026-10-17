#include "engine.hpp"

namespace mapper::detail::sa_array {

// Initial placement and SIS order.

std::optional<PlacementState> Placement2DSAEngine::RandomPlacement() {
  PlacementState state;
  state.dfg_to_cell.assign(dfg_.GetNodeNum(), -1);
  state.cell_to_dfg.assign(rows_ * cols_, -1);
  std::vector<int> nodes(dfg_.GetNodeNum());
  std::iota(nodes.begin(), nodes.end(), 0);
  std::shuffle(nodes.begin(), nodes.end(), rng_);
  std::stable_sort(nodes.begin(), nodes.end(), [&](int a, int b) {
    if (IsIONode(a) != IsIONode(b)) return IsIONode(a) > IsIONode(b);
    return degree_[a] > degree_[b];
  });
  for (int node : nodes) {
    auto cells = CompatibleCells(node, state);
    if (cells.empty()) return std::nullopt;
    std::uniform_int_distribution<int> dist(0,
                                            static_cast<int>(cells.size()) - 1);
    PlaceNode(node, cells[dist(rng_)], state);
  }
  return state;
}

}  // namespace mapper::detail::sa_array
