# 2D Placement

Assigns each DFG node to one physical PE. Edge routes are not produced.

## Flow

```text
Algorithm.type
  -> mapper/src/mapper_registry.cpp
  -> mapper class in mapper/include/mapper/placement2d/
  -> search or direct-grid frontend
  -> algorithm implementation
```

## Source Tree

```text
placement2d/
  placement2d_search_mapper_base.cpp   generic-search frontend
  placement2d_array_mapper_base.cpp    direct-grid frontend
  placement2d_search_engine.cpp        shared-search adapter
  placement2d_ilp_mapper.cpp           ILP implementation
  engine/
    array_engine.cpp                   cache, dispatch, result conversion
    array_engine_internal.hpp          shared private interface
    array_engine_types.hpp             placement and traversal data
    support/                            cache, options, quality, traversal
    algorithms/
      yoto_yott/                        YOTO, YOTT, YOTT Core, repair
      prisa/                            initial placement, moves, refinement
      sa/                               simulated annealing
```

## Mapper Families

| family | frontend | implementation |
| --- | --- | --- |
| MRRG YOTO/YOTT/SA/PRISA | `Placement2DSearchMapperBase` | `../common/search/` |
| direct-grid YOTO/YOTT | `Placement2DArrayMapperBase` | `engine/algorithms/yoto_yott/` |
| YOTT Core | `Placement2DArrayMapperBase` | `engine/algorithms/yoto_yott/yott_core.cpp` |
| YOTT Core Repair | `Placement2DArrayMapperBase` | `engine/algorithms/yoto_yott/yott_core_repair*.cpp`, `yott_core_profile_repair.cpp` |
| array PRISA | `Placement2DArrayMapperBase` | `engine/algorithms/prisa/` |
| array SA | `Placement2DArrayMapperBase` | `engine/algorithms/sa/` |
| ILP | `Placement2DILPMapper` | `placement2d_ilp_mapper.cpp` |

Compatibility aliases such as `Placement2DFaithfulArrayYOTT` are visible in
`mapper/src/mapper_registry.cpp`.

- [Direct-grid engine](engine/README.md)
- [Experiment configuration](../../../research/configs/experiments/placement2d/README.md)
