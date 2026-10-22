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
routing, and output. Core and core-repair retain their own single backends.

ILP lives entirely in `ilp/`: `ilp_mapper.cpp` selects the formulation and
`include/mapper/ilp/ilp_workflow.hpp` orders variable creation, objective, constraints, optimization,
and result collection. The engine classes own the Gurobi model and variables.
The implementation units under `src/ilp/` preserve the Gurobi source lineage:
`ilp_mapper.cpp` contains `FullRoutingILPEngine` and the canonical ILP frontend;
`placement2d_ilp_mapper.cpp` contains `SpatialILPEngine` and its explicit frontend.
They are renamed from `gurobi_mapper.cpp` and `gurobi_placement_mapper.cpp`,
respectively. Engine declarations remain under `include/mapper/ilp/engine/`.
`connectivity_path_ilp` is deliberately unchanged pending a separate decision.

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
  within the ILP algorithm. They share solver orchestration and keep distinct
  constraint builders. Connectivity-path ILP remains deferred.

## MRRG selection

Use `SA`, `PRISA`, `YOTO`, `YOTT`, or `ILP` (also `<name>Mapper`) as
`Algorithm.type`. The corresponding algorithm's `<algorithm>_mapper.cpp` reads the input MRRG:

- `context_size == 1`: its spatial placement strategy (spatial formulation for ILP).
- `context_size > 1`: its context-aware strategy (physical/context assignment for PRISA,
  full per-edge routing formulation for ILP).

Example: `{"Algorithm": {"type": "YOTT", "random_seed": 42}}` works with
either a single-context array or a multi-context MRRG. `placement_only` remains
an output/evaluation option; it does not choose the implementation.

Existing `Placement2D*`, `Modulo*`, paper, CPU, fallback, and `FullRoutingILPMapper` names remain
explicit variant compatibility aliases for old experiment configurations.
Their behavior is preserved. The generic `SA`, `SAMapper`, `YOTO`, `YOTOMapper`,
`YOTT`, and `YOTTMapper` names now select from the MRRG instead of implicitly
selecting the context-aware variant. `ILPMapper` and `GurobiILPMapper` likewise
select the spatial ILP formulation for a single context, retaining full routing
for multiple contexts; use `FullRoutingILPMapper` to force its prior behavior.
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
