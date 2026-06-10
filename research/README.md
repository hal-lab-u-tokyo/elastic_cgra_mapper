# Research Workflow

This directory contains a small research layer on top of the existing `build/mapping` executable. It is intended for modulo-style CGRA mapper studies where `context_size` is treated as the candidate II and `CGRA_type` is forced to `default`.

## Quick Start

Build the project first:

```bash
sh scripts/build.sh
```

Run the smoke experiment:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/smoke_test.json \
  --out research/results/smoke_test
```

Create a compact Markdown comparison:

```bash
python3 research/scripts/compare_results.py \
  --metrics research/results/smoke_test/metrics.csv \
  --group-by mapper \
  --out research/results/smoke_test/summary.md
```

Create a benchmark-level report:

```bash
python3 research/scripts/report_by_benchmark.py \
  --metrics research/results/smoke_test/metrics.csv \
  --out research/results/smoke_test/benchmark_report.md
```

Validate the metrics for internal consistency:

```bash
python3 research/scripts/validate_metrics.py \
  --metrics research/results/smoke_test/metrics.csv \
  --out research/results/smoke_test/validation.md
```

Generate the standard report set for any completed run:

```bash
python3 research/scripts/generate_reports.py \
  --result-dir research/results/smoke_test
```

The generated files under `research/results/` are ignored by git except for `.gitkeep`.

## Directory Layout

- `configs/arch_templates/`: CGRA templates without `context_size`; the runner fills it with each candidate II.
- `configs/mapper/`: mapper configuration files for `ILPMapper`, `ILPPlacementMapper`, and `PlacementFirstHeuristicMapper`.
- `configs/experiments/`: reproducible experiment manifests.
- `scripts/`: II sweep runner, MII calculator, metrics normalizer, and comparison helper.
- `docs/`: short notes on the problem setting, metrics, and extension points.

## Standard Assumptions

For ordinary modulo CGRA mapping comparisons, use `parallel_num = 1`. The current `build/mapping` executable duplicates the input DFG when `parallel_num > 1`, which is useful for throughput-style experiments but changes the mapping problem for algorithm comparisons.

The MII helper computes ResMII and RecMII. RecMII uses explicit edge distance attributes when present, and by default treats missing-distance self-loop edges as distance-1 recurrences for compatibility with the current benchmark files.

The smoke manifest starts from a fixed II so it can confirm that logs and metrics are generated quickly. Use `mii: "auto"` in baseline manifests when measuring achieved II from the computed lower bound.

Manifests can define multiple benchmark sets:

```json
"benchmark_sets": [
  {
    "name": "kernel",
    "benchmark_root": "/home/ubuntu/elastic_cgra_mapper/benchmark/kernel",
    "benchmarks": ["convolution_no_loop", "fixed_fir_pro"]
  },
  {
    "name": "converted_cgrabench",
    "benchmark_root": "/home/ubuntu/elastic_cgra_mapper/benchmark/kernel",
    "benchmarks": ["fft_pro"]
  }
]
```

The aggregate `metrics.csv` includes `benchmark_set`, and `report_by_benchmark.py` breaks results down by set, benchmark, mapper, and architecture.

For placement-oriented algorithm research, see `research/docs/placement_research_plan.md`.

## Placement Baselines

Use `placement_routing_stress_sanity.json` for a quick end-to-end check that the 4x4, 6x6, and 8x8 architecture templates, auto MII, II sweep, mapper selection, and placement metrics all work.

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/placement_routing_stress_sanity.json \
  --out research/results/placement_routing_stress_sanity
```

Use `placement_routing_stress_probe.json` to quickly check a heavier routing benchmark (`fixed_convolution2d`) with short per-II timeouts. It is useful for detecting whether `route_to_compute_ratio`, `avg_manhattan_distance`, and `compute_bbox_utilization` move in the expected direction, but it should not be used as final quality evidence.

Use `benchmark_diversity_probe.json` to check whether the evaluation flow works across several benchmark types: memory-oriented kernels, vision-style kernels, recurrence kernels, and nested parallel matrix-multiply kernels. It is a short 6x6-only probe, not a final comparison.

Use `placement_paper_baseline.json` as the broad paper-oriented manifest. It includes routing-stress, memory, vision, recurrence, smoke, and parallel-matrix benchmark sets across 4x4, 6x6, and 8x8 default CGRAs. Because this manifest is intentionally broad, run `preflight_manifest.py` first and split long runs with runner filters such as `--only-benchmark-set`, `--only-arch`, and `--only-mapper`.

Use `placement_routing_stress_baseline.json` for the actual placement-oriented baseline. It separates 4x4, 6x6, and 8x8 results, uses `mii: "auto"`, gives more `ii_max` headroom, uses a longer timeout, and includes `fixed_fft_pro`, `fixed_convolution2d`, `fixed_latnrm_pro`, `fixed_stencil`, recurrence/matrix kernels, and smoke kernels. Generate `benchmark_report.md`, `summary_by_mapper.md`, `summary_by_arch.md`, and `validation.md` after each run.

Preflight a long manifest before running it:

```bash
python3 research/scripts/preflight_manifest.py \
  --manifest research/configs/experiments/placement_paper_baseline.json \
  --repo-root /home/ubuntu/elastic_cgra_mapper \
  --out-dir research/results/preflight_placement_paper_baseline
```

Example chunked run:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/placement_paper_baseline.json \
  --out research/results/placement_paper_baseline_mesh6x6_routing \
  --only-arch mesh6x6_default_all \
  --only-benchmark-set kernel_routing_stress
```

Raw GenMap and CGRA-Bench `.dot` files are not always directly compatible with the current mapper input format because some use uppercase opcodes, `type` fields, or display `label`s instead of lowercase `opcode` fields. Convert or normalize them before adding them to final manifests.
