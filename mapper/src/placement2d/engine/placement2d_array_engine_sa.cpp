#include "placement2d_array_engine_internal.hpp"

namespace mapper::detail::placement2d {

bool Placement2DArrayEngine::ApplySwap(PlacementState& state, int a, int b) {
  if (a == b) return false;
  const int cell_a = state.dfg_to_cell[a];
  const int cell_b = state.dfg_to_cell[b];
  if (cell_a < 0 || cell_b < 0) return false;
  if (!IsCompatible(a, cell_b) || !IsCompatible(b, cell_a)) return false;
  state.dfg_to_cell[a] = cell_b;
  state.dfg_to_cell[b] = cell_a;
  state.cell_to_dfg[cell_a] = b;
  state.cell_to_dfg[cell_b] = a;
  return true;
}

bool Placement2DArrayEngine::ApplyMoveToFreeCell(PlacementState& state, int node) {
  auto cells = CompatibleCells(node, state);
  if (cells.empty()) return false;
  std::uniform_int_distribution<int> dist(0, static_cast<int>(cells.size()) - 1);
  const int old_cell = state.dfg_to_cell[node];
  const int new_cell = cells[dist(rng_)];
  state.cell_to_dfg[old_cell] = -1;
  state.cell_to_dfg[new_cell] = node;
  state.dfg_to_cell[node] = new_cell;
  return true;
}

std::optional<PlacementState> Placement2DArrayEngine::RunSA(std::chrono::steady_clock::time_point start) {
  auto current = RandomPlacement();
  if (!current.has_value()) return std::nullopt;
  PlacementState best = *current;
  double current_cost = PlacementCost(*current);
  double best_cost = current_cost;
  double temperature = std::max(10.0, current_cost / std::max(1, dfg_.GetNodeNum()));
  std::uniform_real_distribution<double> unit(0.0, 1.0);
  std::uniform_int_distribution<int> node_dist(0, std::max(0, dfg_.GetNodeNum() - 1));

  for (int iteration = 0;
       iteration < MaxIterations() && !HasTimedOut(start, 0.05);
       iteration++) {
    PlacementState next = *current;
    bool changed = false;
    if (unit(rng_) < 0.75 || dfg_.GetNodeNum() < rows_ * cols_) {
      changed = ApplySwap(next, node_dist(rng_), node_dist(rng_));
    } else {
      changed = ApplyMoveToFreeCell(next, node_dist(rng_));
    }
    if (!changed) continue;
    const double next_cost = PlacementCost(next);
    const double delta = next_cost - current_cost;
    if (delta <= 0.0 || unit(rng_) < std::exp(-delta / temperature)) {
      *current = next;
      current_cost = next_cost;
    }
    if (next_cost < best_cost) {
      best = next;
      best_cost = next_cost;
    }
    temperature *= 0.9975;
    if (temperature < 1.0e-4) temperature = std::max(1.0, best_cost / 10.0);
  }
  return best;
}

std::optional<PlacementState> Placement2DArrayEngine::RunSAMultiSeed(
    std::chrono::steady_clock::time_point start) {
  std::optional<PlacementState> best;
  double best_cost = std::numeric_limits<double>::infinity();
  int seeds = 0;
  for (int seed = 0; seed < SeedCount() && !HasTimedOut(start, 0.05); seed++) {
    ResetSeed(seed);
    auto placement = RunSA(start);
    seeds++;
    if (!placement.has_value()) continue;
    const double cost = PlacementCost(*placement);
    if (cost < best_cost) {
      best = placement;
      best_cost = cost;
    }
  }
  Log("array_sa seeds=" + std::to_string(seeds));
  return best;
}

}  // namespace mapper::detail::placement2d
