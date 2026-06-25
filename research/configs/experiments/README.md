# Experiment Manifests

Experiment manifests define benchmark sets, architecture settings, mapper entries, timeouts, and output groups for `research/scripts/run_suite.py`.

## Standard Manifests

| setting | manifest |
| --- | --- |
| Modulo mapper search | `modulo/search.json` |
| Modulo comparison with ILP and VPR | `modulo/all_mappers.json` |
| 2D placement search | `placement2d/search.json` |
| 2D placement comparison with ILP | `placement2d/all_mappers.json` |

Use `placement2d/paper_comparison/` for prior-work paper comparison:

| setting | manifest |
| --- | --- |
| TRAVERSAL/YOTT paper comparison | `placement2d/paper_comparison/traversal_yott.json` |
| YOTT 2021 paper benchmark comparison | `placement2d/paper_comparison/yott_cases2021.json` |
| PRISA paper comparison | `placement2d/paper_comparison/prisa_vpr8.json` |

Use `placement2d/probes/` for diagnostics and ablations. Use `placement2d/archive/` only to inspect older manifests.

## Problem Types

- `problem_type: "placement2d"` fixes II to 1 and evaluates physical PE placement.
- `problem_type: "modulo"` sweeps II from MII to `ii_max` and validates routed connectivity.
- Modulo manifests can use `mapper_matrix` to cross placement-first mapper configs with routing policies.

## Runner Styles

Mapper entries use one of these styles:

- no `runner`: run an in-repository C++ mapper through `build/mapping`.
- `runner: "vpr"`: run VPR simulated-annealing placement for 2D placement.
- `runner: "vpr_modulo"`: run VPR placement, assign contexts, then route with this repository's CGRA router.
- `runner: "vpr_modulo_full_route"`: run generated CGRA routing resources through VPR route. Keep this in comparison manifests because it is slower.

`run_suite.py` expands the manifest and delegates every case to `research/scripts/run_mapper_case.py`, so C++ mappers and VPR entries share the same output layout and reports.

## Adding a Mapper Entry

1. Add a mapper preset under `research/configs/mapper/`.
2. Add it to one manifest.
3. Keep benchmark, architecture, timeout, and metric settings fixed while comparing mappers.
4. Run `research/scripts/preflight_manifest.py`.
5. Run `research/scripts/run_suite.py --manifest <manifest>`.

Fallback configs are named `*_with_fallback_mapper.json`. Use them when studying mixed strategies, not for clean comparisons.
