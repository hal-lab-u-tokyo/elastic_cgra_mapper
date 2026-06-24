# Generic Placement Search Engine

MRRG-resource search engine shared by MRRG-facing 2D placement mappers and modulo placement/routing mappers.

## Files

- `placement_search_engine.cpp`: run flow, placement-only versus routed mapping, and public wrapper.
- `placement_search_engine_internal.hpp`: private state, helper structs, and method declarations.
- `placement_search_engine_options.cpp`: mapper kind, options, seeds, timeouts, and logging.
- `placement_search_engine_resources.cpp`: MRRG resource cache, operation compatibility, memory access, and resource distance.
- `placement_search_engine_quality.cpp`: placement cost, candidate ranking, random legal placement, and quality helpers.
- `placement_search_engine_traversal.cpp`: YOTO/YOTT-style traversal placement and physical-then-context placement.
- `placement_search_engine_routing.cpp`: BFS/Manhattan routing, route validation, and mapping route generation.
- `placement_search_engine_prisa.cpp`: PRISA-style placement on MRRG resources.
- `placement_search_engine_sa.cpp`: SA-style placement on MRRG resources.

## Edit Points

- Placement order and candidate rules: `placement_search_engine_traversal.cpp`, `placement_search_engine_prisa.cpp`, or `placement_search_engine_sa.cpp`.
- Routing behavior and validation: `placement_search_engine_routing.cpp`.
- Placement objective and quality metrics: `placement_search_engine_quality.cpp`.
- Resource filtering and architecture assumptions: `placement_search_engine_resources.cpp`.
- Config-facing options: `placement_search_engine_options.cpp` plus `mapper/include/mapper/detail/placement_search_engine.hpp`.
