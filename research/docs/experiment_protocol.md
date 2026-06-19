# Experiment Protocol

Compare mappers under the same DFG, architecture template, II range, timeout, and metric extraction logic.

Before a long run, run `research/scripts/preflight_manifest.py`. A valid manifest has no missing files, no unsupported operations, a computable MII for each modulo benchmark/architecture pair, and a non-empty II range.

1. Select benchmark `.dot` files from `benchmark/kernel/` or converted CGRA-Bench inputs.
2. Select an architecture template from `research/configs/arch_templates/`.
3. Compute MII from ResMII and RecMII unless a manifest overrides the start II.
4. For each candidate `II` from `MII` to `ii_max`, generate a temporary CGRA JSON with `context_size = II` and `CGRA_type = "default"`.
5. Run `build/mapping` with one mapper config, one DFG, one generated CGRA JSON, one timeout, and `parallel_num = 1`.
6. Stop at the first successful mapping and record it as `achieved_II`.
7. Normalize logs into `metrics.csv` and use the same CSV columns for all mappers.

Standard manifests:

- `research/configs/experiments/modulo/search.json`: modulo heuristic mappers plus a VPR SA placement-seeded routed baseline.
- `research/configs/experiments/modulo/all_mappers.json`: modulo mappers plus routed/context-aware ILP mappers and strict VPR routed baselines.
- `research/configs/experiments/placement2d/search.json`: 2D placement mappers only.
- `research/configs/experiments/placement2d/all_mappers.json`: 2D placement mappers plus the placement-only ILP baseline.

The modulo search manifest uses a 6x6 default CGRA and native kernel benchmarks. The placement2d search manifest uses a representative LISA/m_bench subset with cpu_mapping-style auto grid sizing, perimeter I/O without corners, the 1-hop cost model, and placement-only evaluation.

For 2D placement, one physical PE is one placement slot, context size/II is fixed to 1, and the runner checks `DFG nodes <= physical PEs`.

For stochastic placement-first mappers, keep the per-II timeout fixed and control search effort from the mapper config. Use explicit `random_seed`; increase `seed_count`, `routing_retry_count`, or `max_iterations` when measuring success rate.

Use the `all_mappers.json` manifests when ILP mappers should be included. Switch settings by changing only the `--manifest` argument to `run_suite.py`.

`FullRoutingILPMapper` solves placement and DFG-edge routing together. `ConnectivityPathILPMapper` solves a path-candidate routing ILP. `PlacementOnlyILPMapper` solves only the placement objective. `vpr_sa_routed` uses VPR for physical placement, then this repository assigns contexts and routes on the CGRA MRRG. `vpr_sa_full_route` uses VPR placement over PE/context slots, a generated CGRA modulo RR graph, VPR routing, and route import into the CGRA mapping format. Always inspect `routing_validation.md` before treating placement-first or imported-route results as route-correct.

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

`run_suite.py` creates `benchmark_report.md`, `summary_by_mapper.md`, `summary_by_arch.md`, `summary_by_set.md`, `validation.md`, and `routing_validation.md` unless `--skip-reports` is passed. `validation.md` checks metric consistency. `routing_validation.md` checks legal MRRG connections, DFG-edge route reachability, same-context/cross-context route counts, routed FIFO, route-path length, and routed mapped LP.

If `run_suite.py --out` is omitted, the result directory is created automatically under `research/results/<result_group>/<timestamp>`. A manifest can set `result_group`, and otherwise the manifest filename is used. Every run writes `run_metadata.json` and `run_info.md`, and `metrics.csv` includes columns that point to per-II stdout/stderr, generated architecture files, raw mapper outputs, mapping JSON files, and Gurobi logs.
