# Experiment Protocol

The default research protocol compares mappers under the same DFG, architecture template, II search range, timeout, and metric extraction logic.

Before running a long experiment, run `research/scripts/preflight_manifest.py` on the manifest. A final baseline manifest should have no missing files, no unsupported operations, a computable MII for every benchmark/architecture pair, and a non-empty II range.

1. Select benchmark `.dot` files from `benchmark/kernel/` or converted CGRA-Bench inputs.
2. Select an architecture template from `research/configs/arch_templates/`.
3. Compute MII from ResMII and RecMII unless a manifest overrides the start II.
4. For each candidate `II` from `MII` to `ii_max`, generate a temporary CGRA JSON with `context_size = II` and `CGRA_type = "default"`.
5. Run `build/mapping` with one mapper config, one DFG, one generated CGRA JSON, one timeout, and `parallel_num = 1`.
6. Stop at the first successful mapping and record it as `achieved_II`.
7. Normalize logs into `metrics.csv` and use the same CSV columns for all mappers.

The small baseline manifest is `research/configs/experiments/modulo_baseline_small.json`. It is intentionally conservative and should be treated as a starting point rather than a publication-scale evaluation.

Use `research/configs/experiments/algorithm_design_compare.json` as the exploration suite while designing new mapper algorithms. It currently uses a 6x6 default CGRA, auto-MII II sweep, `ii_max: 16`, a 20-second per-II timeout, and four representative benchmarks: `fixed_stencil`, `fixed_ellpack`, `fixed_convolution2d`, and `fixed_fft_pro`. ILP-based mappers are intentionally excluded, so add new heuristic or placement/routing mapper entries to this manifest for iterative comparisons.

Use `research/configs/experiments/algorithm_design_reference_small.json` for small all-mapper comparisons across the currently implemented mappers. It includes `ILPMapper`, `ILPPlacementMapper`, `ConnectivityBasedILPMapper`, and `PlacementFirstHeuristicMapper`, but only on small kernels and with a longer timeout. Switch between the two settings by changing only the `--manifest` argument to `run_suite.py`.

Use `ILPMapper` as the first baseline because it is already wired into the executable and solves placement together with DFG-edge-level routing flow constraints on the ordinary `default` CGRA model. This makes it a stricter correctness baseline, but routing-stress kernels can become much slower or time out under short exploration timeouts. Use `ILPPlacementMapper` as a second baseline when you want to compare against the placement-focused ILP formulation, and always inspect `routing_validation.md` before treating a placement-focused result as route-correct.

Use `research/scripts/normalize_benchmarks.py` when evaluating benchmark collections that are not already in the mapper's DOT format. The normalizer scans `.dot` and Revamp `.xml` files, writes mapper-compatible `.dot` files, and can generate a probe manifest at the same time. It handles the current in-repository CGRA-Bench evaluation DOTs, CGRA-Bench kernel DOTs, GenMap DOTs, Revamp XMLs, and the native `benchmark/kernel`, `benchmark/cgrame_kernel`, and `benchmark/parallel` DOTs.

```bash
python3 research/scripts/normalize_benchmarks.py \
  --benchmark-root benchmark \
  --out-dir research/results/benchmark_compatibility/normalized_benchmarks \
  --manifest-out research/results/benchmark_compatibility/normalized_benchmarks/all_normalized_manifest.json \
  --report-out research/results/benchmark_compatibility/normalized_benchmarks/normalization_report.md
```

The normalizer maps memory operations to `load`/`output`, drops control-only `br` nodes, maps `phi` to `const`, folds bitwise/cast/vector helper ops into supported one-cycle ALU classes, and annotates backward dependencies with `distance=1` so RecMII can be computed. This normalization is intended for mapping/placement/routing evaluation, not cycle-accurate program simulation.

After normalization, run `preflight_manifest.py` on the generated manifest before running a suite. A quick all-benchmark health check can use the generated ILP-only manifest with `mesh6x6_default_all`, `mii: "auto"`, `ii_max: 24`, and a short timeout.

For RecMII, prefer explicit loop-carried edge distances in `.dot` files. The manifest field `mii_missing_distance_policy` defaults to `self_loop` so current benchmark self-loops can be used as distance-1 recurrences, but `strict` is better when all recurrence distances are annotated.

To evaluate multiple benchmark collections in one run, use `benchmark_sets` instead of top-level `benchmark_root` and `benchmarks`. Each result row gets a `benchmark_set` column, and each run is stored under `set=<name>/benchmark=<benchmark>/arch=<arch>/mapper=<mapper>/`.

For large manifests, split execution without editing JSON by using `run_suite.py` filters: `--only-benchmark-set`, `--only-benchmark`, `--only-arch`, and `--only-mapper`. The same filters are available in `preflight_manifest.py`.

Use `research/scripts/report_by_benchmark.py` after a suite finishes to inspect benchmark-specific behavior. This matters because mapper averages can hide failures or routing pressure concentrated in only one kernel.

Use `research/scripts/validate_metrics.py` before interpreting a run. It checks internal consistency, highlights smoke-test shortcuts such as `start_II > MII`, and records a short prior-work sanity checklist.

`run_suite.py` creates the standard `benchmark_report.md`, `summary_by_mapper.md`, `summary_by_arch.md`, `summary_by_set.md`, `validation.md`, and `routing_validation.md` files automatically unless `--skip-reports` is passed. `validation.md` checks metric consistency, while `routing_validation.md` checks that each successful mapping connects every DFG edge through valid mapping connections and route configs. Use `research/scripts/generate_reports.py --result-dir <result_dir>` when you want to regenerate reports for an existing result directory.

If `run_suite.py --out` is omitted, the result directory is created automatically under `research/results/<result_group>/<timestamp>`. A manifest can set `result_group`, and otherwise the manifest filename is used. Every run writes `run_metadata.json` and `run_info.md`, and `metrics.csv` includes columns that point to per-II stdout/stderr, generated architecture files, raw mapper outputs, mapping JSON files, and Gurobi logs.
