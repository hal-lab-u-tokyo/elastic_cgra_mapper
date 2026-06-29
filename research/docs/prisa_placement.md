# PRISA Placement

PRISA is available as a 2D placement mapper inspired by "PRISA: A Potential Region-based Intelligent Search Algorithm for Dataflow Graph Mapping in Spatial CGRAs" (ACM TRETS 2025, DOI: 10.1145/3723045).

This implementation is useful for comparison and ablation, but exact paper reproduction is not established. The paper leaves several implementation details open, and the available VPR-8 benchmark artifacts do not fully match the paper's packed-block granularity.

## Scope

| item | status |
| --- | --- |
| problem | 2D placement |
| routing | not part of the mapper; placement-only metrics are the primary comparison target |
| modulo mapping | not a direct PRISA paper setting |
| benchmark set | closest available VPR-8-like DOTs under `benchmark/literature/prisa_vpr8_normalized/vpr8/` |

## Implemented Mappers

| mapper | role |
| --- | --- |
| `Placement2DArrayPRISAMapper` | PRISA-style placement with SIS |
| `Placement2DArrayPRISANoSISMapper` | PRISA-style placement without SIS |
| `Placement2DPRISAMapper` | shared-engine reference with SIS |
| `Placement2DPRISANoSISMapper` | shared-engine reference without SIS |
| `Placement2DCostAwarePRISAMapper` | derived variant that adds direct placement-quality refinement |

The cost-aware mapper is an experimental variant. Do not use it as the paper PRISA implementation.

## Main Ideas

- Build a resource distance matrix (`DM`) on the 2D mesh.
- Split resource pairs into potential and weak regions.
- Use SIS to create an initial low-bandwidth DFG labeling.
- Propose swaps that reduce weak-region adjacency.
- Report placement-only metrics such as optimal distance, FIFO proxy, mapped LP, and runtime.

Some tie-breaks and candidate choices are under-specified in the paper. The current implementation documents those choices in `mapper/src/placement2d/engine/placement2d_array_engine_prisa.cpp`.

## Run

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/placement2d/paper_comparison/prisa_vpr8.json
```

## Metrics

| paper item | repository metric |
| --- | --- |
| compilation time | `mapping_time_sec` |
| maximal FIFO | `placement_max_fifo` or `placement_max_cost`, depending on cost model |
| mapped LP | `placement_mapped_lp_mesh_hop` |
| optimal distance | `placement_optimal_distance_ratio` |

## Known Gaps

- Several VPR-8 entries have different node counts from the paper.
- The exact BLIF/DFG extraction path used by the paper is not available in this repository.
- The exact VPR baseline commands used by the paper are not fully specified.
- The current PRISA implementation should be treated as a paper-guided implementation, not a verified reproduction.

Benchmark notes: `benchmark/literature/prisa_vpr8_normalized/README.md`.
