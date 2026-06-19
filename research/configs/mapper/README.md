# Mapper Configs

The C++ implementations are grouped under `mapper/src/modulo/` and `mapper/src/placement2d/`. The JSON files here are thin run-time presets for those implementations.

## Normal Modulo Configs

Use these with `research/configs/experiments/modulo/search.json`.

- `modulo/modulo_yoto_mapper.json`
- `modulo/modulo_yott_mapper.json`
- `modulo/modulo_sa_mapper.json`
- `modulo/modulo_placement_first_mapper.json`

`modulo/search.json` crosses placement-first configs with routing policies through `mapper_matrix`. Change routing retry counts in the manifest, not by copying mapper config files.

Use these only in `modulo/all_mappers.json` or small reference runs.

- `modulo/full_routing_ilp_mapper.json`
- `modulo/connectivity_path_ilp_mapper.json`
- `placement2d/placement_only_ilp_mapper.json`

The `modulo_*_with_fallback_mapper.json` files are diagnostic variants. They make failures easier to inspect, but they should not be used for clean algorithm comparisons.

## Normal 2D Placement Configs

Use these with `research/configs/experiments/placement2d/search.json`.

- `placement2d/placement2d_cpu_mapping_yoto_1000_trials_mapper.json`
- `placement2d/placement2d_cpu_mapping_yott_1000_trials_mapper.json`
- `placement2d/placement2d_prisa_mapper.json`
- `placement2d/placement2d_prisa_no_sis_mapper.json`

Use `placement2d/placement_only_ilp_mapper.json` only in `placement2d/all_mappers.json` or small reference runs.

## 2D Reproduction and Diagnostic Configs

- `placement2d/reproduction/placement2d_array_yoto_*` and `placement2d/reproduction/placement2d_array_yott_*`: fast 2D-array traversal baselines for implementation studies.
- `placement2d/reproduction/placement2d_yoto_*` and `placement2d/reproduction/placement2d_yott_*`: shared-engine TRAVERSAL/YOTT reproduction presets.
- `placement2d/reproduction/placement2d_array_prisa_mapper.json`: primary array PRISA implementation for PRISA reproduction.
- `placement2d/reproduction/placement2d_array_prisa_no_sis_mapper.json`: array PRISA without SIS.
- `placement2d/probes/placement2d_cost_aware_prisa_mapper.json` and `placement2d/probes/placement2d_array_cost_aware_prisa_mapper.json`: derived PRISA variants for analysis, not paper-faithful baselines.
- `placement2d/probes/placement2d_prisa_*_probe_mapper.json` and `placement2d/probes/placement2d_array_prisa_5000_probe_mapper.json`: iteration/convergence probes.
- `placement2d/archive/placement2d_sa*`: old in-repository SA baselines. Prefer VPR SA for 2D placement comparisons.

When adding a new mapper, create one config with conservative defaults first, then add it to exactly one normal manifest before broadening the comparison. For modulo placement-first methods, add the config to `mapper_matrix.placements`; for routing-aware methods, add it to `routing_aware_mappers`.
