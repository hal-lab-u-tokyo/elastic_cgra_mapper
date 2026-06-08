# CGRA Mapper

Mapping and remapping tools for CGRA architectures. This repository can evaluate both modulo-style (`default`) and elastic/dataflow-style (`elastic`) CGRA models.

## Requirements

The recommended way to run this repository is through Docker Compose. The container is currently confirmed with:

- GCC >= 8.5.0
- Gurobi >= 9.5.1
- CMake >= 3.20.2

The Docker image uses Gurobi 9.5.2 by default. The Compose service pins `linux/amd64`, so it also runs on Apple Silicon hosts through Docker Desktop's emulation layer.

## Setup

Initialize the required submodules:

```bash
git submodule update --init --recursive third_party/boost third_party/googletest
git submodule update --init --recursive benchmark/CGRA-Bench
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

Note: `data/mapper_config.json` currently uses `ILPMapper`. The `ILPMapper` implementation can run both CGRA types, but its elastic-specific constraints are still marked TODO. Use `PlacementILPMapper` when evaluating the `elastic` model with race-condition avoidance and context-collapsing constraints.

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

## Benchmarks And CGRA-Bench

The mapper evaluation uses `.dot` files under:

```text
benchmark/kernel/
```

CGRA-Bench is available as a submodule under:

```text
benchmark/CGRA-Bench/
```

Some CGRA-Bench `.dot` files can be converted with:

```bash
python3 benchmark/converter/cgra-bench_converter.py \
  benchmark/CGRA-Bench/evaluation/fft_pro.dot \
  /tmp/fft_pro_converted.dot
```

Use the converted `.dot` file as a mapper input, or place it under `benchmark/kernel/` and add its filename without `.dot` to `benchmark_name` in `data/experiment_runner/mapping_config.json`.

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
dot -Tpng <input.dot> -o <output.png>
```

Visualize a mapping result:

```bash
cd python_tools/visualizer
python3 mapping_visualize_main.py <mapping.json>
```
