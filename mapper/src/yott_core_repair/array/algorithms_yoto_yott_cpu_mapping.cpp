#include "engine.hpp"

namespace mapper::detail::yott_core_repair_array {

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

CPUMappingPlacementContext
Placement2DYOTTCoreRepairEngine::MakeCPUMappingContext() const {
  CPUMappingPlacementContext context;
  context.state.dfg_to_cell.assign(dfg_.GetNodeNum(), -1);
  context.state.cell_to_dfg.assign(rows_ * cols_, -1);
  context.freedom = InitialFreedomGrid();
  context.mutable_degree = degree_;
  return context;
}

}  // namespace mapper::detail::yott_core_repair_array
