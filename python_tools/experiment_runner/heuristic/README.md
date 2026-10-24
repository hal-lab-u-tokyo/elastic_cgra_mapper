# Heuristic mapping experiments

Run from the repository root:

```sh
python3 python_tools/experiment_runner/heuristic_mapping_runner.py --manifest /path/to/experiment.json --out /path/to/results
```

Supply experiment manifests, architecture templates and mapper configurations explicitly; research configurations and historical results are no longer bundled. Manifest paths are resolved against `--repo-root` (the working directory by default). Only the in-repository `cgra` runner is supported.

The runner preserves per-case and aggregate `metrics.csv`, the CSV column order, trial expansion and normalized mapping metrics used by the summary reports. Existing VPR-named CSV columns remain empty for schema compatibility; VPR execution and log parsing have been removed. Timing and generated artifact paths vary between runs. JSON summaries and Markdown reports remain available; `--skip-reports` disables the latter. Required Python packages remain networkx and its DOT reader dependency pydot.
