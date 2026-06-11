# Research Results Layout

Generated experiment results are ignored by git, but the directory structure is kept stable so runs are easy to revisit.

Recommended layout:

- `algorithm_design/algorithm_design_compare/<YYYYMMDD-HHMMSS[_tag]>/`: standard short comparison runs used while designing new mapping algorithms.
- `baselines/`: older sanity, smoke, and placement baseline runs.
- `benchmark_compatibility/`: benchmark normalization and compatibility probes.
- `preflight/`: manifest preflight reports.

For new algorithm work, run:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/algorithm_design_compare.json
```

The result directory is created automatically under `algorithm_design/algorithm_design_compare/`. Each run contains `run_info.md`, `run_metadata.json`, `metrics.csv`, standard summary reports, and per-II logs.
