# Paper Reference Data

Reference values for the YOTT comparison are transcribed from the
[YOTT paper](https://doi.org/10.1145/3477038). Values printed in tables are
kept separate from values estimated from figures.

## YOTT

- `yott_table1.csv`: Table 1, aggregate speedup, average wire length, and FIFO reduction.
- `yott_table2_edge_categories.csv`: Table 2, average wire length by edge category.
- `yott_table3_benchmarks.csv`: Table 3, benchmark graph sizes and edge-category percentages.
- `yott_table4_multi_kernel.csv`: Table 4, multi-kernel runtime and FIFO values.
- `yott_figure_paper_metrics.csv`: per-benchmark values estimated from
  plot-only figures, with the VPR-BB maximal FIFO series corrected against the
  published plot.

The paper PDF and authors' source code are not copied into this repository.
Benchmark details are in
[`../../docs/placement/yott_benchmarks.md`](../../docs/placement/yott_benchmarks.md).
