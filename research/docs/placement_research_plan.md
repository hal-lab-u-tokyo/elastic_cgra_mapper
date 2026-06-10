# Placement Research Plan

The near-term research target is to compare placement-oriented CGRA mapping methods against the existing ILP baselines, then design a better placement strategy.

## Baseline Readiness

Before adding a new mapper, run an auto-II sanity suite and validate the results:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/auto_sanity.json \
  --out research/results/auto_sanity

python3 research/scripts/report_by_benchmark.py \
  --metrics research/results/auto_sanity/metrics.csv \
  --out research/results/auto_sanity/benchmark_report.md

python3 research/scripts/validate_metrics.py \
  --metrics research/results/auto_sanity/metrics.csv \
  --out research/results/auto_sanity/validation.md
```

For a real baseline, use `mii: "auto"`, enough `ii_max` headroom, and a timeout large enough to avoid mistaking solver timeout for algorithm failure.

For a slightly broader placement-only sanity check, run:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/placement_auto_sanity.json \
  --out research/results/placement_auto_sanity
```

For the 4x4/6x6/8x8 placement baseline requested for YOTT/PRISA-style work, use:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/placement_routing_stress_baseline.json \
  --out research/results/placement_routing_stress_baseline

python3 research/scripts/report_by_benchmark.py \
  --metrics research/results/placement_routing_stress_baseline/metrics.csv \
  --out research/results/placement_routing_stress_baseline/benchmark_report.md

python3 research/scripts/compare_results.py \
  --metrics research/results/placement_routing_stress_baseline/metrics.csv \
  --group-by mapper \
  --out research/results/placement_routing_stress_baseline/summary_by_mapper.md

python3 research/scripts/compare_results.py \
  --metrics research/results/placement_routing_stress_baseline/metrics.csv \
  --group-by arch_name \
  --out research/results/placement_routing_stress_baseline/summary_by_arch.md

python3 research/scripts/validate_metrics.py \
  --metrics research/results/placement_routing_stress_baseline/metrics.csv \
  --out research/results/placement_routing_stress_baseline/validation.md
```

Use `placement_routing_stress_probe.json` first when iterating on code. It keeps the same 4x4/6x6/8x8 split, but limits the benchmark set and timeout so routing-pressure metrics can be checked quickly.

Use `benchmark_diversity_probe.json` to sanity-check different benchmark families before a long baseline run. The current probe covers `fixed_ellpack` (memory-oriented), `fixed_susan_pro` (vision-style), `fixed_matrixmultiply_const` (recurrence), and `fixed_matrixmultiply_double_const` from the nested `benchmark/parallel` tree.

For the broad paper-oriented run, use `placement_paper_baseline.json`. Always preflight it first:

```bash
python3 research/scripts/preflight_manifest.py \
  --manifest research/configs/experiments/placement_paper_baseline.json \
  --repo-root /home/ubuntu/elastic_cgra_mapper \
  --out-dir research/results/preflight_placement_paper_baseline
```

Then run it in chunks, for example:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/placement_paper_baseline.json \
  --out research/results/placement_paper_baseline_mesh6x6_parallel \
  --only-arch mesh6x6_default_all \
  --only-benchmark-set parallel_matrix

python3 research/scripts/generate_reports.py \
  --result-dir research/results/placement_paper_baseline_mesh6x6_parallel
```

Chunked runs are preferable for ILP-heavy baselines because the pessimistic timeout budget can be much larger than the actual runtime.

## Placement-Oriented Comparison Axes

Use the same DFG, architecture, II search rule, timeout, and metrics for every mapper. Report at least:

- `achieved_II` and `II_ratio`
- success rate and timeout/optimal status
- mapping or placement-and-routing time
- `compute_pe_utilization` and `pe_context_utilization`
- `route_to_compute_ratio` and `routing_overhead_ratio`
- `avg_manhattan_distance` and `max_manhattan_distance`
- `compute_bbox_utilization`
- benchmark-level breakdowns, not only averages

## YOTT/PRISA Direction

YOTT-style work is useful as a placement-quality and compilation-time target: it emphasizes fast placement, average wire length, and timing/FIFO-size quality. PRISA-style work is useful as a search-space guidance target: it emphasizes identifying promising regions, reducing ineffective search, compilation time, and communication cost.

This repository can initially compare against these ideas through equivalent metrics even before implementing exact replicas: placement time, achieved II after routing/timing, PE utilization, route overhead, average hop count, and benchmark-level success. A faithful reproduction would require matching architecture assumptions and benchmark suites more closely.

## Candidate New Mapper Idea

The included `PlacementFirstHeuristicMapper` is a deliberately simple placement-first baseline, not a YOTT or PRISA reproduction. It ranks nodes by connectivity to already placed nodes, packs connected operations close together, then runs a BFS router over the MRRG. On routing-stress kernels it is expected to expose weaknesses in naive placement-first mapping; use it as a lower baseline and a scaffold for better algorithms.

A stronger proposed mapper should be placement-first:

1. Rank DFG nodes by criticality, recurrence involvement, degree, and memory affinity.
2. Select a compact potential region on the CGRA, similar in spirit to PRISA.
3. Place high-degree and critical nodes first to reduce bounding boxes and expected routing.
4. Use local repair when routing pressure or fanout becomes high.
5. Use the existing II-sweep runner and metrics to compare against `ILPMapper` and `ILPPlacementMapper`.

The first implementation does not need to beat ILP on tiny graphs. It should aim to scale better while preserving acceptable `achieved_II` and lower routing pressure on larger graphs.
