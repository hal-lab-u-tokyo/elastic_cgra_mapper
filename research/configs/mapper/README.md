# Mapper Configs

Mapper configs are JSON presets for C++ mapper implementations. Implementations live under `mapper/src/placement2d/` and `mapper/src/modulo/`.

## Modulo

Use these with `research/configs/experiments/modulo/search.json`:

- `modulo/modulo_yoto_mapper.json`
- `modulo/modulo_yott_mapper.json`
- `modulo/modulo_sa_mapper.json`
- `modulo/modulo_placement_first_mapper.json`
- `modulo/modulo_prisa_mapper.json`

Use these with `modulo/all_mappers.json`:

- `modulo/full_routing_ilp_mapper.json`
- `modulo/connectivity_path_ilp_mapper.json`
- `modulo/modulo_prisa_manhattan_mapper.json`

`modulo/modulo_*_with_fallback_mapper.json` files combine placement-first search with recovery logic. Use them only when evaluating fallback strategies.

## 2D Placement

Use these with `research/configs/experiments/placement2d/search.json`:

- `placement2d/placement2d_cpu_mapping_yoto_1000_trials_mapper.json`
- `placement2d/placement2d_cpu_mapping_yott_1000_trials_mapper.json`
- `placement2d/placement2d_array_prisa_mapper.json`
- `placement2d/placement2d_array_prisa_no_sis_mapper.json`

Use this with `placement2d/all_mappers.json` or small focused runs:

- `placement2d/placement2d_ilp_mapper.json`

## Paper Comparison

Use these with `placement2d/paper_comparison/*` manifests:

- `placement2d/paper_comparison/placement2d_yoto_*`
- `placement2d/paper_comparison/placement2d_yott_*`
- `placement2d/paper_comparison/placement2d_faithful_array_yoto_*`
- `placement2d/paper_comparison/placement2d_faithful_array_yott_*`
- `placement2d/paper_comparison/placement2d_array_prisa_mapper.json`
- `placement2d/paper_comparison/placement2d_array_prisa_no_sis_mapper.json`

The `placement2d_cpu_mapping_*` configs port the released `cpu_mapping` YOTO/YOTT placement kernels. The YOTT configs default to compatibility mode for the released `__NEIGHBOURHOOD >= 4` degree-selection behavior. Set `Algorithm.cpu_mapping_bug_compatible_degree` to `false` for the corrected degree comparison.

`placement2d/probes/` contains derived PRISA variants and convergence settings. `placement2d/archive/` contains old in-repository SA configs.

## Add a Config

Create one preset with conservative defaults, then add it to exactly one manifest before broadening the comparison. For modulo placement-first methods, add the config to `mapper_matrix.placements`; for routing-aware methods, add it to `routing_aware_mappers`; for 2D placement methods, add it to `mappers`.
