# Mapper

`mapper::CreateMapper(Algorithm.type)` creates an algorithm, followed by
`Configure()` and `Execution()`. The registry and factory remain the common API.

## Layout

Public headers live in `mapper/include/mapper/<algorithm>/`; implementations
live in `mapper/src/<algorithm>/`. Algorithms are `sa`, `prisa`, `yoto`, `yott`,
`yott_core`, `yott_core_repair`, `ilp`, `connectivity_path_ilp`,
`full_routing_ilp`, and `placement_first`. `detail/` contains shared option
forwarding and compatibility adapters, without optimization logic.

Each algorithm owns its objectives, state, moves, and engine construction.
`array/` retains direct-grid paper/CPU variants, `spatial/` maps physical PEs,
and `context/` assigns contexts and routes. These are private implementations
inside an algorithm, rather than separate public mapping domains.

## MRRG selection

Use `SA`, `PRISA`, `YOTO`, or `YOTT` (also `<name>Mapper`) as `Algorithm.type`.
The corresponding algorithm's `search.cpp` reads the input MRRG:

- `context_size == 1`: its spatial engine.
- `context_size > 1`: its context-aware engine (physical/context assignment for PRISA).

Example: `{"Algorithm": {"type": "YOTT", "random_seed": 42}}` works with
either a single-context array or a multi-context MRRG. `placement_only` remains
an output/evaluation option; it does not choose the implementation.

Existing `Placement2D*`, `Modulo*`, paper, CPU, fallback, and ILP names remain
explicit variant compatibility aliases for old experiment configurations.
Their behavior is preserved. The generic `SA`, `SAMapper`, `YOTO`, `YOTOMapper`,
`YOTT`, and `YOTTMapper` names now select from the MRRG instead of implicitly
selecting the context-aware variant. Public include paths have moved to the
algorithm directories; include, for example, `<mapper/yott/yott_mapper.hpp>`.

To add an algorithm, add its public class and implementation under matching
algorithm directories, register its name in `src/mapper_registry.cpp`, then
validate the supported MRRGs. CMake discovers `.cpp` files recursively.
