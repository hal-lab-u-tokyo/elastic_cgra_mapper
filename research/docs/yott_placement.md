# YOTT Placement

YOTO/YOTT placement-only comparison on the YOTT paper's 23 case-study benchmarks.

## Setup

| item | value |
| --- | --- |
| manifest | `research/configs/experiments/placement2d/paper_comparison/yott_cases2021_best_vpr.json` |
| benchmark set | `benchmark/literature/yott_cases2021_source_order_normalized` |
| architecture | `fit_io_no_corners` |
| grid policy | `cpu_mapping_yoto_yott_fit_structural_io` |
| I/O cells | perimeter, corners excluded |
| network model | `one_hop_axis2` |
| VPR BB setting | `inner_num = 7`, `seed = 1`, output-LUT I/O pads |
| result | `research/results/placement2d/paper_comparison/yott_cases2021_best_vpr/20260629-105929_best_feasible_vpr_yoto_yott_compare` |
| figures | `research/results/placement2d/paper_comparison/yott_cases2021_best_vpr/20260629-105929_best_feasible_vpr_yoto_yott_compare/figures` |

This run completes all 23 benchmarks for VPR BB, Faithful Array YOTO/YOTT, and CPU Mapping YOTO/YOTT.

## Mappers

| mapper family | role | implementation |
| --- | --- | --- |
| Faithful Array YOTO/YOTT | paper-described traversal on the direct 2D grid engine | `mapper/src/placement2d/engine/placement2d_array_engine_faithful_traversal.cpp` |
| CPU Mapping YOTO/YOTT | public `cpu_mapping`-style traversal and placement behavior | `mapper/src/placement2d/engine/placement2d_array_engine_cpu_mapping.cpp` |
| VPR BB | external simulated-annealing placement baseline with bounding-box cost | `research/scripts/run_vpr_baseline.py` |

## Paper Metrics

| paper figure/table | local metric |
| --- | --- |
| Table 1 speedup | VPR BB `mapping_time_sec` divided by mapper `mapping_time_sec` |
| Table 1 average wire length | `placement_avg_cost` |
| Table 1 FIFO reduction | VPR BB FIFO sum divided by mapper FIFO sum |
| Fig. 19 optimal mapped edges | `placement_optimal_edge_ratio * 100` |
| Fig. 20(a) execution time | `mapping_time_sec * 1000` |
| Fig. 20(b) number of swaps | `placement_swap_attempts` |
| Fig. 22 maximal FIFO | `placement_max_cost` for the paper-style one-hop segment convention |

`placement_max_paper_fifo` is still recorded as `max(0, placement_cost - 1)`, but the corrected VPR BB FIFO series from the paper is closer to raw `placement_max_cost`.

## VPR BB

The closest VPR-BB-only setting is:

| item | value |
| --- | --- |
| run | `research/results/placement2d/probes/yott_vpr_endpoint_model_probe/20260629-042838_endpoint_model_probe2` |
| condition | `yott_cases2021_source_order / non_io_plus2_no_corners / inner=6 / seed=30` |
| Fig. 22 metric | `placement_max_cost` |

That setting is useful for checking VPR BB alone. The all-mapper comparison uses `fit_io_no_corners / inner=7 / seed=1` because every YOTO/YOTT mapper also needs enough structural I/O sites.

## Commands

```bash
docker compose -f environment/docker-compose.yaml exec gurobi bash -lc '
cd /home/ubuntu/elastic_cgra_mapper &&
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/placement2d/paper_comparison/yott_cases2021_best_vpr.json
'
```

```bash
python3 research/scripts/plot_yott_paper_metrics_by_benchmark.py \
  --run-dir research/results/placement2d/paper_comparison/yott_cases2021_best_vpr/20260629-105929_best_feasible_vpr_yoto_yott_compare \
  --output-dir research/results/placement2d/paper_comparison/yott_cases2021_best_vpr/20260629-105929_best_feasible_vpr_yoto_yott_compare/figures
```

```bash
python3 research/scripts/report_yott_benchmark_table.py \
  --run-dir research/results/placement2d/paper_comparison/yott_cases2021_best_vpr/20260629-105929_best_feasible_vpr_yoto_yott_compare \
  --output-dir research/results/placement2d/paper_comparison/yott_cases2021_best_vpr/20260629-105929_best_feasible_vpr_yoto_yott_compare/figures
```

## Limits

- The 23 benchmark graphs match the paper's node and edge counts, but opcode-level I/O counts do not always match the paper's IN/OUT columns.
- The paper does not publish the exact VPR command line, VTR version, I/O encoding, or FIFO postprocess.
- VPR BB maximal FIFO is close under `placement_max_cost`, but not exact for every benchmark.
- Placement-only figures do not prove routed CGRA correctness.

See also:

- `research/docs/yott_benchmarks.md`
- `research/docs/yott_vpr_bb.md`
- `research/docs/metrics.md`
