# YOTT Placement

YOTO/YOTT placement-only comparison on the YOTT paper's 23 case-study benchmarks. This is a best-effort comparison setup; exact paper reproduction is not established.

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

The current manifest completes all 23 benchmarks for VPR BB, paper-guided YOTO/YOTT, and CPU Mapping YOTO/YOTT.

## Mappers

| mapper family | role | implementation |
| --- | --- | --- |
| Paper-guided Array YOTO/YOTT | traversal variant on the direct 2D grid engine. The C++ class name contains `Faithful`, but exact reproduction is not established. | `mapper/src/placement2d/engine/placement2d_array_engine_faithful_traversal.cpp` |
| CPU Mapping YOTO/YOTT | public `cpu_mapping`-style traversal and placement behavior | `mapper/src/placement2d/engine/placement2d_array_engine_cpu_mapping.cpp` |
| VPR BB | external simulated-annealing placement baseline with bounding-box cost | `research/scripts/run_vpr_baseline.py` |

## Paper Metrics

| paper figure/table | repository metric |
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

The closest VPR-BB-only setting is described in `research/docs/yott_vpr_bb.md`.

The all-mapper comparison here uses `fit_io_no_corners / inner=7 / seed=1` because every YOTO/YOTT mapper also needs enough structural I/O sites.

## Commands

```bash
docker compose -f environment/docker-compose.yaml exec gurobi bash -lc '
set -e
cd /home/ubuntu/elastic_cgra_mapper

RUN_DIR="research/results/placement2d/paper_comparison/yott_cases2021_best_vpr/$(date +%Y%m%d-%H%M%S)_metric_compare"

python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/placement2d/paper_comparison/yott_cases2021_best_vpr.json \
  --out "$RUN_DIR"

python3 research/scripts/plot_yott_paper_metrics_by_benchmark.py \
  --run-dir "$RUN_DIR" \
  --output-dir "$RUN_DIR/figures"

python3 research/scripts/report_yott_benchmark_table.py \
  --run-dir "$RUN_DIR" \
  --output-dir "$RUN_DIR/figures"
'
```

## Status

- The 23 benchmark graphs match the paper's node and edge counts, but opcode-level I/O counts do not always match the paper's IN/OUT columns.
- The paper does not publish the exact VPR command line, VTR version, I/O encoding, or FIFO postprocess.
- VPR BB maximal FIFO is close under `placement_max_cost`, but not exact for every benchmark.
- YOTO/YOTT implementation variants should be compared as hypotheses, not treated as confirmed reproductions.
- Placement-only figures do not prove routed CGRA correctness.

See also:

- `research/docs/yott_benchmarks.md`
- `research/docs/yott_vpr_bb.md`
- `research/docs/metrics.md`
