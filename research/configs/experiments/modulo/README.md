# Modulo Experiment Manifests

Use `search.json` while designing modulo mapping algorithms. It compares:

- placement-first methods crossed with CGRA BFS routing retry policies
- routing-aware placement methods
- external VPR placement followed by CGRA routing

YOTO/YOTT entries are intentionally split:

- `*_physical_context`: 2D physical placement first, ASAP context assignment second, CGRA BFS routing last.
- `*_direct_context`: direct placement on MRRG resources that already include modulo contexts.

`modulo_prisa_physical_context` adapts PRISA to modulo mapping as 2D physical placement, ASAP context assignment, and the same CGRA BFS routing used for YOTO/YOTT. `modulo_prisa_physical_context__manhattan`, available in broader reference manifests, mirrors PRISA's placement-then-Manhattan-routing assumption, but it is not a literal PRISA paper setting because the PRISA paper does not solve modulo scheduling.

Use `all_mappers.json` for broader reference runs. It adds ILP and VPR full-route baselines, so it is slower.

## Mapper Groups

`mapper_matrix` is for placement-first methods. Each `placements` entry is crossed with each `routers` entry. Router settings are applied as `Algorithm` overrides in a generated mapper config under the result directory.

```json
"mapper_matrix": {
  "placements": [
    {
      "name": "modulo_yoto_physical_context",
      "mapper_config": ".../modulo_physical_yoto_mapper.json",
      "placement_method": "yoto_physical_context"
    }
  ],
  "routers": [
    {
      "name": "cgra_bfs_retry6",
      "routing_method": "cgra_bfs",
      "algorithm_overrides": {
        "routing_retry_count": 6
      }
    }
  ]
}
```

`routing_aware_mappers` is for methods that choose placement while considering routeability. Add new integrated algorithms here.

`external_mappers` is for baselines that are not normal `build/mapping` C++ mapper configs, such as VPR.

## Adding A New Method

For a placement-first method:

1. Add a mapper config under `research/configs/mapper/modulo/`.
2. Add one entry to `mapper_matrix.placements`.
3. Keep the existing `routers` list unchanged while comparing placement algorithms.

For a new routing method:

1. Add one entry to `mapper_matrix.routers`.
2. Put method-specific runtime knobs in `algorithm_overrides`.
3. Compare with `summary_by_routing_method.md`.

For a routing-aware placement method:

1. Add the C++ mapper under `mapper/src/modulo/`.
2. Add a mapper config under `research/configs/mapper/modulo/`.
3. Add one entry to `routing_aware_mappers`.

Results include `placement_method`, `routing_method`, and `mapper_role` columns in `metrics.csv`, plus summaries by those axes when reports are generated.
