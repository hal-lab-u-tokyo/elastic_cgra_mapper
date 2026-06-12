# Modulo Mappers

Mappers in this directory solve the modulo CGRA mapping problem. Different context slots of the same physical PE may be used by different DFG operations.

ILP-backed modulo mappers live here when they model routed or context-aware modulo mapping:

- `FullRoutingILPMapper`: exact placement and DFG-edge routing flow ILP.
- `ConnectivityPathILPMapper`: ILP over precomputed MRRG connectivity/path candidates.

To add a mapper:

1. Add `mapper/include/mapper/modulo/<name>.hpp`.
2. Add `mapper/src/modulo/<name>.cpp`.
3. Register an `Algorithm.type` such as `ModuloMyMapper`.
4. Add `research/configs/mapper/modulo_my_mapper.json`.
5. Add the config to `research/configs/experiments/modulo/search.json`.
