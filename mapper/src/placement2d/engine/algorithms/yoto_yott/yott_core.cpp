#include "../../array_engine_internal.hpp"

namespace mapper::detail::placement2d {

// YOTT Core keeps only the compact traversal pieces:
// random local traversal, reconvergence annotations, annotated tip placement,
// nearest-cell fallback, and best-of-trials selection by PlacementCost().

std::vector<Step> Placement2DArrayEngine::BuildYOTTCorePlan() {
  if (traversal_order_policy_ == "depth_first") {
    return BuildGraphSearchPlan(false);
  }
  if (traversal_order_policy_ == "breadth_first") {
    return BuildGraphSearchPlan(true);
  }
  if (traversal_order_policy_ == "fully_random") {
    return BuildFullyRandomPlan();
  }
  if (traversal_order_policy_ != "zigzag" &&
      traversal_order_policy_ != "default") {
    Log("unknown traversal_order_policy=" + traversal_order_policy_ +
        "; using zigzag");
  }

  const std::string saved_policy = traversal_neighbor_policy_;
  if (traversal_neighbor_policy_ == "default") {
    traversal_neighbor_policy_ = "random";
  }
  std::vector<Step> plan = BuildCPUMappingPlan();
  traversal_neighbor_policy_ = saved_policy;
  return plan;
}

int Placement2DArrayEngine::ChooseYOTTCoreCell(
    const std::vector<std::pair<int, int>>& cells, int dfg_node,
    const CPUMappingPlacementContext& context) {
  std::vector<int> valid_cells;
  valid_cells.reserve(cells.size());
  for (const auto& [unused_freedom, cell] : cells) {
    (void)unused_freedom;
    if (CanPlaceCPUMapping(dfg_node, cell, context.state)) {
      valid_cells.push_back(cell);
    }
  }
  if (valid_cells.empty()) return -1;
  RecordPlacementSwapAttempts(static_cast<long long>(valid_cells.size()));
  std::uniform_int_distribution<int> dist(
      0, static_cast<int>(valid_cells.size()) - 1);
  return valid_cells[dist(rng_)];
}

bool Placement2DArrayEngine::PlaceYOTTCoreStep(
    const Step& step, CPUMappingPlacementContext& context) {
  const int dfg_node = step.target;
  if (context.state.dfg_to_cell[dfg_node] >= 0) return true;

  const int anchor_cell = context.state.dfg_to_cell[step.anchor];
  if (anchor_cell < 0) {
    return PlaceCPUMappingInitialNode(dfg_node, context);
  }

  std::vector<std::pair<int, int>> tips =
      CPUMappingTipCells(anchor_cell, dfg_node, context);
  std::vector<std::pair<int, int>> annotated_tips = tips;
  std::vector<int> placed_annotation_nodes;

  for (const auto& annotation : step.annotations) {
    if (annotation.kind != StepAnnotationKind::kReconvergence) continue;
    if (annotation.distance > 2) continue;
    const int annotated_cell =
        context.state.dfg_to_cell[annotation.anchor_node];
    if (annotated_cell < 0) continue;
    placed_annotation_nodes.push_back(annotation.anchor_node);
    auto cells = CPUMappingCellsWithinAnnotatedDistance(
        annotated_cell, annotation.distance, context);
    annotated_tips = IntersectCPUMappingCellPairs(annotated_tips, cells);
    if (annotated_tips.empty()) break;
  }

  int cell = ChooseYOTTCoreCell(annotated_tips, dfg_node, context);
  if (cell >= 0) {
    PlaceCPUMappingNode(dfg_node, cell, context);
    return true;
  }

  if (!tips.empty()) {
    std::vector<std::pair<int, int>> candidates = tips;
    if (!placed_annotation_nodes.empty()) {
      int best_cost = std::numeric_limits<int>::max();
      std::vector<std::pair<int, int>> closest_tips;
      for (const auto& [available, candidate] : tips) {
        int cost = 0;
        for (int annotated_node : placed_annotation_nodes) {
          const int annotated_cell = context.state.dfg_to_cell[annotated_node];
          if (annotated_cell >= 0)
            cost += DistanceCost(annotated_cell, candidate);
        }
        if (cost < best_cost) {
          best_cost = cost;
          closest_tips.clear();
          closest_tips.push_back({available, candidate});
        } else if (cost == best_cost) {
          closest_tips.push_back({available, candidate});
        }
      }
      if (!closest_tips.empty()) candidates = closest_tips;
    }

    cell = ChooseYOTTCoreCell(candidates, dfg_node, context);
    if (cell >= 0) {
      PlaceCPUMappingNode(dfg_node, cell, context);
      return true;
    }
  }

  return TryCPUMappingAdjacency(dfg_node, anchor_cell, context, true);
}

std::optional<PlacementState>
Placement2DArrayEngine::ConstructYOTTCorePlacement(
    const std::vector<Step>& plan) {
  CPUMappingPlacementContext context = MakeCPUMappingContext();

  for (const auto& step : plan) {
    const bool anchor_placed = context.state.dfg_to_cell[step.anchor] >= 0;
    const bool target_placed = context.state.dfg_to_cell[step.target] >= 0;
    if (!anchor_placed && !target_placed) {
      if (!PlaceCPUMappingInitialNode(step.anchor, context)) {
        return std::nullopt;
      }
      PlaceYOTTCoreStep(step, context);
    } else if (anchor_placed && !target_placed) {
      PlaceYOTTCoreStep(step, context);
    } else if (!anchor_placed && target_placed) {
      Step reversed = step;
      reversed.anchor = step.target;
      reversed.target = step.anchor;
      PlaceYOTTCoreStep(reversed, context);
    }
  }

  for (int node = 0; node < dfg_.GetNodeNum(); node++) {
    if (!PlaceCPUMappingInitialNode(node, context)) {
      return std::nullopt;
    }
  }
  return context.state;
}

std::optional<PlacementState> Placement2DArrayEngine::RunYOTTCoreMultiStart(
    std::chrono::steady_clock::time_point start) {
  std::optional<PlacementState> best;
  double best_cost = std::numeric_limits<double>::infinity();
  int trials = 0;
  std::ofstream trace;
  if (TraceTrials()) {
    trace.open(TrialTracePath());
    if (trace) WriteTrialTraceHeader(trace);
  }
  for (int seed = 0; seed < SeedCount() && !HasTimedOut(start, 0.05); seed++) {
    ResetSeed(seed);
    for (int trial = 0; trial < MaxTrials() && !HasTimedOut(start, 0.05);
         trial++) {
      if (UsesPerTrialSeeds()) ResetTrialSeed(seed, trial);
      const std::vector<Step> plan = BuildYOTTCorePlan();
      auto placement = ConstructYOTTCorePlacement(plan);
      trials++;
      const double cost =
          placement.has_value() ? PlacementCost(*placement) : kImpossibleCost;
      const bool better = placement.has_value() && cost < best_cost;
      if (trace) {
        WriteTrialTraceRow(trace, "yott_core", seed, trial,
                           placement.has_value(), better, cost, 0.0, placement,
                           SecondsSince(start), PlanHash(plan));
      }
      if (!placement.has_value()) continue;
      if (better) {
        best = placement;
        best_cost = cost;
      }
    }
  }
  Log("yott_core trials=" + std::to_string(trials));
  return best;
}

}  // namespace mapper::detail::placement2d
