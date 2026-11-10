# Usage

## Container

The Docker image provides GCC, CMake, Graphviz, Gurobi, and the Python dependencies used by the runners. The Compose service pins `linux/amd64`, so Apple Silicon hosts run it through Docker Desktop emulation.

From the host:

```bash
cd environment
docker compose up -d
docker compose exec gurobi bash
cd /home/ubuntu/elastic_cgra_mapper
```

## Build Outputs

Build:

```bash
sh scripts/build.sh
```

Generated executables:

- `build/mapping`
- `build/remapping`
- `build/create_database`
- `build/bulk_mapping`

## Single Mapping Run

`build/mapping` uses named options (positional arguments are not supported):

```bash
./build/mapping \
  --dfg_file <dfg_dot_file> \
  --cgra_file <cgra_json_file> \
  --output_dir <output_dir> \
  --mapper_config <mapper_config_json> \
  --timeout_s <timeout_seconds> \
  --parallel_num <parallel_num>
```

Example:

```bash
./build/mapping \
  --dfg_file /home/ubuntu/elastic_cgra_mapper/benchmark/kernel/fixed_fft_pro.dot \
  --cgra_file /home/ubuntu/elastic_cgra_mapper/data/CGRA/test_cgra.json \
  --output_dir /home/ubuntu/elastic_cgra_mapper/output/manual \
  --mapper_config /home/ubuntu/elastic_cgra_mapper/data/mapper_config.json \
  --timeout_s 60 \
  --parallel_num 1
```

Results are written under:

```text
--output_dir <output_dir>/mapping/<run_id>/
```

Typical files:

- `input_log_<run_id>.json`
- `output_log_<run_id>.json`
- `mapping_<run_id>.json`
- `cgra_<run_id>.json`
- `gurobi_log_<run_id>.log`

## CGRA Model

CGRA JSON files use `CGRA_type`:

- `default`: modulo-style CGRA. Spatial connections advance to `(k + 1) % context_size`.
- `elastic`: dataflow-style CGRA. Spatial connections can target any context.

Use `default` for ordinary modulo experiments.

## Batch Runner

Configure batch mapping with:

```text
data/experiment_runner/mapping_config.json
```

Run:

```bash
sh scripts/exec_mapping_experiment.sh
```

For algorithm comparisons, use `python_tools/experiment_runner/heuristic_mapping_runner.py`; it records normalized metrics, validation reports, and run metadata.

## Remapper

Remapper experiments are configured by:

```text
data/experiment_runner/remapper_config.json
```

Run:

```bash
sh scripts/exec_remapper_experiment.sh
```

Analyze a remapper output directory:

```bash
sh scripts/analyze.sh /home/ubuntu/elastic_cgra_mapper/output/experiments/<datetime>
```

## Visualizer

Visualize a mapping result:

```bash
cd python_tools/visualizer
python3 mapping_visualize_main.py <mapping.json>
```

Outputs are written under `python_tools/visualizer/output/`.
