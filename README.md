# CGRA Mapper

Toolkit for CGRA mapping experiments.

- **Modulo mapping**: place operations on `(PE, context)` and route DFG edges.
- **2D placement**: place operations on physical PEs and compare placement quality.
- **Evaluation**: run mapper suites, normalize benchmarks, generate reports, and use VTR/VPR baselines.

## Setup

```bash
git submodule update --init --recursive third_party/boost third_party/googletest
git submodule update --init --recursive benchmark/CGRA-Bench
git submodule update --init --recursive third_party/vtr
```

Place a Gurobi WLS license at:

```text
license_files/gurobi.lic
```

Build and enter the container:

```bash
cd environment
docker compose build
docker compose up -d
docker compose exec gurobi bash
cd /home/ubuntu/elastic_cgra_mapper
```

Build:

```bash
sh scripts/build.sh
```

Build VPR:

```bash
sh scripts/build_vpr.sh
```

Test:

```bash
sh scripts/test.sh
```

## Run Experiments

Modulo:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/modulo/search.json
```

2D placement:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/placement2d/search.json
```

- Preflight: `research/scripts/preflight_manifest.py`
- Reports: `research/scripts/generate_reports.py`


## Add a Mapper

Add C++ code under the matching problem directory:

- `mapper/src/placement2d/` and `mapper/include/mapper/placement2d/` for 2D placement.
- `mapper/src/modulo/` and `mapper/include/mapper/modulo/` for modulo placement and routing.

Each mapper derives from `mapper::IMapper`, registers an `Algorithm.type`, and gets a JSON preset under `research/configs/mapper/`.

Implementation checklist: `research/docs/mapper_extension_guide.md`.

## Benchmarks

Mapper-ready DOT files are under:

```text
benchmark/kernel/
benchmark/cgrame_kernel/
benchmark/parallel/
benchmark/literature/
```

Normalize CGRA-Bench, GenMap, Revamp, or nonstandard DOT files with `research/scripts/normalize_benchmarks.py`.

## Repository Layout

| path | responsibility |
| --- | --- |
| `mapper/` | C++ mapper interfaces and implementations |
| `entity/`, `io/`, `util/` | core data structures, JSON/DOT I/O, helper code |
| `research/configs/` | experiment manifests, mapper presets, architecture templates |
| `research/scripts/` | runners, preflight checks, normalization, reports, validation |
| `research/docs/` | metric definitions, reproduction notes, extension guides |
| `benchmark/` | native and imported benchmark sets |
| `python_tools/visualizer/` | DFG and mapping visualizers |
| `environment/` | Docker image and Compose service |

## Documentation

- `docs/usage.md`: direct mapper CLI, batch runner, remapper, visualizer.
- `research/README.md`: research runner and outputs.
- `research/docs/metrics.md`: metric definitions.
- `research/docs/mii_definition.md`: MII, ResMII, and RecMII.
- `research/docs/traversal_yott_reproduction.md`: TRAVERSAL/YOTT reproduction notes.
- `research/docs/prisa_reproduction.md`: PRISA reproduction notes.
- `research/docs/vpr_modulo_routing.md`: VPR placement/routing integration.
- `research/docs/placement_research_plan.md`: placement-to-routing research direction.
