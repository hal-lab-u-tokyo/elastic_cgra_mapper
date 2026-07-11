# Experiment Manifests

A manifest defines the cases expanded and executed by
`research/scripts/run_suite.py`.

```text
manifest
  benchmark_sets[]     DOT directory and benchmark names
  architectures[]      template, grid, I/O, network, and II settings
  mappers[]            C++ mapper presets or external runners
  timeout_sec
  result_group
```

Modulo manifests may also use `mapper_matrix`, `routing_aware_mappers`, and
`external_mappers`.

## Custom 2D Placement Experiment

Create a manifest under `research/configs/experiments/placement2d/` and select
the benchmark, architecture, and mapper entries needed for the comparison:

```json
{
  "name": "placement2d_custom",
  "problem_type": "placement2d",
  "evaluation_mode": "placement_only",
  "mode": "placement2d_fixed_ii",
  "result_group": "placement2d/custom",
  "mapping_bin": "build/mapping",
  "benchmark_sets": [
    {
      "name": "lisa",
      "benchmark_root": "benchmark/literature/traversal_yott_normalized/lisa/dac",
      "benchmarks": ["atax"]
    }
  ],
  "architectures": [
    {
      "name": "one_hop_perimeter_io",
      "template": "research/configs/arch_templates/mesh_10x10_default.json",
      "auto_grid": {"policy": "ceil_sqrt_non_io_plus_2"},
      "memory_io": "perimeter_no_corners",
      "network_type": "one_hop_axis2",
      "ii": 1
    }
  ],
  "mappers": [
    {
      "name": "yott_100",
      "mapper_config": "research/configs/mapper/placement2d/yott.json",
      "algorithm_overrides": {"max_trials": 100}
    }
  ],
  "timeout_sec": 60,
  "parallel_num": 1
}
```

Relative paths are resolved from the repository root. Add entries to any of the
three arrays to form a Cartesian comparison.

## Build A Command

List valid filter names:

```bash
python3 research/scripts/list_manifest_options.py --manifest <manifest>
```

Check a selected case:

```bash
python3 research/scripts/preflight_manifest.py \
  --manifest <manifest> \
  --out-dir /tmp/cgra_mapper_preflight \
  --only-benchmark-set lisa \
  --only-benchmark atax \
  --only-arch one_hop_perimeter_io \
  --only-mapper yott_100
```

Run the same case by replacing `preflight_manifest.py` with `run_suite.py` and
removing `--out-dir`. Filters may be repeated or comma-separated. Without
filters, all cases run.

| filter | selects |
| --- | --- |
| `--only-benchmark-set` | `benchmark_sets[].name` |
| `--only-benchmark` | names inside a selected benchmark set |
| `--only-arch` | `architectures[].name` |
| `--only-mapper` | expanded mapper names |

## Problem Types

- `problem_type: "placement2d"` fixes II to 1 and evaluates physical placement.
- `problem_type: "modulo"` sweeps II from MII to `ii_max` and validates routing.
- `mapper_matrix` crosses placement-first mapper presets with routing policies.

## Architecture Fields

| field | purpose |
| --- | --- |
| `template` | base CGRA JSON from `research/configs/arch_templates/` |
| `auto_grid` | benchmark-dependent grid sizing |
| `memory_io` | I/O-capable PE policy |
| `network_type` | placement distance model |
| `ii` | fixed context count for 2D placement |
| `mii`, `ii_max` | modulo II range |

Architecture values are listed in
[`../arch_templates/README.md`](../arch_templates/README.md). Mapper presets are
listed in [`../mapper/README.md`](../mapper/README.md).

## Runner Types

| entry | execution path |
| --- | --- |
| no `runner` | in-repository C++ mapper through `build/mapping` |
| `runner: "vpr"` | VPR simulated-annealing placement |
| `runner: "vpr_modulo"` | VPR placement, context assignment, then CGRA routing |
| `runner: "vpr_modulo_full_route"` | VPR placement and generated CGRA routing resources |

All runner types use the same result directory and normalized metrics.

## Included Manifests

| comparison | manifest |
| --- | --- |
| Modulo comparison | `modulo/compare.json` |
| Modulo comparison with ILP and VPR | `modulo/with_ilp.json` |
| 2D placement comparison | `placement2d/compare.json` |
| 2D placement with ILP | `placement2d/with_ilp.json` |
| YOTT 2021 cases | `placement2d/literature/yott_2021.json` |

## Add A Mapper

1. Add a preset under `research/configs/mapper/`.
2. Add one entry to a manifest.
3. Run `preflight_manifest.py` on one benchmark.
4. Keep benchmark, architecture, timeout, and metrics fixed while comparing methods.

Implementation steps are in
[`../../docs/development/adding_mappers.md`](../../docs/development/adding_mappers.md).
