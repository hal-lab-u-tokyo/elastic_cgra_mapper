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

Each algorithm's `<algorithm>_mapper.cpp` owns the readable execution workflow. Private
`engine/` classes provide search, resource/cost, routing, and result operations.
The six stochastic algorithms combine their former array dispatch and routed
dispatch in one frontend, sharing the workflow across explicit variants. Engines
no longer expose a complete `Run()` workflow. Spatial and context engines use
the same routed frontend; array variants share their placement-only frontend.
`placement_first/placement_first_mapper.cpp` similarly orders placement, routing, and output.

ILP lives entirely in `ilp/`: `ilp_mapper.cpp` selects the formulation and
`include/mapper/ilp/ilp_workflow.hpp` orders variable creation, objective, constraints, optimization,
and result collection. `engine/` owns the Gurobi model and variables.
`connectivity_path_ilp` is deliberately unchanged pending a separate decision.

### Why private backend variants remain

- Array engines store `dfg_to_cell` / `cell_to_dfg`, use grid compatibility
  caches and paper/CPU candidate policies, and produce a placement with empty
  routes. Routed engines store MRRG-node assignments and route ownership;
  treating these as one state would change candidate order and output semantics.
- SA spatial placement reserves physical positions/context zero, while its
  context engine selects time-expanded MRRG resources and permits temporal
  reuse. Their random moves and legal-resource sets differ.
- PRISA's context backend adds physical placement followed by context
  assignment and routing; the spatial backend's refinement and candidate
  resources differ. The array variant has its own SIS/grid objective.
- YOTO and YOTT context backends support physical-to-context assignment and
  modulo fallback policies; spatial backends enforce physical exclusivity.
  Their `CanOccupyResource`, `ConstructTraversalPlacement`, context assignment,
  and routing legality differ. YOTT also retains annotated ranking; YOTO does
  not gain those semantics through consolidation.
- YOTT core and core-repair are already single array backends with distinct
  core objectives and repair passes. They each now have one mapper frontend.
- Spatial ILP uses operation/route-node constraints; full-routing ILP uses
  per-DFG-edge flow variables plus producer-level route ownership. Combining
  those mathematical formulations would change the feasible set. They share
  solver orchestration, but keep separate constraint builders.

These are semantic boundaries, not separate public algorithms. Keeping private
`engine/{array,spatial,context}/` implementations preserves traversal/RNG order
and the existing experiment contracts while eliminating duplicated frontend
workflows. Identical spatial/context operations live in each algorithm’s
`include/mapper/<algorithm>/engine/<algorithm>_engine_operations.hpp`. These
algorithm-owned CRTP operation classes share cache-independent search, scoring,
resource queries, traversal and routing helpers while using concrete engine
state and legality rules. SA shares 39 methods, PRISA 44, YOTO 38 and YOTT 38.
Per-algorithm timing/operation-compatibility/distance helpers are shared too;
YOTO/YOTT array and routed engines use the same degree penalty. No helper
selects among different algorithms. Domain-specific state, legal-resource
checks, candidate ordering and objectives remain in the domain engines.

## MRRG selection

Use `SA`, `PRISA`, `YOTO`, `YOTT`, or `ILP` (also `<name>Mapper`) as
`Algorithm.type`. The corresponding algorithm's `<algorithm>_mapper.cpp` reads the input MRRG:

- `context_size == 1`: its spatial engine (spatial formulation for ILP).
- `context_size > 1`: its context-aware engine (physical/context assignment for PRISA,
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
