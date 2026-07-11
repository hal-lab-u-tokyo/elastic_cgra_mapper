# Direct-Grid Engine

Array-backed 2D placement for YOTO/YOTT variants, PRISA, and SA.

## Pipeline

```text
Placement2DArrayMapperBase
  -> Placement2DArrayOptions
  -> Placement2DArrayEngine
       1. build DFG, grid, and compatibility caches
       2. dispatch to one algorithm
       3. keep the best placement
       4. convert cell IDs to MRRG node IDs
  -> MappingResult
```

## Files

```text
array_engine.cpp
array_engine_internal.hpp
array_engine_types.hpp

support/
  cache.cpp                  DFG/grid compatibility and distance
  options.cpp                defaults, policies, seeds, timeout, trace
  quality.cpp                placement objectives and comparison
  traversal.cpp              graph scores and shared traversal helpers

algorithms/yoto_yott/
  paper_guided.cpp           paper-guided traversal variant
  cpu_mapping_plan.cpp       cpu_mapping traversal and annotations
  traversal_order.cpp        DFS, BFS, and random-order traversal baselines
  cpu_mapping_candidates.cpp candidate cells and local placement
  cpu_mapping.cpp            trial construction and multi-start
  yott_core.cpp              compact YOTT Core
  yott_core_repair_candidates.cpp optional weighted construction
  yott_core_repair_objective.cpp  optional weighted objective
  yott_core_repair.cpp       repair-policy dispatch
  yott_core_profile_repair.cpp guarded local improvement

algorithms/prisa/
  initial_placement.cpp
  moves.cpp
  refinement.cpp

algorithms/sa/
  sa.cpp
```

## YOTT Paths

```text
CPU Mapping YOTT
  plan -> candidates -> trial loop

YOTT Core
  traversal plan -> annotated tips -> local fallback -> best trial

YOTT Core Repair
  YOTT Core trials -> keep four placements by placement cost
  -> relocation, swap, and ejection-chain search
  -> select only a result that does not regress from YOTT Core
```

The default repair minimizes maximal FIFO while preserving the YOTT Core
baseline's direct-edge count, total FIFO, and maximal FIFO. Four elites and 16
repair iterations capture nearly all of the measured gain. The alternative
`core_tail_repair` policy requires the complete FIFO-tail distribution to be
non-worsening after every move.

The algorithm files show the processing order at their top-level multi-start
function. Shared legality, distance, and quality definitions remain under
`support/`.

For YOTT Core, `traversal_order_policy` selects `zigzag`, `depth_first`,
`breadth_first`, or `fully_random`. With `zigzag`,
`traversal_neighbor_policy` selects `degree`, `betweenness`, `critical_path`,
`zigzag`, or `random`. Here `critical_path` ranks by input-distance score, and
`zigzag` alternates between the front and back of the current neighbor list.
