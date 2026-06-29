# 2D Placement Mappers

Mapper frontends for 2D placement. One physical PE is one placement slot, so experiments run with II/context size 1.

## Mapper Families

- `placement2d_yoto_mapper.cpp`: YOTO variants and `Algorithm.type` registration.
- `placement2d_yott_mapper.cpp`: YOTT variants and `Algorithm.type` registration.
- `placement2d_prisa_mapper.cpp`: PRISA, PRISA without SIS, cost-aware PRISA, and array PRISA variants.
- `placement2d_sa_mapper.cpp`: SA/VPR-facing placement baselines.
- `placement2d_ilp_mapper.cpp`: placement-only Gurobi ILP baseline.
- `placement2d_search_engine.cpp`: bridge to the generic search engine in `../common/search/`.
- `placement2d_array_mapper_base.cpp`: config wrapper for direct-grid array/cpu_mapping mappers.

## Execution Flow

1. `research/scripts/run_suite.py` chooses a benchmark, architecture, and mapper config.
2. The mapper config `Algorithm.type` selects a registered C++ mapper through `mapper_factory`.
3. The mapper frontend stores JSON options such as `max_trials`, `random_seed`, and `io_node_policy`.
4. The frontend calls either the generic search engine or the direct-grid array engine.
5. The engine returns an `entity::Mapping` with one DFG node assigned to one physical PE. 2D placement does not emit edge routes.

## YOTO/YOTT Implementations

| `Algorithm.type` | frontend | engine path | main use |
| --- | --- | --- | --- |
| `Placement2DYOTOMapper` | `placement2d_yoto_mapper.cpp` | `../common/search/` via `placement2d_search_engine.cpp` | legacy MRRG-facing YOTO experiments |
| `Placement2DYOTTMapper` | `placement2d_yott_mapper.cpp` | `../common/search/` via `placement2d_search_engine.cpp` | legacy MRRG-facing YOTT experiments |
| `Placement2DFaithfulArrayYOTOMapper` | `placement2d_yoto_mapper.cpp` | `engine/placement2d_array_engine_faithful_traversal.cpp` | paper-guided traversal variant with direct-grid data structures |
| `Placement2DFaithfulArrayYOTTMapper` | `placement2d_yott_mapper.cpp` | `engine/placement2d_array_engine_faithful_traversal.cpp` | paper-guided traversal variant with YOTT annotations/ranking |
| `Placement2DCPUMappingYOTOMapper` | `placement2d_yoto_mapper.cpp` | `engine/placement2d_array_engine_cpu_mapping.cpp` | public `cpu_mapping`-style YOTO port |
| `Placement2DCPUMappingYOTTMapper` | `placement2d_yott_mapper.cpp` | `engine/placement2d_array_engine_cpu_mapping.cpp` | public `cpu_mapping`-style YOTT port |

## Implementation Areas

- `engine/`: direct 2D-grid implementation used by paper-guided YOTO/YOTT, cpu_mapping YOTO/YOTT, array PRISA, and internal SA.
- `../common/search/`: generic placement/search implementation used by MRRG-facing 2D and modulo mappers.

## Reading Path

1. Start from the mapper family file (`placement2d_yoto_mapper.cpp`, `placement2d_yott_mapper.cpp`, `placement2d_prisa_mapper.cpp`, etc.).
2. Read the execution-path comment at the top of that file.
3. If the mapper uses `Placement2DArrayMapperBase`, continue in the named `engine/placement2d_array_engine_*.cpp` file.
4. If the mapper uses `placement2d_search_engine.cpp`, continue in `../common/search/`.
5. In each engine file, start from the pipeline comment at the top, then edit the function named by the relevant ablation handle.

## YOTO/YOTT Placement

- YOTT mapper configs: `research/configs/mapper/placement2d/paper_comparison/`.
- Paper-guided array traversal: `engine/placement2d_array_engine_faithful_traversal.cpp`.
- cpu_mapping-compatible traversal: `engine/placement2d_array_engine_cpu_mapping.cpp`.
- Direct-grid objective and metrics: `engine/placement2d_array_engine_quality.cpp`.
- DFG/grid compatibility, I/O policy, and distance cache: `engine/placement2d_array_engine_cache.cpp`.
- Run config: `research/configs/experiments/placement2d/paper_comparison/yott_cases2021_best_vpr.json`.

## Ablation Edit Points

| question | edit here | typical config knobs |
| --- | --- | --- |
| Edge/root traversal order | `engine/placement2d_array_engine_faithful_traversal.cpp`, `BuildFaithfulTraversalPlan()` or `engine/placement2d_array_engine_cpu_mapping.cpp`, `BuildCPUMappingPlan()` | `Algorithm.type`, `random_seed` |
| Neighbor chosen during traversal | `ChooseFaithfulNeighbor()` or `ChooseCPUMappingZigZagNeighbor()` | `Algorithm.type` |
| Candidate PE list around an anchor | `ClosestCompatibleCells()`, `CPUMappingTipCells()`, `TryCPUMappingAdjacency()` | architecture `network_type`, grid size |
| Candidate ranking/scoring | `FaithfulTraversalRank()`, `BestCPUMappingDegreeCell()`, `PlacementCost()` | `cpu_mapping_bug_compatible_degree` |
| YOTT-specific annotations | `BackpropagateFaithfulAnnotation()`, `ApplyCPUMappingCycleAnnotations()` | `Algorithm.type` |
| Input/output node treatment | `IsIONode()`, `CPUMappingCellType()`, `CanPlace()` in `engine/placement2d_array_engine_cache.cpp` | `io_node_policy`, architecture memory cells |
| Trial count and seed behavior | `RunFaithfulTraversalMultiStart()`, `RunCPUMappingMultiStart()`, `SeedFor()` | `max_trials`, `seed_count`, `random_seed` |
| Final objective used to keep the best trial | `PlacementCost()`, `ComputePlacementQuality()`, `IsBetterQuality()` | none unless a new option is added |

## Adding A Mapper

1. Add a public header under `mapper/include/mapper/placement2d/`.
2. Add a mapper frontend in this directory.
3. Register a unique `Algorithm.type`.
4. Add a mapper config under `research/configs/mapper/placement2d/`.
5. Add the config to an experiment manifest under `research/configs/experiments/placement2d/`.

`research/scripts/preflight_manifest.py` checks that the DFG node count fits the physical PE count before a 2D placement run.
