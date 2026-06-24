# Direct-Grid Placement Engine

Direct-grid implementation for faithful array YOTO/YOTT, cpu_mapping YOTO/YOTT, array PRISA, and internal SA.

## Files

- `placement2d_array_engine.cpp`: run flow, result conversion, and logging.
- `placement2d_array_engine_internal.hpp`: private state, helper structs, and method declarations.
- `placement2d_array_engine_options.cpp`: config normalization, seeds, timeouts, and mapper names.
- `placement2d_array_engine_cache.cpp`: DFG edges, grid cells, compatibility tables, I/O policy, and distance cache.
- `placement2d_array_engine_quality.cpp`: placement cost, quality metrics, ranking helpers, and validity checks.
- `placement2d_array_engine_faithful_traversal.cpp`: faithful array YOTO/YOTT traversal order, annotations, and candidate ranking.
- `placement2d_array_engine_cpu_mapping.cpp`: cpu_mapping-style YOTO/YOTT port, type matrix, freedom grid, tips, and intersections.
- `placement2d_array_engine_prisa.cpp`: PRISA SIS, PR/WR computation, move proposals, and refinement.
- `placement2d_array_engine_sa.cpp`: internal SA placement baseline.

## Edit Points

- YOTO/YOTT traversal order: `placement2d_array_engine_faithful_traversal.cpp` or `placement2d_array_engine_cpu_mapping.cpp`.
- Candidate scoring and placement cost: `placement2d_array_engine_quality.cpp`.
- I/O-node treatment and architecture compatibility: `placement2d_array_engine_cache.cpp`.
- PRISA placement behavior: `placement2d_array_engine_prisa.cpp`.
- Mapper options exposed to JSON configs: `placement2d_array_engine_options.cpp` plus `mapper/include/mapper/placement2d/placement2d_array_mapper_base.hpp`.
