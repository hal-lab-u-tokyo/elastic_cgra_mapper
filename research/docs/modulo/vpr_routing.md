# VPR Modulo Routing Baselines

The modulo manifests contain two VPR-based routed baselines.

`vpr_sa_routed` runs VPR simulated-annealing placement, imports the placement, assigns modulo contexts, and routes with this repository's CGRA MRRG router.

`vpr_sa_full_route` runs VPR placement and VPR routing. With the default `modulo_placement_mode: "vpr_context"`, each candidate II is represented as a VPR grid with one site per PE/context slot, so VPR chooses both physical PE and modulo context. The runner then generates a VPR RR graph that represents the CGRA modulo routing graph, routes with VPR, imports the `.route` file, and emits a CGRA mapping only when the imported route can be represented by legal PE/context `route` configs.

For diagnostics, `modulo_placement_mode: "physical_then_context"` keeps the older flow: VPR chooses only the physical PE grid, this repository generates context assignments, and VPR routes the resulting fixed PE/context placement. This mode is useful for separating placement quality from context-assignment quality, but it is not the default comparison point.

The generated RR graph keeps one routable node for each unused PE/context and connects nodes according to the same CGRA modulo adjacency used by native mappers. Because VPR's route checker also expects geometrically adjacent CHANX/CHANY traceback nodes, long context jumps in the flattened VPR coordinate system are represented with internal bridge nodes. These bridge nodes are VPR-only; route import ignores them and validates the resulting path against the CGRA MRRG.

The two VPR modulo baselines are intentionally different. `vpr_sa_routed` measures VPR SA placement followed by this repository's router. `vpr_sa_full_route` measures VPR SA placement plus VPR routing over the generated CGRA RR graph. Both outputs must pass the same `routing_validation.md` checks before they are treated as routed mappings.

Typical command for the strict VPR-routing baseline:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/modulo/with_ilp.json \
  --only-mapper vpr_sa_full_route
```

Always inspect `routing_validation.md`. A successful imported route must pass the same legal-MRRG and DFG-edge reachability checks as native mappers.
