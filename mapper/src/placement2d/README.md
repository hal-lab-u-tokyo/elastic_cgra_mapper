# 2D Placement Mappers

Mappers in this directory solve the 2D placement problem. One physical PE is one placement slot, so the runner fixes II/context size to 1.

Two implementation styles are available:

- `Placement2D*`: uses the shared placement engine and MRRG-facing helper code.
- `Placement2DArray*`: uses a direct 2D grid array fast path for YOTO/YOTT/SA-style placement. This keeps the same output format and metrics, but removes general MRRG search overhead when the problem is strictly 2D placement.
- `PlacementOnlyILPMapper`: solves the placement-only objective exactly with Gurobi and is the ILP baseline for placement-quality comparisons.

To add a mapper:

1. Add `mapper/include/mapper/placement2d/<name>.hpp`.
2. Add `mapper/src/placement2d/<name>.cpp`.
3. Register an `Algorithm.type` such as `Placement2DMyMapper`.
4. Add `research/configs/mapper/placement2d_my_mapper.json`.
5. Add the config to `research/configs/experiments/placement2d/search.json`.

`preflight_manifest.py` checks that DFG node count is not larger than physical PE count.

For TRAVERSAL/YOTT-style placement-quality experiments, use `research/configs/experiments/placement2d/traversal_yott_placement_quality.json`. It compares shared-engine mappers, array fast-path mappers, placement-only ILP, and VPR under the same placement-only metrics.
