#include "../../array_engine_internal.hpp"

namespace mapper::detail::placement2d {

// cpu_mapping-style YOTO/YOTT pipeline.
//
//   1. BuildCPUMappingPlan()
//      Port the public cpu_mapping edge traversal order. YOTT attaches
//      reconvergence annotations after the plan is built.
//   2. MakeCPUMappingContext()
//      Keep placement, freedom grid, and mutable node degree together.
//   3. ConstructCPUMappingPlacement(plan)
//      Walk the plan. Place missing endpoints using the selected YOTO/YOTT
//      step policy.
//   4. PlaceCPUMappingOrientedStep()
//      Dispatch one edge-local step to YOTO adjacency placement or YOTT
//      annotated-tip placement.
//   5. RunCPUMappingMultiStart()
//      Repeat plan generation and placement, then keep the lowest
//      cpu_mapping-style traversal edge cost.
//
// Ablation handles:
//   - traversal order: BuildCPUMappingPlan()
//   - neighbor choice: ChooseCPUMappingZigZagNeighbor()
//   - YOTT cycle hints: ApplyCPUMappingCycleAnnotations()
//   - local cells/tips: CPUMappingTipCells(), TryCPUMappingAdjacency()
//   - cell choice: BestCPUMappingDegreeCell()

CPUMappingPlacementContext Placement2DArrayEngine::MakeCPUMappingContext()
    const {
  CPUMappingPlacementContext context;
  context.state.dfg_to_cell.assign(dfg_.GetNodeNum(), -1);
  context.state.cell_to_dfg.assign(rows_ * cols_, -1);
  context.freedom = InitialFreedomGrid();
  context.mutable_degree = degree_;
  return context;
}

std::optional<CPUMappingTrialResult>
Placement2DArrayEngine::ConstructCPUMappingPlacement(
    const std::vector<Step>& plan) {
  CPUMappingPlacementContext context = MakeCPUMappingContext();
  double traversal_edge_cost = 0.0;

  auto add_edge_cost = [&](const Step& step) {
    const int anchor_cell = context.state.dfg_to_cell[step.anchor];
    const int target_cell = context.state.dfg_to_cell[step.target];
    if (anchor_cell >= 0 && target_cell >= 0) {
      traversal_edge_cost += DistanceCost(anchor_cell, target_cell);
    } else {
      traversal_edge_cost += 1000.0;
    }
  };

  for (const auto& step : plan) {
    const bool anchor_placed = context.state.dfg_to_cell[step.anchor] >= 0;
    const bool target_placed = context.state.dfg_to_cell[step.target] >= 0;
    if (!anchor_placed && !target_placed) {
      if (!PlaceCPUMappingInitialNode(step.anchor, context)) {
        return std::nullopt;
      }
      PlaceCPUMappingOrientedStep(step, context);
    } else if (anchor_placed && !target_placed) {
      PlaceCPUMappingOrientedStep(step, context);
    } else if (!anchor_placed && target_placed) {
      Step reversed = step;
      reversed.anchor = step.target;
      reversed.target = step.anchor;
      PlaceCPUMappingOrientedStep(reversed, context);
    }
    add_edge_cost(step);
  }

  for (int node = 0; node < dfg_.GetNodeNum(); node++) {
    if (!PlaceCPUMappingInitialNode(node, context)) {
      return std::nullopt;
    }
  }
  CPUMappingTrialResult result;
  result.state = context.state;
  result.traversal_edge_cost = traversal_edge_cost;
  result.final_placement_cost = PlacementCost(context.state);
  return result;
}

std::optional<CPUMappingTrialResult>
Placement2DArrayEngine::ConstructCPUMappingPlacement() {
  return ConstructCPUMappingPlacement(BuildCPUMappingPlan());
}

std::optional<PlacementState> Placement2DArrayEngine::RunCPUMappingMultiStart(
    std::chrono::steady_clock::time_point start) {
  std::optional<PlacementState> best;
  double best_traversal_edge_cost = std::numeric_limits<double>::infinity();
  double best_final_placement_cost = std::numeric_limits<double>::infinity();
  int trials = 0;
  std::ofstream trace;
  if (TraceTrials()) {
    trace.open(TrialTracePath());
    if (trace) WriteTrialTraceHeader(trace);
  }

  const auto evaluate_trial = [&](int seed, int trial,
                                  const std::vector<Step>& plan) {
    auto trial_result = ConstructCPUMappingPlacement(plan);
    trials++;
    bool better = false;
    std::optional<PlacementState> trace_state;
    double traversal_edge_cost = kImpossibleCost;
    double final_placement_cost = kImpossibleCost;
    if (trial_result.has_value()) {
      trace_state = trial_result->state;
      traversal_edge_cost = trial_result->traversal_edge_cost;
      final_placement_cost = trial_result->final_placement_cost;
      better = traversal_edge_cost < best_traversal_edge_cost ||
               (traversal_edge_cost == best_traversal_edge_cost &&
                final_placement_cost < best_final_placement_cost);
    }
    if (trace) {
      WriteTrialTraceRow(trace, "cpu_mapping", seed, trial,
                         trial_result.has_value(), better, traversal_edge_cost,
                         final_placement_cost, trace_state, SecondsSince(start),
                         PlanHash(plan));
    }
    if (!trial_result.has_value() || !better) return;
    best = trial_result->state;
    best_traversal_edge_cost = trial_result->traversal_edge_cost;
    best_final_placement_cost = trial_result->final_placement_cost;
  };

  for (int seed = 0; seed < SeedCount() && !HasTimedOut(start, 0.05); seed++) {
    ResetSeed(seed);
    if (UsesPerTrialSeeds()) {
      for (int trial = 0; trial < MaxTrials() && !HasTimedOut(start, 0.05);
           trial++) {
        ResetTrialSeed(seed, trial);
        const std::vector<Step> plan = BuildCPUMappingPlan();
        evaluate_trial(seed, trial, plan);
      }
      continue;
    }

    std::vector<std::vector<Step>> plans;
    plans.reserve(MaxTrials());
    for (int trial = 0; trial < MaxTrials() && !HasTimedOut(start, 0.05);
         trial++) {
      plans.push_back(BuildCPUMappingPlan());
    }
    for (int trial = 0; trial < static_cast<int>(plans.size()); trial++) {
      if (HasTimedOut(start, 0.05)) break;
      evaluate_trial(seed, trial, plans[trial]);
    }
  }
  Log("cpu_mapping_traversal trials=" + std::to_string(trials) +
      " best_traversal_edge_cost=" + std::to_string(best_traversal_edge_cost) +
      " best_final_placement_cost=" +
      std::to_string(best_final_placement_cost));
  return best;
}

}  // namespace mapper::detail::placement2d
