# Experiment Scripts

Run commands from the repository root.

## Run

```bash
python3 research/scripts/list_manifest_options.py --manifest <manifest>
python3 research/scripts/preflight_manifest.py \
  --manifest <manifest> --out-dir /tmp/cgra_mapper_preflight
python3 research/scripts/run_suite.py --manifest <manifest>
```

| command | purpose |
| --- | --- |
| `list_manifest_options.py` | list benchmark, architecture, and mapper filter names |
| `preflight_manifest.py` | check paths, capacity, MII, operations, and filters |
| `run_suite.py` | run all or selected manifest cases |
| `generate_reports.py` | rebuild reports from an existing result directory |
| `normalize_benchmarks.py` | convert benchmark inputs to mapper-ready DOT files |

## Directories

```text
research/scripts/
  run_suite.py             experiment runner
  preflight_manifest.py    manifest checks
  generate_reports.py      report entry point
```

## Supporting Commands

| group | commands |
| --- | --- |
| Metrics | `collect_metrics.py`, `validate_metrics.py`, `report_by_benchmark.py`, `compare_results.py` |
| Routing | `validate_mapping_routes.py`, `run_vpr_modulo_routing.py`, `run_vpr_modulo_full_routing.py` |
| VPR placement | `run_vpr_baseline.py` |
| Conversion | `blif_to_dot.py`, `vpr_net_to_dot.py`, `normalize_mapping_result.py` |
| Architecture and II | `make_arch_for_ii.py`, `compute_mii.py` |

`run_mapper_case.py` executes one expanded case. `lib.py` contains shared DOT,
architecture, metric, path, and reporting helpers.
