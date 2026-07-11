# 2D Placement Ablation

Fix the benchmark, architecture, seed, and trial budget. Change one placement
stage at a time.

## Baseline

Use `research/configs/experiments/placement2d/literature/yott_2021.json` for the
benchmark and architecture conditions. Copy the manifest and add one mapper
entry per stage variant. Keep the mapper preset fixed and isolate each change
with `algorithm_overrides`:

```json
{
  "name": "yott_no_annotations",
  "mapper_config": "research/configs/mapper/placement2d/yott.json",
  "algorithm_overrides": {
    "max_trials": 100,
    "use_yott_annotations": false
  }
}
```

Check the resulting manifest before a full run:

```bash
python3 research/scripts/preflight_manifest.py \
  --manifest <manifest> \
  --out-dir /tmp/cgra_mapper_preflight \
  --only-benchmark mac \
  --only-mapper yott_no_annotations
```

## Stages

| stage | config switch | code |
| --- | --- | --- |
| Neighbor choice | `traversal_neighbor_policy` | `ChoosePaperGuidedNeighbor()`, `ChooseCPUMappingZigZagNeighbor()` |
| YOTT hints | `use_yott_annotations` | `BackpropagatePaperGuidedAnnotation()`, `ApplyCPUMappingCycleAnnotations()` |
| Candidate cells | `candidate_scope_policy` | `ClosestCompatibleCells()`, `CPUMappingTipCells()`, `TryCPUMappingAdjacency()` |
| Candidate ranking | `candidate_rank_policy` | `PaperGuidedTraversalRank()`, `BestCPUMappingDegreeCell()` |
| Final trial selection | fixed in this probe | CPU-mapping edge cost or `PlacementCost()` |

## Study Layout

Keep the paper comparison, simplification study, and repair study separate.

| question | baseline | variants | main evidence |
| --- | --- | --- | --- |
| Which YOTT stage matters? | regular YOTT | remove annotations; fix each traversal mode; change candidate scope; change candidate ranking | paired per-benchmark change from regular YOTT |
| What does YOTT Core simplify? | regular YOTT | YOTT Core at the same seed and trial count | quality-time trade-off and best-so-far trial curves |
| What does repair add? | YOTT Core | YOTT Core Repair with FIFO, cut, local repair, top-M, or profile repair removed one at a time | incremental quality gain and runtime cost |

Use the same 23 benchmarks, architecture, seed, and trial count within each
comparison. Compare 100 trials for stage screening and 1000 trials only for the
small set of surviving variants.

## Metrics

Use three headline metrics:

| role | metric | reason |
| --- | --- | --- |
| local placement quality | `placement_optimal_edge_ratio` | directly comparable with the YOTT paper |
| worst timing pressure | `placement_max_paper_fifo` | exposes a single long dependency hidden by averages |
| search cost | `mapping_time_sec` | measures the quality-time trade-off |

`placement_max_paper_fifo` is the cost-minus-one FIFO-depth proxy used for the
ablation. Paper-figure comparisons use `placement_max_cost` as documented in
[YOTT Placement](yott.md), because it is closer to the reported VPR BB series.

Use `placement_avg_cost`, `placement_max_cut_congestion`, and
`placement_estimated_max_link_demand` as secondary metrics. Use
`placement_swap_attempts`, failed-trial count, and trial variance only to explain
why a change works.

Report per-benchmark paired differences before means. Group the 23 cases by node
count, reconvergent-edge ratio, I/O ratio, and maximum degree when interpreting
which DFG structures benefit.

## Figures

1. Algorithm blocks for regular YOTT, YOTT Core, and YOTT Core Repair.
2. Three per-benchmark panels: optimal mapped edges, maximal FIFO, and execution time.
3. A paired-effect heatmap for YOTT stage removals; plot execution time separately.
4. Best-so-far quality versus trial count for regular YOTT and YOTT Core.
5. Leave-one-component-out effects for YOTT Core Repair.

Keep mesh-hop and congestion figures in supplementary results unless they change
the conclusion from the three headline metrics.

## Current Findings

The following results use the 23 YOTT case-study graphs, structural perimeter
I/O without corners, and the one-hop placement model. They describe this setup,
not routed modulo mapping.

- Random candidate selection is the main reason YOTT Core outperforms regular
  YOTT. Across 1000 trials, the mean distinct-placement ratio rises from 42.0%
  to 99.8%. Core covers more placements on 20 of 23 graphs and is never worse.
- With the traversal plan held fixed, random candidate selection improves
  optimal mapped edges by 2.63 percentage points and reduces maximal FIFO by
  0.49 on average.
- Reconvergence annotations remain useful: adding them improves optimal mapped
  edges by 1.80 percentage points and reduces placement cost by 2.70%.
- Random traversal contributes little after candidate selection is already
  random. Its mean change is +0.12 percentage points in optimal mapped edges,
  while maximal FIFO and placement cost slightly worsen.
- Placement-cost trial selection changes the result on only 2 of 23 graphs in
  the matched-candidate comparison. It is not the main source of Core's gain.

The smallest strong variant is therefore mixed traversal with random candidate
selection and annotations. At 1000 trials it reaches 85.92% optimal mapped
edges, maximal FIFO 2.48, and 220.5 ms. YOTT Core reaches 85.85%, 2.57, and
220.2 ms under the same benchmark setting.

The YOTT Core Repair preset uses the same 100 YOTT Core trials, retains four
placements, and applies 16 guarded local-repair iterations. On the 23 YOTT
case-study DFGs with structural perimeter I/O, mean optimal mapped edges rise
from 83.34% to 86.38%, mean FIFO falls from 0.257 to 0.205, and mean maximal
FIFO falls from 3.09 to 1.91. Mean mapping time rises from 27.0 ms to 42.1 ms.
Optimal mapped edges improve or tie on every DFG. Increasing the elite count
from 4 to 16 changes optimal mapped edges by only 0.09 percentage points and
does not further reduce maximal FIFO. These results cover 2D placement only
and do not establish routed modulo quality.

## Reading Results

Compare each row against the unchanged `yott` preset.

If a stage changes `placement_optimal_edge_ratio` but also worsens
`placement_estimated_max_link_demand`, the placement is improving local edge
quality while concentrating routing demand. That is a useful signal for the
modulo routing-aware algorithm design.

## Adding A New Stage Switch

Add a JSON option only when the stage cannot be isolated by existing mapper
entries. Wire it through:

1. `entity/include/entity/mapper_config.hpp`
2. `io/src/mapper_config_io.cpp`
3. `mapper/include/mapper/mapper.hpp`
4. `mapper/include/mapper/placement2d/placement2d_array_mapper_base.hpp`
5. `mapper/src/placement2d/placement2d_array_mapper_base.cpp`
6. `mapper/src/placement2d/engine/array_engine_internal.hpp`
7. the stage function under `mapper/src/placement2d/engine/`

Keep the option name tied to the stage, for example
`traversal_neighbor_policy` or `candidate_rank_policy`.
