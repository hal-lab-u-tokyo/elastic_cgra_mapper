# 2D Placement Mappers

Mappers in this directory solve the 2D placement problem. One physical PE is one placement slot, so the runner fixes II/context size to 1.

Implementation files are grouped by method:

- `placement2d_yoto_mapper.cpp`: YOTO shared-engine mapper, array YOTO, and cpu_mapping-style YOTO.
- `placement2d_yott_mapper.cpp`: YOTT shared-engine mapper, array YOTT, and cpu_mapping-style YOTT.
- `placement2d_prisa_mapper.cpp`: PRISA, PRISA without SIS, cost-aware PRISA, and their array variants.
- `placement2d_sa_mapper.cpp`: internal SA baselines, including the array SA archive mapper.
- `placement_only_ilp_mapper.cpp`: placement-only Gurobi ILP baseline.
- `placement2d_array_engine.cpp`: common direct-grid engine used by the array/cpu_mapping variants. It is a shared implementation detail, not a mapper family.
- `placement2d_array_mapper_base.cpp`: common wrapper for array/cpu_mapping mapper classes.
- `placement2d_search_engine.cpp`: bridge from 2D mapper names to the shared search engine in `mapper/src/common/`.

Recommended reading order:

1. Start with this file to identify the problem setting and mapper family.
2. Open the method file you care about, such as `placement2d_yoto_mapper.cpp` or `placement2d_prisa_mapper.cpp`. These files show the registered `Algorithm.type` names and the search kind passed to the implementation.
3. For `Placement2D*` mappers, read `placement2d_search_engine.cpp` and then `mapper/src/common/placement_search_engine.cpp`.
4. For `Placement2DArray*` and `Placement2DCPUMapping*` mappers, read `placement2d_array_engine.cpp`. It is organized as: input/cache setup, shared placement objectives, YOTO/YOTT traversal, cpu_mapping-style traversal, PRISA/SIS/move selection, and SA.
5. Treat `placement2d_array_mapper_base.cpp` as plumbing. It only forwards config values into the direct-grid engine.

Several implementation styles are available:

- `Placement2D*`: uses the shared placement engine and MRRG-facing helper code.
- `Placement2DArray*`: uses a direct 2D grid array fast path for YOTO/YOTT/SA-style placement. This keeps the same output format and metrics, but removes general MRRG search overhead when the problem is strictly 2D placement.
- `Placement2DCPUMappingYOTOMapper` / `Placement2DCPUMappingYOTTMapper`: direct 2D-grid YOTO/YOTT variants for PRISA comparisons. These keep the existing array mappers intact, but use a local-neighborhood placement kernel closer to the public `cpu_mapping` code path and are intended for 1000-trial runtime comparisons.
- `Placement2DPRISAMapper` / `Placement2DPRISANoSISMapper`: PRISA-style potential-region guided placement, with and without SIS initial DFG labeling.
- `Placement2DCostAwarePRISAMapper`: PRISA with SIS, followed by cost-aware refinement after WR is cleared. It directly optimizes placement-only mesh hop, mapped LP, and max FIFO distance, and is a comparison variant rather than the paper-faithful PRISA reproduction.
- `PlacementOnlyILPMapper`: solves the placement-only objective exactly with Gurobi and is the ILP baseline for placement-quality comparisons.

To add a mapper:

1. Add `mapper/include/mapper/placement2d/<name>.hpp`.
2. Add `mapper/src/placement2d/<name>.cpp`.
3. Register an `Algorithm.type` such as `Placement2DMyMapper`.
4. Add `research/configs/mapper/placement2d/placement2d_my_mapper.json`.
5. Add the config to `research/configs/experiments/placement2d/search.json`.

`preflight_manifest.py` checks that DFG node count is not larger than physical PE count.

For normal 2D placement experiments, use `research/configs/experiments/placement2d/search.json` or `research/configs/experiments/placement2d/all_mappers.json`. For TRAVERSAL/YOTT-style reproduction, use `research/configs/experiments/placement2d/reproduction/traversal_yott.json`. For PRISA VPR-8 reproduction, use `research/configs/experiments/placement2d/reproduction/prisa_vpr8.json`; this config uses the `Placement2DCPUMappingYOTO/YOTT` 1000-trial mappers rather than the generic array fast path.
