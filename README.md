# CGRA Mapper

Mapping and remapping tools for CGRA architectures. This repository can evaluate both modulo-style (`default`) and elastic/dataflow-style (`elastic`) CGRA models.

## Requirements

The recommended way to run this repository is through Docker Compose. The container is currently confirmed with:

- GCC >= 8.5.0
- Gurobi >= 9.5.1
- CMake >= 3.20.2
- Graphviz
- VTR/VPR v8.0.0, optional for the external VPR placement baseline

The Docker image uses Gurobi 9.5.2 by default. The Compose service pins `linux/amd64`, so it also runs on Apple Silicon hosts through Docker Desktop's emulation layer.

## Setup

Initialize the required submodules:

```bash
git submodule update --init --recursive third_party/boost third_party/googletest
git submodule update --init --recursive benchmark/CGRA-Bench
```

For the optional VPR baseline, also initialize VTR:

```bash
git submodule update --init --recursive third_party/vtr
```

Download a Gurobi WLS license from [Web License Manager](https://license.gurobi.com/manager/licenses), then place it at:

```text
license_files/gurobi.lic
```

Build and start the container:

```bash
cd environment
docker compose build
docker compose up -d
```

Open a shell inside the container:

```bash
docker compose exec gurobi bash
```

All project commands below assume the container shell and this working directory:

```bash
cd /home/ubuntu/elastic_cgra_mapper
```

You can also run one-off commands from the host with `docker compose exec gurobi ...` while your current directory is `environment/`.

## Build And Test

Build:

```bash
sh scripts/build.sh
```

Build the optional VPR baseline:

```bash
sh scripts/build_vpr.sh
```

Run tests:

```bash
sh scripts/test.sh
```

The main generated executables are:

- `build/mapping`
- `build/remapping`
- `build/create_database`
- `build/bulk_mapping`

## Single Mapping Run

`build/mapping` currently takes six arguments:

```bash
./build/mapping \
  <dfg_dot_file> \
  <cgra_json_file> \
  <output_dir> \
  <mapper_config_json> \
  <timeout_seconds> \
  <parallel_num>
```

Example:

```bash
./build/mapping \
  /home/ubuntu/elastic_cgra_mapper/benchmark/kernel/fixed_fft_pro.dot \
  /home/ubuntu/elastic_cgra_mapper/data/CGRA/test_cgra.json \
  /home/ubuntu/elastic_cgra_mapper/output/manual \
  /home/ubuntu/elastic_cgra_mapper/data/mapper_config.json \
  60 \
  1
```

The input paths and output directory must be absolute paths. Results are written under:

```text
<output_dir>/mapping/<run_id>/
```

Typical files in each run directory:

- `input_log_<run_id>.json`: input DFG, CGRA setting, timeout, parallel count
- `output_log_<run_id>.json`: success flag, mapping time, output file paths
- `mapping_<run_id>.json`: mapping result
- `cgra_<run_id>.json`: copied/generated CGRA architecture
- `gurobi_log_<run_id>.log`: Gurobi solver log

## CGRA Types

This repository uses two CGRA types:

- `default`: the modulo-style CGRA model. Spatial connections advance to the next context, `(k + 1) % context_size`.
- `elastic`: the elastic/dataflow-style CGRA model. Spatial connections can target any context, giving the mapper more timing flexibility.

To evaluate an ordinary modulo-style CGRA, use `CGRA_type: "default"` in a CGRA JSON file, or set the experiment config to:

```json
"cgra_type": ["default"]
```

To compare both models:

```json
"cgra_type": ["default", "elastic"]
```

Note: `data/mapper_config.json` currently uses `ILPMapper`. The `ILPMapper` implementation can run both CGRA types, but its elastic-specific constraints are still marked TODO. Use `ILPPlacementMapper` when evaluating the `elastic` model with race-condition avoidance and context-collapsing constraints.

`Algorithm.accept_feasible_solution` controls how Gurobi statuses are handled. Set it to `true` to accept a feasible incumbent found before a timeout, or `false` to accept only `GRB_OPTIMAL`/`GRB_SUBOPTIMAL` results.

## Mapper Evaluation

Mapper batch evaluation is driven by:

```text
data/experiment_runner/mapping_config.json
```

Run it with:

```bash
sh scripts/exec_mapping_experiment.sh
```

This script calls `python_tools/experiment_runner/mapping_runner.py`, which builds all combinations from the config and launches `build/mapping`.

The main config fields are:

- `exec_setting.kernel_dir_path`: directory containing benchmark `.dot` files
- `exec_setting.output_dir_path`: base output directory
- `exec_setting.timeout_s`: timeout for each mapping run
- `exec_setting.process_num`: number of parallel worker processes
- `exec_setting.mapper_config_path`: mapper algorithm config
- `cgra_settings.cgra_type`: CGRA type list, such as `default` or `elastic`
- `cgra_settings.cgra_size.min/max`: square CGRA size range
- `cgra_settings.memory_io`: memory I/O modes, such as `all` and `both_ends`
- `cgra_settings.network_type`: network type list
- `benchmark_name`: benchmark names without the `.dot` suffix

Results are written under:

```text
output/experiments/<datetime>/mapping/
```

The default config is intended for longer experiments. For a quick smoke test, use a small temporary config, for example by reducing these values:

```json
{
  "exec_setting": {
    "timeout_s": 60,
    "process_num": 1
  },
  "cgra_settings": {
    "memory_io": ["all"]
  },
  "benchmark_name": ["convolution_no_loop"]
}
```

Keep the rest of the existing JSON fields unchanged.

## Research Mapping Evaluation

For research on new mapping algorithms, use the `research/` workflow. It keeps experiment manifests, architecture templates, mapper configs, metrics scripts, and generated results separate from the older batch runner.

The standard modulo-style evaluation treats `context_size` as the candidate II, forces `CGRA_type: "default"`, keeps `parallel_num: 1`, and tries `II = MII, MII + 1, ...` until the first successful mapping is found. That first success is reported as `achieved_II`.

Use one of the standard manifests:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/modulo/search.json
```

Available manifests:

- `research/configs/experiments/modulo/search.json`: modulo mappers only; quick iteration.
- `research/configs/experiments/modulo/all_mappers.json`: modulo mappers plus ILP mappers.
- `research/configs/experiments/placement2d/search.json`: 2D placement mappers only; quick iteration.
- `research/configs/experiments/placement2d/all_mappers.json`: 2D placement mappers plus ILP mappers.

Run preflight before long runs:

```bash
python3 research/scripts/preflight_manifest.py \
  --manifest research/configs/experiments/modulo/all_mappers.json \
  --repo-root /home/ubuntu/elastic_cgra_mapper \
  --out-dir research/results/preflight/modulo_all_mappers
```

Regenerate reports for an existing result directory:

```bash
python3 research/scripts/generate_reports.py \
  --result-dir <result_dir>
```

The primary output is `metrics.csv`. Important columns are `MII`, `start_II`, `achieved_II`, `II_ratio`, `status`, `mapping_time_sec`, `wall_time_sec`, `compute_pe_utilization`, `pe_context_utilization`, `route_to_compute_ratio`, `avg_manhattan_distance`, `compute_bbox_utilization`, `objective_value`, `best_bound`, and `mip_gap`.

Experiment manifests can either use a single `benchmark_root` plus `benchmarks`, or multiple `benchmark_sets`. Multiple sets are useful when comparing built-in kernels, converted CGRA-Bench kernels, and custom benchmarks in one run while keeping the result rows separable by `benchmark_set`.

Use `research/scripts/normalize_benchmarks.py` when a benchmark collection is not already in the mapper's DOT format. It scans `.dot` and Revamp `.xml` files, writes mapper-compatible `.dot` files, and can generate a research manifest for the normalized benchmark set.

```bash
python3 research/scripts/normalize_benchmarks.py \
  --benchmark-root benchmark \
  --out-dir research/results/benchmark_compatibility/normalized_benchmarks \
  --manifest-out research/results/benchmark_compatibility/normalized_benchmarks/all_normalized_manifest.json \
  --report-out research/results/benchmark_compatibility/normalized_benchmarks/normalization_report.md
```

Then preflight and run the generated manifest:

```bash
python3 research/scripts/preflight_manifest.py \
  --manifest research/results/benchmark_compatibility/normalized_benchmarks/all_normalized_manifest.json \
  --repo-root /home/ubuntu/elastic_cgra_mapper \
  --out-dir research/results/benchmark_compatibility/normalized_benchmarks/preflight

python3 research/scripts/run_suite.py \
  --manifest research/results/benchmark_compatibility/normalized_benchmarks/all_normalized_manifest.json \
  --out research/results/benchmark_compatibility/all_normalized_ilp_probe

python3 research/scripts/generate_reports.py \
  --result-dir research/results/benchmark_compatibility/all_normalized_ilp_probe
```

New mapping algorithms should derive from `mapper::IMapper` and register their `Algorithm.type` string with the mapper registry. This keeps mapper selection string-based and avoids editing enum or JSON parser code for each research prototype.

For modulo algorithms, add mapper configs to `research/configs/experiments/modulo/search.json` or `research/configs/experiments/modulo/all_mappers.json`. For 2D placement algorithms, use `research/configs/experiments/placement2d/search.json` or `research/configs/experiments/placement2d/all_mappers.json`. Each run records `run_metadata.json`, `run_info.md`, metrics, per-II logs, mapping files, Gurobi logs, and `routing_validation.md`.

See `research/README.md` and `research/docs/` for the experiment protocol, MII definition, metric meanings, and the recommended next steps for adding a new mapper implementation.

For YOTT/PRISA-style placement research, start with `research/docs/placement_research_plan.md`.

## Benchmarks And CGRA-Bench

The mapper consumes DFG `.dot` files with `opcode` attributes. Native mapper-ready examples are under:

```text
benchmark/kernel/
benchmark/cgrame_kernel/
benchmark/parallel/
```

Other benchmark assets are available under:

- `benchmark/CGRA-Bench/evaluation/`: CGRA-Bench DOTs and prior evaluation artifacts
- `benchmark/CGRA-Bench/kernels/`: C/C++ source kernels and a few generated DOTs
- `benchmark/GenMap/`: GenMap DOTs with different opcode names
- `benchmark/revamp_kernel/`: Revamp XML DFGs

Use `research/scripts/normalize_benchmarks.py` to convert the in-repository DOT/XML benchmark files into the current mapper format before research runs. The normalizer currently handles CGRA-Bench evaluation DOTs, CGRA-Bench kernel DOTs, GenMap DOTs, Revamp XMLs, and the native DOTs already used by the mapper.

Example:

```bash
python3 research/scripts/normalize_benchmarks.py \
  --benchmark-root benchmark \
  --out-dir research/results/benchmark_compatibility/normalized_benchmarks \
  --manifest-out research/results/benchmark_compatibility/normalized_benchmarks/all_normalized_manifest.json \
  --report-out research/results/benchmark_compatibility/normalized_benchmarks/normalization_report.md
```

The generated manifest is intended as a health-check baseline. It uses `mesh6x6_default_all`, `mii: "auto"`, `ii_max: 24`, `timeout_sec: 3`, and `ilp_mapper`. In the latest check, all 41 DOT/XML benchmark inputs under `benchmark/` normalized successfully, preflight passed for all 41, and the generated ILP probe produced a feasible or optimal mapping for all 41. Because the probe timeout is short, many rows are `timeout_feasible`; use longer timeouts for publication-quality comparisons.

The normalizer is for mapping, placement, and routing evaluation. It folds unsupported source-level operations into mapper-supported resource classes, drops control-only branch nodes, maps `phi` to `const`, and annotates backward dependencies with `distance=1` for RecMII. It does not turn CGRA-Bench C/C++ source files, JSON metadata, images, or prior `.map` files into new DFGs; those are source data or previous outputs rather than direct `build/mapping` inputs.

## Remapper Evaluation

Remapper experiments are configured by:

```text
data/experiment_runner/remapper_config.json
```

Run:

```bash
sh scripts/exec_remapper_experiment.sh
```

Analyze a remapper experiment directory with:

```bash
sh scripts/analyze.sh /home/ubuntu/elastic_cgra_mapper/output/experiments/<datetime>
```

The analyzer expects a remapper output directory, not a mapper-only experiment.

## Visualizer

Visualize a DFG `.dot` file:

```bash
cd python_tools/visualizer
python3 dfg_visualize_main.py <input.dot>
```

Visualize a mapping result:

```bash
cd python_tools/visualizer
python3 mapping_visualize_main.py <mapping.json>
```

Visualizer outputs are written under `python_tools/visualizer/output/`.
