# TRAVERSAL, YOTT, and PRISA

This note summarizes how the repository relates to three placement papers. It is a guide for comparison experiments, not a claim of exact reproduction.

References:

- "TRAVERSAL: A Fast and Adaptive Graph-Based Placement and Routing for CGRAs", IEEE TCAD 2021.
- "You Only Traverse Twice: A YOTT Placement, Routing, and Timing Approach for CGRAs", ACM TECS 2021.
- "PRISA: A Potential Region-based Intelligent Search Algorithm for Dataflow Graph Mapping in Spatial CGRAs", ACM TRETS 2025.

## Status

| area | current confidence | use in this repository |
| --- | --- | --- |
| YOTT 2D placement benchmarks | High for node/edge counts on the 23 case-study DOTs. I/O classification and VPR details still have gaps. | Paper-metric comparison and ablation. |
| YOTO/YOTT placement algorithms | Partial. The code has paper-guided and public-code-guided variants, but exact behavior is still being checked. | Compare variants; do not treat one variant as proven ground truth. |
| VPR BB baseline | Partial. Several settings were probed; no setting matches every paper metric. | Baseline sensitivity checks and best-known comparison setting. |
| PRISA placement | Partial. The main PRISA/SIS structure is implemented, but exact benchmark granularity and some algorithmic choices remain under-specified. | PRISA-style placement comparison and probes. |
| Modulo adaptations | Exploratory. The papers are mainly 2D placement or placement-then-routing studies, while modulo mapping adds context assignment and routing feasibility. | Research prototypes, not paper reproduction. |

## Problem Split

| problem | meaning | main configs |
| --- | --- | --- |
| 2D placement | Place DFG nodes on physical PEs and measure placement quality. Routing is not validated unless explicitly added. | `research/configs/experiments/placement2d/search.json` |
| YOTT paper-metric comparison | 2D placement on the 23 YOTT case-study benchmarks with paper-style metrics. | `research/configs/experiments/placement2d/paper_comparison/yott_cases2021_best_vpr.json` |
| PRISA VPR-8 comparison | 2D placement on the closest available PRISA VPR-8 benchmark set. Some entries have granularity mismatch. | `research/configs/experiments/placement2d/paper_comparison/prisa_vpr8.json` |
| Modulo mapping | Place operations in `(PE, context)` and route DFG edges. | `research/configs/experiments/modulo/search.json` |

## Paper Settings

| method | paper focus | routing in the paper | modulo in the paper | repository treatment |
| --- | --- | --- | --- | --- |
| TRAVERSAL / YOTO | Graph traversal for CGRA placement and routing. | Traversal places local edges and routes remaining edges afterward. | Includes a 4x4 ADRES-style modulo experiment and a fully pipelined setting. | Used as inspiration for 2D placement variants and modulo probes. Exact modulo reproduction is not established. |
| YOTT | Two-pass traversal for fully pipelined 2D CGRA placement. | Placement first; remaining non-adjacent edges use simple routing. | Not the main evaluation target. | Main focus is placement-only metrics on 23 case-study benchmarks. |
| PRISA | Potential/weak-region guided 2D placement. | Placement first; simple Manhattan routing is discussed after placement. | Not a modulo mapper. | Implemented as `placement2d`; modulo variants are adaptations. |

## YOTT Comparison

Use:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/placement2d/paper_comparison/yott_cases2021_best_vpr.json
```

The main benchmark set is:

```text
benchmark/literature/yott_cases2021_source_order_normalized/
```

The 23 DOTs match the paper's node and edge counts. Some I/O details and the exact VPR BB setup remain uncertain.

Metrics used for comparison:

| paper item | repository metric |
| --- | --- |
| average wire length | `placement_avg_cost` |
| optimal mapped edges | `placement_optimal_edge_ratio` |
| execution time | `mapping_time_sec` |
| number of swaps | `placement_swap_attempts` |
| maximal FIFO | `placement_max_cost` for the one-hop cost convention |

Relevant docs:

- `research/docs/yott_placement.md`
- `research/docs/yott_benchmarks.md`
- `research/docs/yott_vpr_bb.md`

## PRISA Comparison

Use:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/placement2d/paper_comparison/prisa_vpr8.json
```

The closest benchmark set is:

```text
benchmark/literature/prisa_vpr8_normalized/vpr8/
```

This set is useful but not fully equivalent to the paper's VPR-8 data. Several generated entries differ in packed-block granularity.

Metrics used for comparison:

| paper item | repository metric |
| --- | --- |
| compilation time | `mapping_time_sec` |
| maximal FIFO | `placement_max_fifo` or `placement_max_cost`, depending on the chosen cost model |
| mapped LP | `placement_mapped_lp_mesh_hop` |
| optimal distance | `placement_optimal_distance_ratio` |

Relevant docs:

- `research/docs/prisa_placement.md`
- `benchmark/literature/prisa_vpr8_normalized/README.md`

## Modulo Notes

Modulo experiments are intentionally separate from the placement-only paper comparisons. They add at least three extra choices:

- context assignment;
- router choice;
- routing validation and failure handling.

YOTO/YOTT/PRISA modulo mappers should therefore be described as adaptations. They are useful for studying placement-then-routing ideas, but they should not be presented as direct reproductions of the 2D placement papers.

## Interpretation

- Use 2D placement results to study placement quality and paper-metric trends.
- Use modulo results to study routed CGRA feasibility and achieved II.
- Do not mix the two result types without stating the problem setting.
- Treat the current paper comparisons as best-effort baselines with documented gaps.
