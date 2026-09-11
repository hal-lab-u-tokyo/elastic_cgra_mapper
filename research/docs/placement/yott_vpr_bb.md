# YOTT VPR BB

The YOTT paper reports VPR BB as the baseline for speed, wire length, optimal mapped edges, swaps, and maximal FIFO.

## Comparison Setting

| item | value |
| --- | --- |
| manifest | `research/configs/experiments/placement2d/literature/yott_2021.json` |
| benchmarks | `yott_cases2021_source_order` |
| grid | `fit_io_no_corners` |
| VPR BB | `inner_num = 7`, `seed = 1` |

This setting keeps the 23 DFGs and architecture fixed across VPR BB and the in-repository mappers.

## Metrics

| paper item | repository metric |
| --- | --- |
| wire length | `placement_avg_cost` |
| optimal mapped edges | `placement_optimal_edge_ratio` |
| swaps | `placement_swap_attempts` |
| maximal FIFO | `placement_max_cost` |

`placement_max_cost` counts one-hop communication segments directly. `placement_max_paper_fifo` subtracts one segment and is kept as a cost-minus-one FIFO proxy, but it is not the closest match to the paper VPR BB maximal-FIFO series.

## Remaining Gap

No tested VPR 8 setting matches all paper metrics at once. The remaining likely causes are the exact VPR command line, the VTR/VPR build used by the paper, the I/O block encoding, and the FIFO postprocess used after placement.

## Possible Gap Sources

Use these as disclosure points when reporting results from this repository.

- **VPR version and options**: the paper does not publish the full VPR command line, annealing options, VTR/VPR revision, or build configuration. Small changes in placement effort, seed handling, and packer behavior can move per-benchmark results.
- **Grid and I/O model**: separate VPR-only sweeps favored `non_io_plus2_no_corners` for some FIFO values, while the shared comparison requires `fit_io_no_corners` so every mapper has enough structural I/O sites. Corner I/O, output nodes as I/O pads versus CLBs, and structural I/O capacity all affect both placement and FIFO metrics.
- **Benchmark encoding**: the 23 DOTs match paper node/edge counts, but source edge order, source/sink encoding, opcode names, and I/O classification may still differ from the exact input files used by the paper.
- **BLIF/net conversion**: the VPR runner must translate DFG nodes into a VPR-compatible netlist. LUT packing, constants, buffers, high-fanin decomposition, and output-pad modeling can change what VPR actually places.
- **Metric definition**: the paper's wire length, optimal edge, swap count, and maximal FIFO are not fully specified as executable formulas. This repository reports `placement_avg_cost`, `placement_optimal_edge_ratio`, `placement_swap_attempts`, and `placement_max_cost`; these are close but may not be identical to the paper's postprocess.
- **FIFO convention**: `placement_max_cost` matches the corrected paper VPR-BB maximal-FIFO trend better than `max(0, placement_cost - 1)`, but the exact paper FIFO postprocess is still unknown.
- **Runtime environment**: execution time depends on CPU, compiler flags, Docker overhead, and VPR build mode. Use runtime primarily for relative comparisons within the same run.

The current VPR BB rows should therefore be treated as the closest documented baseline in this environment, not as a verified reproduction of the paper's VPR BB run.
