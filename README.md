# CGRA Mapper

Toolkit for CGRA mapping experiments.

- Modulo mapping places operations on `(PE, context)` and routes DFG edges.
- 2D placement assigns operations to physical PEs and measures placement quality.
- Experiment manifests compare in-repository mappers and VTR/VPR baselines.

## Setup

```bash
git submodule update --init --recursive
cd environment
docker compose build
docker compose up -d
docker compose exec gurobi bash
cd /home/ubuntu/elastic_cgra_mapper
sh scripts/build.sh
```

Place the Gurobi WLS license at `license_files/gurobi.lic`. Build VPR with
`sh scripts/build_vpr.sh` before running a VPR baseline.

## Run

2D placement:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/placement2d/compare.json
```

Modulo mapping:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/modulo/compare.json
```

Use filters to run one benchmark, architecture, and mapper. See
[Experiments](research/README.md) for commands and configuration.

## Repository

| path | contents |
| --- | --- |
| [`mapper/`](mapper/README.md) | mapper interfaces, registration, and algorithms |
| [`research/`](research/README.md) | experiment manifests, scripts, metrics, and results |
| [`benchmark/`](benchmark/README.md) | native and normalized DFG benchmark sets |
| [`python_tools/`](python_tools/README.md) | DFG and mapping visualizers |
| [`environment/`](environment/) | Docker image and Compose service |

## Guides

- [Experiments and custom comparisons](research/README.md)
- [Mapper implementation and extension](mapper/README.md)
- [Benchmark sets](benchmark/README.md)
- [Direct CLI and visualizer usage](docs/usage.md)
