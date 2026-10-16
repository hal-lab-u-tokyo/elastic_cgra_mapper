# 2D Placement

Assigns each DFG node to one physical PE. Edge routes are not produced.

The registry and mapper interfaces retain their existing names and options.
`placement2d_array_mapper_base.cpp` forwards direct-grid options;
`placement2d_search_engine.cpp` forwards MRRG-search options.
`../algorithm_dispatch.cpp` selects one algorithm-owned engine.

| Algorithm | Direct-grid engine | MRRG-search engine |
| --- | --- | --- |
| SA | `sa/array/Placement2DSAEngine` | `sa/routed/Placement2DSAEngine` |
| PRISA and variants | `prisa/array/Placement2DPRISAEngine` | `prisa/routed/Placement2DPRISAEngine` |
| YOTO | `yoto/array/Placement2DYOTOEngine` | `yoto/routed/Placement2DYOTOEngine` |
| YOTT | `yott/array/Placement2DYOTTEngine` | `yott/routed/Placement2DYOTTEngine` |
| YOTT Core | `yott_core/array/Placement2DYOTTCoreEngine` | — |
| YOTT Core Repair | `yott_core_repair/array/Placement2DYOTTCoreRepairEngine` | — |
| ILP | `placement2d_ilp_mapper.cpp` | — |

Each directory contains `engine.hpp`, algorithm-local support and optimization
methods, and (for direct-grid engines) `types.hpp`. Variant dispatch remains
within its algorithm family, including paper-guided and CPU-mapping variants.
Cost/objective methods belong to that engine rather than an all-algorithm utility.

[Experiment configuration](../../../research/configs/experiments/placement2d/README.md)
