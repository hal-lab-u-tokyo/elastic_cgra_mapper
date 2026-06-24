# Modulo Mappers

Mappers in this directory solve the modulo CGRA mapping problem. Different context slots of the same physical PE may be used by different DFG operations.

ILP-backed modulo mappers live here when they model routed or context-aware modulo mapping:

- `FullRoutingILPMapper`: exact placement and DFG-edge routing flow ILP.
- `ConnectivityPathILPMapper`: ILP over precomputed MRRG connectivity/path candidates.

Traversal-style mappers are split by interpretation:

- `ModuloPhysicalYOTOMapper` / `ModuloPhysicalYOTTMapper`: first place operations on the 2D physical PE grid, then assign ASAP modulo contexts on those physical PEs, then validate routes. Use these to test paper-style placement transferred to modulo routing.
- `ModuloYOTOMapper` / `ModuloYOTTMapper`: direct MRRG/context variants. They choose placement resources with contexts already included, then validate routes.
- `ModuloYOTOWithFallbackMapper` / `ModuloYOTTWithFallbackMapper`: diagnostic hybrids that add routing-aware fallback placements. Do not include them in ordinary comparisons unless mixed strategies are the object of study.

PRISA-style modulo adaptation:

- `ModuloPhysicalPRISAMapper`: runs PRISA as a 2D physical placement algorithm, assigns ASAP modulo contexts, then validates routes with the shared CGRA BFS/maze router. Use this in ordinary modulo placement-first comparisons so PRISA, YOTO, and YOTT share the same downstream router.
- `ModuloPhysicalPRISAManhattanMapper`: runs the same PRISA/context stages, then validates routes with a Manhattan-prioritized legal router. PRISA itself is not a modulo mapper in the paper; this mapper is a paper-assumption probe, not the default modulo search baseline.

To add a mapper:

1. Add `mapper/include/mapper/modulo/<name>.hpp`.
2. Add `mapper/src/modulo/<name>.cpp`.
3. Register an `Algorithm.type` such as `ModuloMyMapper`.
4. Add `research/configs/mapper/modulo/modulo_my_mapper.json`.
5. Add the config to `research/configs/experiments/modulo/search.json`.

Experiment manifests separate three roles:

- placement-first methods go in `mapper_matrix.placements` and are crossed with `mapper_matrix.routers`.
- integrated routing-aware methods go in `routing_aware_mappers`.
- external baselines such as VPR go in `external_mappers`.

Placement-first routers live in `mapper/src/common/search/placement_search_engine_routing.cpp`:

- CGRA BFS/maze-style routing, tuned through `Algorithm` overrides such as `routing_retry_count`.
- Manhattan-prioritized legal routing, currently used by the PRISA modulo adaptation.
