# Mapper

`mapper::CreateMapper(Algorithm.type)` creates an algorithm, followed by
`Configure()` and `Execution()`. The registry and factory remain the common API.

## Layout

Public headers live in `mapper/include/mapper/<algorithm>/`; implementations
live in `mapper/src/<algorithm>/`. Algorithms are `sa`, `prisa`, `yoto`, `yott`,
`yott_core`, `yott_core_repair`, `ilp`, `connectivity_path_ilp`,
and `placement_first`. Headers, including backend declarations, live under
`include/mapper/<algorithm>/`; no `detail/` dispatcher or cross-algorithm
mapper base remains. Each concrete frontend implements `IMapper` directly.

Each algorithm's `<algorithm>_mapper.cpp` owns the execution workflow.
SA, PRISA, YOTO and YOTT each have one concrete engine declared in
`include/mapper/<algorithm>/engine/<algorithm>_engine.hpp`. Each backend has
three implementation files: `<algorithm>_engine.cpp` for setup, caches and
shared operations; `<algorithm>_placement.cpp` for search and placement costs;
and `<algorithm>_routing.cpp` for route search and reservation. There are no
array/spatial/context engine subfolders, CRTP engine bases, or template mapper
gateways for these four algorithms. Their frontends call placement and routing
operations directly, including the placement-only result path.
`placement_first/placement_first_mapper.cpp` similarly orders placement,
routing, and output. Core and core-repair each expose one concrete engine in
`engine/<algorithm>_engine.hpp`, with setup/cache/trace operations in
`<algorithm>_engine.cpp` and placement search in `<algorithm>_placement.cpp`.
Core-repair additionally keeps its repair passes in `yott_core_repair_refinement.cpp`.
Their mapper frontends call the engine directly without template gateways.

ILP lives entirely in `ilp/`. `ilp_mapper.cpp` retains the full-routing
formulation in one `ILPMapper::Execution` body; `ilp_placement_mapper.cpp`
retains operation placement in `PlacementILPMapper::Execution`, including
multi-context CGRAs.
Variable creation, objective, constraints, optimization and result collection
follow the original source order without separate engine classes or a workflow
template. `ILPMapper` always executes the routing formulation regardless of
context count; operation placement is explicitly selected with `PlacementILPMapper`.
The routing model uses the original producer-level `map_op_to_route` and
`map_route_op_to_PE` variables, constraints and result extraction. It does not
add per-DFG-edge flow variables. `connectivity_path_ilp_mapper.cpp` retains its
separate formulation unchanged.

### Variant policies within one engine

Grid placement retains cell coordinates, grid compatibility/cost policies and
empty-route output. Routed placement retains MRRG-node assignments and route
ownership. Both coordinate representations belong to the algorithm's one
placement state; neither is silently substituted for the other. Seed/budget,
timeout, logging and critical-path operations are shared where identical.
Per-trial seed handling, candidate order and existing counters are preserved.

One-context spatial and multi-context placement use the same concrete engine.
The generic algorithm name chooses a strategy from the MRRG, while explicit
legacy variants retain their original behavior, including explicit spatial
strategies on multi-context input. Context-aware policies handle physical PE
exclusivity, distance caching and assignment to time-expanded resources.

- PRISA retains SIS/no-SIS and cost-aware placement/refinement, including
  physical placement followed by context assignment and optional Manhattan
  routing. Grid and MRRG cost-aware metrics remain distinct types because
  they represent different coordinate domains.
- YOTO and YOTT retain physical-to-context assignment and modulo fallback
  candidate policies. YOTT retains its reconvergence annotations and ranking;
  YOTO does not acquire those behaviors through consolidation.
- YOTT core and core-repair remain separate algorithms with their original
  compact objectives and repair passes; this does not fold them into YOTT.
- Spatial ILP and full-routing ILP retain separate mathematical formulations
  within the ILP algorithm. Each formulation retains its original monolithic
  model-building order for comparison with the original implementations.
  Connectivity-path ILP remains a separate formulation.

### Header names

`grid_<algorithm>_mapper.hpp` declares grid-coordinate placement variants;
`routed_<algorithm>_mapper.hpp` declares MRRG placement and routing variants.
A routed variant can use one or multiple contexts, so headers no longer label
that distinction as placement versus modulo. Existing registry/class names
remain compatible with saved experiment configurations.

## MRRG selection

Use `SA`, `PRISA`, `YOTO`, or `YOTT` (also `<name>Mapper`) as
`Algorithm.type`. The corresponding algorithm's `<algorithm>_mapper.cpp` reads the input MRRG:

- `context_size == 1`: its spatial placement strategy.
- `context_size > 1`: its context-aware strategy (physical/context assignment for PRISA).

Example: `{"Algorithm": {"type": "YOTT", "random_seed": 42}}` works with
either a single-context array or a multi-context MRRG. `placement_only` remains
an output/evaluation option; it does not choose the implementation.

Existing `Placement2D*`, `Modulo*`, paper, CPU, fallback, and `FullRoutingILPMapper` names remain
explicit variant compatibility aliases for old experiment configurations.
The full-routing ILP alias uses the original producer-level model; other explicit
variants retain their behavior. The generic `SA`, `SAMapper`, `YOTO`, `YOTOMapper`,
`YOTT`, and `YOTTMapper` names now select from the MRRG instead of implicitly
selecting the context-aware variant. `ILP`, `ILPMapper`, `GurobiILPMapper` and
`FullRoutingILPMapper` all construct the same routing ILP regardless of context
count. Operation placement is a separate formulation that also supports multiple
contexts; context count does not select between these two ILP formulations.
The redundant full-routing C++ wrapper class/header have been removed. C++ callers
can use `ILPMapper(dfg, mrrg)` directly.
`PlacementILPMapper` is the operation-placement class and registry name.
`Placement2DILPMapper` remains only a saved-configuration alias.
The placement implementation is `ilp_placement_mapper.cpp` with public header
`<mapper/ilp/ilp_placement_mapper.hpp>`. The connectivity-path formulation lives
beside it under `ilp/`; its class/configuration names and pipeline are unchanged.
Public include paths have moved to the
algorithm directories; include, for example, `<mapper/yott/yott_mapper.hpp>`.

To add an algorithm, add its public class and implementation under matching
algorithm directories, register its name in `src/mapper_registry.cpp`, then
validate the supported MRRGs. CMake discovers `.cpp` files recursively.

## Entity integration

PR140's reviewed entity configuration and MRRG model remain authoritative.
`mapper/configuration.hpp` extends `entity::AlgorithmConfig` with registry names
and optional search settings, rather than adding algorithm fields to entity.
MRRG uses `kOneHop2d` / `one_hop_2d`; legacy architecture JSON spellings are
translated by IO, and new mappings always serialize `one_hop_2d`. The research
metric `placement_cost_model=one_hop_axis2` keeps its mathematical definition.

Operation enums follow Google style (`OpType::kAdd`, `kLoad`, etc.), without
uppercase aliases. Numeric values and JSON/opcode strings are unchanged.
