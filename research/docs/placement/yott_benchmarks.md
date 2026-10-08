# YOTT Benchmarks

The YOTT case-study set contains 23 DOT files.

| item | value |
| --- | --- |
| mapper-ready DOTs | `benchmark/literature/yott_cases2021_normalized` |
| source-order DOTs | `benchmark/literature/yott_cases2021_source_order_normalized` |
| paper data | `research/data/papers/yott_table3_benchmarks.csv` |
| graph size check | `benchmark/literature/yott_cases2021_normalized/README.md` |

The normalized DOTs match the paper's node counts, edge counts, graph source counts, and graph sink counts for all 23 benchmarks.

The paper's IN/OUT columns correspond to graph sources and sinks. They do not always correspond to normalized `load` and `output` opcodes.

Use `yott_cases2021_source_order_normalized` for the VPR BB and YOTO/YOTT placement run. It preserves the source edge order used by the VPR BB probes.
