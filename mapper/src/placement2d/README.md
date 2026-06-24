# 2D Placement Mappers

Mapper frontends for 2D placement. One physical PE is one placement slot, so experiments run with II/context size 1.

## Mapper Families

- `placement2d_yoto_mapper.cpp`: YOTO variants and `Algorithm.type` registration.
- `placement2d_yott_mapper.cpp`: YOTT variants and `Algorithm.type` registration.
- `placement2d_prisa_mapper.cpp`: PRISA, PRISA without SIS, cost-aware PRISA, and array PRISA variants.
- `placement2d_sa_mapper.cpp`: SA/VPR-facing placement baselines.
- `placement2d_ilp_mapper.cpp`: placement-only Gurobi ILP baseline.
- `placement2d_search_engine.cpp`: bridge to the generic search engine in `../common/search/`.
- `placement2d_array_mapper_base.cpp`: config wrapper for direct-grid array/cpu_mapping mappers.

## Implementation Areas

- `engine/`: direct 2D-grid implementation used by faithful array YOTO/YOTT, cpu_mapping YOTO/YOTT, array PRISA, and internal SA.
- `../common/search/`: generic placement/search implementation used by MRRG-facing 2D and modulo mappers.

## Reading Path

1. Start from the mapper family file (`placement2d_yoto_mapper.cpp`, `placement2d_yott_mapper.cpp`, `placement2d_prisa_mapper.cpp`, etc.).
2. Check the registered `Algorithm.type` and the engine kind passed by that mapper.
3. If the mapper uses `Placement2DArrayMapperBase`, continue in `engine/`.
4. If the mapper uses `placement2d_search_engine.cpp`, continue in `../common/search/`.

## YOTO/YOTT Reproduction

- Reproduction configs: `research/configs/mapper/placement2d/reproduction/`.
- Faithful array traversal: `engine/placement2d_array_engine_faithful_traversal.cpp`.
- cpu_mapping-compatible traversal: `engine/placement2d_array_engine_cpu_mapping.cpp`.
- Direct-grid objective and metrics: `engine/placement2d_array_engine_quality.cpp`.
- DFG/grid compatibility, I/O policy, and distance cache: `engine/placement2d_array_engine_cache.cpp`.
- Run configs: `research/configs/experiments/placement2d/reproduction/traversal_yott.json`.

## Adding A Mapper

1. Add a public header under `mapper/include/mapper/placement2d/`.
2. Add a mapper frontend in this directory.
3. Register a unique `Algorithm.type`.
4. Add a mapper config under `research/configs/mapper/placement2d/`.
5. Add the config to an experiment manifest under `research/configs/experiments/placement2d/`.

`research/scripts/preflight_manifest.py` checks that the DFG node count fits the physical PE count before a 2D placement run.
