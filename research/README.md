# Experiments

An experiment combines a benchmark set, a CGRA architecture, and one or more
mappers in a JSON manifest.

Run commands from the repository root inside the Docker container.

## Run One Case

List the names accepted by a manifest:

```bash
python3 research/scripts/list_manifest_options.py \
  --manifest research/configs/experiments/placement2d/compare.json
```

Check one combination:

```bash
python3 research/scripts/preflight_manifest.py \
  --manifest research/configs/experiments/placement2d/compare.json \
  --out-dir /tmp/cgra_mapper_preflight \
  --only-benchmark-set lisa_sample \
  --only-benchmark atax \
  --only-arch one_hop_perimeter_no_corners_io \
  --only-mapper yott
```

Run it:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/placement2d/compare.json \
  --only-benchmark-set lisa_sample \
  --only-benchmark atax \
  --only-arch one_hop_perimeter_no_corners_io \
  --only-mapper yott \
  --tag trial
```

Remove filters to run every combination in the manifest.

## Configure

| component | location |
| --- | --- |
| Benchmark sets | [`../benchmark/README.md`](../benchmark/README.md) |
| CGRA templates and grid, I/O, network settings | [`configs/arch_templates/README.md`](configs/arch_templates/README.md) |
| Mapper presets | [`configs/mapper/README.md`](configs/mapper/README.md) |
| Manifests, filters, and a custom example | [`configs/experiments/README.md`](configs/experiments/README.md) |
| Metrics | [`docs/experiments/metrics.md`](docs/experiments/metrics.md) |

## Common Manifests

| comparison | manifest |
| --- | --- |
| 2D placement heuristics | `configs/experiments/placement2d/compare.json` |
| 2D placement with ILP | `configs/experiments/placement2d/with_ilp.json` |
| Modulo mapping heuristics | `configs/experiments/modulo/compare.json` |
| Modulo mapping with ILP and VPR | `configs/experiments/modulo/with_ilp.json` |
| YOTT 2021 benchmark set | `configs/experiments/placement2d/literature/yott_2021.json` |

## Results

Without `--out`, each run is written under
`research/results/<result_group>/<timestamp>/`.

| file | contents |
| --- | --- |
| `metrics.csv` | normalized result for every case |
| `benchmark_report.md` | benchmark-level comparison |
| `summary_by_mapper.md` | mapper aggregates |
| `validation.md` | metric consistency checks |
| `routing_validation.md` | routed connectivity checks |
| `run_metadata.json` | command, manifest, and environment metadata |

Regenerate reports with:

```bash
python3 research/scripts/generate_reports.py --result-dir <result_dir>
```

## Reference

- [Experiment rules and result interpretation](docs/experiments/protocol.md)
- [Placement ablations](docs/placement/ablation.md)
- [Algorithms, benchmarks, and routing](docs/README.md)
- [Scripts](scripts/README.md)
- [Adding a mapper](docs/development/adding_mappers.md)
