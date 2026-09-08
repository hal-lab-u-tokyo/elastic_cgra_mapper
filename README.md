# Elastic CGRA Mapper

Mapping tool for Elastic CGRAs.

## Requirements

- GCC 8.5.0 or later
- Gurobi 9.1.1
- CMake 3.20.2 or later
- Ninja
- Python 3 and the Python packages installed by the project Docker image

The commands below assume that the repository is mounted at
`/home/ubuntu/elastic_cgra_mapper` in the project Docker container.

## Setup

Clone this repository, download a Gurobi WLS license from the
[Web License Manager](https://license.gurobi.com/manager/licenses), and place it
in `license_files/`. Then build and start the container:

```bash
cd environment
docker compose build
docker compose up -d
```

Run the remaining commands from the repository root in the container:

```bash
cd /home/ubuntu/elastic_cgra_mapper
```

## Scripts

### `scripts/build.sh`

Purpose: configure the C++ project with CMake and Ninja, then build all targets
under `build/`.

Prerequisites: the compiler, CMake, Ninja, Gurobi, and other project dependencies
must be available in the container. Set `DEBUG_ENABLE=1` near the top of the
script when a Debug build is required; its default is `0`.

Inputs: none.

```bash
sh scripts/build.sh
```

Principal outputs: the `build/` directory and executables such as
`build/mapping` and `build/remapping`.

### `scripts/test.sh`

Purpose: configure and build the C++ project, run all CTest tests verbosely, and
then run the Python unit-test suite.

Prerequisites: the same build dependencies as `scripts/build.sh`; Python test
dependencies must also be installed.

Inputs: none.

```bash
sh scripts/test.sh
```

Principal outputs: updated build artifacts in `build/` and test results printed
to the terminal. A non-zero exit status indicates a build or test failure.

### `scripts/exec_mapping_experiment.sh`

Purpose: run the mapping experiment matrix described by
`data/experiment_runner/mapping_config.json`.

Prerequisites: build the project first. Before running the experiment, review
the configuration, especially its benchmark list, CGRA settings, timeout,
process count, mapper configuration path, and output directory.

Inputs: the script takes no command-line arguments; edit
`data/experiment_runner/mapping_config.json` to define the experiment.

```bash
sh scripts/build.sh
sh scripts/exec_mapping_experiment.sh
```

Principal outputs: a timestamped directory at
`output/experiments/<date>/mapping/`, containing the copied configuration, log,
and mapping results.

### `scripts/exec_remapper_experiment.sh`

Purpose: create or reuse a mapping database and run the remapping experiment
matrix described by `data/experiment_runner/remapper_config.json`.

Prerequisites: build the project first. Review the configuration before use,
especially `create_database`, `database_path`, benchmark and CGRA settings,
timeouts, process count, remapper modes, and available-mapping counts. When
`create_database` is `false`, `database_path` must refer to an existing database.

Inputs: the script takes no command-line arguments; edit
`data/experiment_runner/remapper_config.json` to define the experiment.

```bash
sh scripts/build.sh
sh scripts/exec_remapper_experiment.sh
```

Principal outputs: a timestamped directory at `output/experiments/<date>/`,
including `remapper_config.json`, an experiment log, and remapper results under
`remapper/`. When database creation is enabled, the generated database is under
the same experiment directory.

### `scripts/analyze.sh`

Purpose: analyze the remapper results of one experiment using
`data/analyzer/plotter_config.json`.

Prerequisites: the selected experiment must contain remapper results, and the
Python analysis dependencies must be installed.

Input: one experiment directory, for example a directory created by
`scripts/exec_remapper_experiment.sh`.

```bash
sh scripts/analyze.sh output/experiments/2026-09-08-12-00-00
```

Replace the example timestamp with an existing directory name.

Principal outputs: `output/experiments/<date>/remapper/analysis/`, including
`remapper_result.csv`, `remapper_failed_results.csv`, and configured plots.
`remapper_failed_results.csv` is the input for the remapper debugging workflow
below.

### `scripts/debug_remapper_experiment.sh`

Purpose: re-execute only the failed remapper cases listed in a failure CSV,
generate VS Code launch configurations for those cases, and analyze the new
results. This is the TAS-351/TAS-354 iterative debugging workflow.

Prerequisites: build `build/remapping` first and analyze the original experiment
so that `remapper_failed_results.csv` exists. The CSV must be below
`output/experiments/<experiment-date>/` or below a previous debug run that has
`debug_metadata.json`.

Inputs:

1. Required: a failed-results CSV.
2. Optional: a plotter configuration path. The default is
   `data/analyzer/plotter_config.json`.

Start with the failures from an analyzed experiment:

```bash
sh scripts/debug_remapper_experiment.sh \
  output/experiments/2026-09-08-12-00-00/remapper/analysis/remapper_failed_results.csv
```

Replace the example timestamp with an existing experiment directory. To use a
different plotter configuration:

```bash
sh scripts/debug_remapper_experiment.sh \
  output/experiments/2026-09-08-12-00-00/remapper/analysis/remapper_failed_results.csv \
  data/analyzer/plotter_config.json
```

Principal outputs: a new `debug/<experiment-date>/<debug-date>/` directory with:

- `input_failed_results.csv`: an immutable copy of the CSV used for this run;
- `debug_metadata.json`: the original experiment path/date and input CSV path;
- `debug.log`: the re-execution log;
- `remapper/analysis/launch.json`: one VS Code debug configuration per case;
- `remapper/analysis/remapper_result.csv` and `remapper_failed_results.csv`;
- `failed_results.csv`: a convenient copy of the remaining failures.

After fixing a problem, feed the latest `failed_results.csv` back into the same
script. The metadata links the new run to the original experiment automatically:

```bash
sh scripts/debug_remapper_experiment.sh \
  debug/2026-09-08-12-00-00/2026-09-08-13-00-00/failed_results.csv
```

Each iteration creates a new timestamped directory under the same
`debug/<experiment-date>/` directory. If the input CSV contains only its header,
the script reports that there are no remaining failures and exits successfully.

## Direct executable usage

After building, the mapper can also be invoked directly:

```bash
./build/mapping input.dot input_arch.json output_mapping.json
```

## Visualizer

From `python_tools/visualizer`, create a DOT image or visualize a mapping result:

```bash
cd python_tools/visualizer
dot -Tpng input.dot -o output.png
python3 mapping_visualize_main.py input_mapping.json
```
