# Experiment Protocol

## Comparison Conditions

Keep these fixed when comparing mappers:

- input DFG
- architecture template and grid policy
- I/O policy and network model
- II range
- timeout and trial budget
- metric extraction
- `parallel_num = 1`

For stochastic mappers, set `random_seed` explicitly. Change `seed_count`,
`routing_retry_count`, or `max_iterations` only when search effort is the
independent variable.

## Preflight

```bash
python3 research/scripts/preflight_manifest.py \
  --manifest <manifest> \
  --out-dir /tmp/cgra_mapper_preflight
```

A valid case has existing input files, supported operations, enough placement
capacity, and a non-empty II range. Modulo cases also require a computable MII.

## 2D Placement

- II and `context_size` are fixed to 1.
- One DFG node occupies one physical PE.
- The capacity check requires `DFG nodes <= physical PEs`.
- Routing-derived quantities are estimates unless an explicit routed validation
  is enabled.

The standard manifest is
`research/configs/experiments/placement2d/compare.json`. Use
`placement2d/with_ilp.json` when the placement-only ILP baseline is needed.

## Modulo Mapping

For each DFG and architecture:

1. Compute ResMII and RecMII.
2. Start from MII unless the manifest specifies another start II.
3. Generate a CGRA JSON with `context_size = II`.
4. Run one mapper with the fixed timeout.
5. Validate placement and routed DFG connectivity.
6. Stop at the first successful II and record `achieved_II`.

Use `mii: "auto"` for an MII-based sweep. Prefer explicit loop-carried edge
distances in the DOT file. `mii_missing_distance_policy: "self_loop"` treats
current self-loops as distance-1 recurrences; use `strict` when all recurrence
distances are annotated.

The standard manifest is `research/configs/experiments/modulo/compare.json`.
Use `modulo/with_ilp.json` for routed/context-aware ILP and strict VPR
baselines.

| method | solved problem |
| --- | --- |
| `FullRoutingILPMapper` | placement and DFG-edge routing together |
| `ConnectivityPathILPMapper` | path-candidate routing ILP |
| `Placement2DILPMapper` | placement objective only |
| `vpr_sa_routed` | VPR physical placement, context assignment, then CGRA routing |
| `vpr_sa_full_route` | VPR placement and routing on generated modulo resources |

Treat a modulo result as valid only when `routing_validation.md` confirms legal
connections and complete DFG-edge reachability.

## Benchmark Conversion

Use `normalize_benchmarks.py` for CGRA-Bench, GenMap, Revamp, or nonstandard DOT
inputs:

```bash
python3 research/scripts/normalize_benchmarks.py \
  --benchmark-root benchmark \
  --out-dir research/results/benchmark_compatibility/normalized_benchmarks \
  --manifest-out research/results/benchmark_compatibility/normalized_benchmarks/all_normalized_manifest.json \
  --report-out research/results/benchmark_compatibility/normalized_benchmarks/normalization_report.md
```

Normalization maps operations to the mapper's opcode set, removes control-only
nodes, and adds known recurrence distances. It supports mapping evaluation, not
cycle-accurate program simulation. Run preflight on the generated manifest.

## Validation

```bash
python3 research/scripts/validate_metrics.py --help
python3 research/scripts/report_by_benchmark.py --help
```

`validation.md` checks metric consistency and highlights shortcuts such as
`start_II > MII`. `routing_validation.md` checks MRRG legality, route
reachability, routed FIFO, and path length. Inspect benchmark-level results
before aggregate means; failures and routing pressure are often concentrated in
a small number of DFGs.

## Results

Without `--out`, `run_suite.py` writes to
`research/results/<result_group>/<timestamp>/`.

| file | contents |
| --- | --- |
| `metrics.csv` | normalized per-case metrics and raw artifact paths |
| `benchmark_report.md` | per-benchmark comparison |
| `summary_by_mapper.md` | mapper aggregates |
| `summary_by_arch.md` | architecture aggregates |
| `summary_by_set.md` | benchmark-set aggregates |
| `validation.md` | metric checks |
| `routing_validation.md` | routed connectivity checks |
| `run_info.md`, `run_metadata.json` | command, manifest, output, and environment |

Use repeated filters such as `--only-benchmark`, `--only-arch`, and
`--only-mapper` to split a large run without editing the manifest.
