# YOTT VPR BB

The YOTT paper reports VPR BB as the baseline for speed, wire length, optimal mapped edges, swaps, and maximal FIFO.

## Closest VPR-BB-Only Setting

| item | value |
| --- | --- |
| run | `research/results/placement2d/probes/yott_vpr_endpoint_model_probe/20260629-042838_endpoint_model_probe2` |
| benchmarks | `yott_cases2021_source_order` |
| grid | `non_io_plus2_no_corners` |
| VPR BB | `inner_num = 6`, `seed = 30` |
| Fig. 22 metric | `placement_max_cost` |
| figure values | `research/results/placement2d/probes/yott_vpr_endpoint_model_probe/20260629-042838_endpoint_model_probe2/figures/yott_fig22_vpr_bb_max_fifo_paper_vs_local_best_values.csv` |

This setting is the closest VPR-BB-only match for the corrected paper maximal-FIFO series.

## All-Mapper Setting

| item | value |
| --- | --- |
| manifest | `research/configs/experiments/placement2d/paper_comparison/yott_cases2021_best_vpr.json` |
| run | `research/results/placement2d/paper_comparison/yott_cases2021_best_vpr/20260629-105929_best_feasible_vpr_yoto_yott_compare` |
| benchmarks | `yott_cases2021_source_order` |
| grid | `fit_io_no_corners` |
| VPR BB | `inner_num = 7`, `seed = 1` |

This setting is used when VPR BB, Faithful Array YOTO/YOTT, and CPU Mapping YOTO/YOTT must all run on the same 23 benchmarks.

## Metrics

| paper item | local value |
| --- | --- |
| wire length | `placement_avg_cost` |
| optimal mapped edges | `placement_optimal_edge_ratio` |
| swaps | `placement_swap_attempts` |
| maximal FIFO | `placement_max_cost` |

`placement_max_cost` counts one-hop communication segments directly. `placement_max_paper_fifo` subtracts one segment and is kept as a local FIFO proxy, but it is not the closest match to the paper VPR BB maximal-FIFO series.

## Remaining Gap

No tested VPR 8 setting matches all paper metrics at once. The remaining likely causes are the exact VPR command line, the VTR/VPR build used by the paper, the I/O block encoding, and the FIFO postprocess used after placement.
