# Modulo Mapping

Assigns DFG nodes to PE contexts and constructs or validates routes.

## Implementations

| family | source |
| --- | --- |
| YOTO/YOTT direct MRRG | `modulo_yoto_mapper.cpp`, `modulo_yott_mapper.cpp` |
| physical placement then context assignment | `modulo_yoto_mapper.cpp`, `modulo_yott_mapper.cpp`, `modulo_prisa_mapper.cpp` |
| placement-first routing | `modulo_placement_first_mapper.cpp` |
| SA | `modulo_sa_mapper.cpp` |
| full-routing ILP | `full_routing_ilp_mapper.cpp` |
| connectivity-path ILP | `connectivity_path_ilp_mapper.cpp` |

Shared traversal, context assignment, and routing are under
`mapper/src/common/search/`. Mapper names and aliases are listed in
`mapper/src/mapper_registry.cpp`.

## Experiment Roles

- `mapper_matrix.placements`: physical placement crossed with routers
- `routing_aware_mappers`: integrated placement and routing
- `external_mappers`: external baselines

The standard placement-first router is implemented in
`mapper/src/common/search/support/routing.cpp`.
