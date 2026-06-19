# Experiment Manifests

Use the four manifests below for normal experiments.

| problem | purpose | manifest |
| --- | --- | --- |
| modulo placement + routing | heuristic search | `modulo/search.json` |
| modulo placement + routing | broad comparison with ILP/VPR baselines | `modulo/all_mappers.json` |
| 2D placement only | heuristic search | `placement2d/search.json` |
| 2D placement only | broad comparison with placement-only ILP/VPR baselines | `placement2d/all_mappers.json` |

Use `placement2d/reproduction/` only when checking prior-work reproduction.

| paper setting | manifest |
| --- | --- |
| TRAVERSAL/YOTT-style LISA and m_bench placement quality | `placement2d/reproduction/traversal_yott.json` |
| PRISA VPR-8 placement quality | `placement2d/reproduction/prisa_vpr8.json` |

Use `placement2d/probes/` for convergence checks, ablations, and one-off diagnostics. Use `placement2d/archive/` only to inspect older exploratory manifests; do not start new experiments from those files.

## Problem Types

- `problem_type: "placement2d"` fixes `II = 1`, places each operation on one physical PE, and normally uses `evaluation_mode: "placement_only"`.
- `problem_type: "modulo"` sweeps II from MII to `ii_max`, places operations on `(PE, context)`, and requires routed connectivity unless a manifest explicitly says otherwise.
- `runner: "vpr"` is an external VPR simulated-annealing placement baseline for 2D placement-only experiments.
- `runner: "vpr_modulo"` places with VPR SA, then assigns contexts and routes with this repository's CGRA router.
- `runner: "vpr_modulo_full_route"` uses a generated CGRA RR graph and VPR routing; keep it in broad comparison manifests, not search manifests.
- Modulo manifests can use `mapper_matrix` to cross placement-first mapper configs with routing policies. Generated results include `placement_method`, `routing_method`, and `mapper_role` columns.

## Before Running

1. Choose one of the four normal manifests unless reproducing a paper.
2. Add a new mapper entry to the selected manifest. For modulo placement-first methods, add it to `mapper_matrix.placements`; for routing-aware methods, add it to `routing_aware_mappers`.
3. Keep benchmark, architecture, timeout, and metric settings unchanged while comparing mappers.
4. Run `research/scripts/preflight_manifest.py`.
5. Run `research/scripts/run_suite.py --manifest <manifest>`.

Fallback-based mapper configs are named `*_with_fallback_mapper.json` and are for diagnosis, not default comparisons.
