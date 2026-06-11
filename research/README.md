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
  --out research/results/baselines/smoke_test
```

Create a compact Markdown comparison:

```bash
python3 research/scripts/compare_results.py \
  --metrics research/results/baselines/smoke_test/metrics.csv \
  --group-by mapper \
  --out research/results/baselines/smoke_test/summary.md
```

Create a benchmark-level report:

```bash
python3 research/scripts/report_by_benchmark.py \
  --metrics research/results/baselines/smoke_test/metrics.csv \
  --out research/results/baselines/smoke_test/benchmark_report.md
```

Validate the metrics for internal consistency:

```bash
python3 research/scripts/validate_metrics.py \
  --metrics research/results/baselines/smoke_test/metrics.csv \
  --out research/results/baselines/smoke_test/validation.md
```

Generate the standard report set for any completed run:

```bash
python3 research/scripts/generate_reports.py \
  --result-dir research/results/baselines/smoke_test
```

The generated files under `research/results/` are ignored by git except for layout markers such as `.gitkeep` and `research/results/README.md`.

## Directory Layout

- `configs/arch_templates/`: CGRA templates without `context_size`; the runner fills it with each candidate II.
- `configs/mapper/`: mapper configuration files for `ILPMapper`, `ILPPlacementMapper`, `ConnectivityBasedILPMapper`, and `PlacementFirstHeuristicMapper`.
- `configs/experiments/`: reproducible experiment manifests.
- `scripts/`: II sweep runner, MII calculator, metrics normalizer, and comparison helper.
- `docs/`: short notes on the problem setting, metrics, and extension points.

## Adding a Mapper

Add new C++ mapping algorithms under `mapper/include/mapper/` and `mapper/src/`, derive them from `mapper::IMapper`, and register the `Algorithm.type` string with `mapper::RegisterMapperType` in the mapper's `.cpp` file. Then create a JSON config under `research/configs/mapper/` and include it in an experiment manifest. The JSON reader no longer needs to know every mapper type in advance; unknown names are checked by the mapper registry at runtime. Mapper `.cpp` files under `mapper/src/` are collected by CMake automatically after re-running `scripts/build.sh`.

See `research/docs/mapper_extension_guide.md` for the exact checklist.

## Algorithm Design Comparison

Use `configs/experiments/algorithm_design_compare.json` as the exploration suite while designing new mapping algorithms. It uses one 6x6 default CGRA, `mii: "auto"`, `ii_max: 16`, a 20-second per-II timeout, and four representative benchmarks: `fixed_stencil` for smoke, `fixed_ellpack` for memory pressure, and `fixed_convolution2d` plus `fixed_fft_pro` for routing stress. It intentionally excludes ILP-based mappers; add new heuristic or placement/routing algorithms to this manifest while exploring ideas.

To compare a new mapper, add or remove entries in the manifest's `mappers` list, then run:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/algorithm_design_compare.json
```

Use `configs/experiments/algorithm_design_reference_small.json` when you want a small all-mapper reference run that includes `ILPMapper`, `ILPPlacementMapper`, `ConnectivityBasedILPMapper`, and `PlacementFirstHeuristicMapper`:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/algorithm_design_reference_small.json
```

This reference suite uses only small kernels and a longer per-II timeout. It compares a new algorithm with the strict ILP, relaxed placement ILP, and connectivity-based ILP baselines under the same small benchmark set. `ILPMapper` uses DFG-edge-level routing flow constraints, so it is route-correct when it succeeds, but it can time out even on modest kernels if recurrence or routing constraints are tight. `ConnectivityBasedILPMapper` restricts routing to precomputed short MRRG paths, which usually makes it smaller than full edge-flow ILP while keeping route paths explicit.

If `--out` is omitted, `run_suite.py` creates a timestamped result directory under the manifest's result group and prints it. For this suite, runs go under `research/results/algorithm_design/algorithm_design_compare/`. The runner prints one concise progress line after each benchmark/architecture/mapper condition finishes, including run time, total elapsed time, and estimated remaining time. Only the status label is colorized by default; color is automatic on TTY output and can be controlled with `--color auto`, `--color always`, or `--color never`. Pass `--verbose` to show each II attempt, or `--quiet` to suppress progress output. Standard comparison reports are generated automatically unless `--skip-reports` is passed, including `routing_validation.md` for independent DFG-edge route reachability checks. Each run writes `run_metadata.json` and `run_info.md` with the command, start/end time, git commit, dirty status, filters, and log lookup conventions. `metrics.csv` also includes `trial_dir`, `stdout_file`, `stderr_file`, `arch_file`, `raw_output_dir`, `run_dir`, `mapping_file`, and `gurobi_log_file` columns.

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
  --out research/results/baselines/placement_routing_stress_sanity
```

Use `placement_routing_stress_probe.json` to quickly check a heavier routing benchmark (`fixed_convolution2d`) with short per-II timeouts. It is useful for detecting whether `route_to_compute_ratio`, `avg_manhattan_distance`, and `compute_bbox_utilization` move in the expected direction, but it should not be used as final quality evidence.

Use `benchmark_diversity_probe.json` to check whether the evaluation flow works across several benchmark types: memory-oriented kernels, vision-style kernels, recurrence kernels, and nested parallel matrix-multiply kernels. It is a short 6x6-only probe, not a final comparison.

Use `placement_comprehensive_baseline.json` for the broad placement baseline. It includes routing-stress, memory, vision, recurrence, smoke, and parallel-matrix benchmark sets across 4x4, 6x6, and 8x8 default CGRAs. Because this manifest is intentionally broad, run `preflight_manifest.py` first and split long runs with runner filters such as `--only-benchmark-set`, `--only-arch`, and `--only-mapper`.

Use `placement_routing_stress_baseline.json` for the actual placement-oriented baseline. It separates 4x4, 6x6, and 8x8 results, uses `mii: "auto"`, gives more `ii_max` headroom, uses a longer timeout, and includes `fixed_fft_pro`, `fixed_convolution2d`, `fixed_latnrm_pro`, `fixed_stencil`, recurrence/matrix kernels, and smoke kernels. Generate `benchmark_report.md`, `summary_by_mapper.md`, `summary_by_arch.md`, and `validation.md` after each run.

Preflight a long manifest before running it:

```bash
python3 research/scripts/preflight_manifest.py \
  --manifest research/configs/experiments/placement_comprehensive_baseline.json \
  --repo-root /home/ubuntu/elastic_cgra_mapper \
  --out-dir research/results/preflight/preflight_placement_comprehensive_baseline
```

Example chunked run:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/placement_comprehensive_baseline.json \
  --out research/results/baselines/placement_comprehensive_baseline_mesh6x6_routing \
  --only-arch mesh6x6_default_all \
  --only-benchmark-set kernel_routing_stress
```

Raw GenMap and CGRA-Bench `.dot` files are not always directly compatible with the current mapper input format because some use uppercase opcodes, `type` fields, or display `label`s instead of lowercase `opcode` fields. Convert or normalize them before adding them to final manifests.
