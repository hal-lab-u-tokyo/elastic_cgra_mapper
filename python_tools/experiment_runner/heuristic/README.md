# Heuristic mapping experiments

Run from the repository root:

```sh
python3 python_tools/experiment_runner/heuristic_mapping_runner.py --manifest /path/to/experiment.json --out /path/to/results
```

Supply experiment manifests, architecture templates and mapper configurations explicitly; research configurations and historical results are no longer bundled. Manifest paths are resolved against `--repo-root` (the working directory by default). Only the in-repository `cgra` runner is supported.

The runner preserves per-case and aggregate `metrics.csv`, the CSV column order, trial expansion and normalized mapping metrics used by the summary reports. Existing VPR-named CSV columns remain empty for schema compatibility; VPR execution and log parsing have been removed. Timing and generated artifact paths vary between runs. JSON summaries and Markdown reports remain available; `--skip-reports` disables the latter. Required Python packages remain networkx and its DOT reader dependency pydot.


Mapper config schema: `Exec.type` selects the mapper; `Algorithm` contains
family-specific defaults. Log paths/timeouts are CLI arguments. Acceptance
policy belongs to ILP, and placement_only belongs only to supporting heuristic
families. Runner overrides update the selected family's object and preserve
unselected defaults. Automatic placement-only requests are omitted for inherently
placement-only engines and legacy wrappers that do not forward the flag.
Explicit unsupported new-schema options are rejected by the mapper reader.
Legacy flat Algorithm inputs preserve their existing effective behavior.
Run `python3 mapper/test/configuration_runner_test.py` from the repository root
for schema/override checks; the mapper README describes the consumption matrix.

Result optimality is recorded in `mapping_status`: optimal, suboptimal,
optimality_unknown or failure. Output JSON's new tag is authoritative; legacy
boolean-only success is conservatively unknown. Solver logs are diagnostics,
and an optimal subproblem log cannot promote a staged/heuristic result to a
global optimum. See the mapper README for tolerance and acceptance-policy details.

CSV appends mapping_status as column159; the original158column order stays
unchanged. The existing status column remains the run verdict (success, failed,
timeout, timeout_feasible, error, etc.), while mapping_status carries optimality.
Pre-execution errors have failure; old success rows without certificates have
optimality_unknown. Invalid explicit tags raise an error instead of being treated
as success. Run `python3 mapper/test/mapping_status_runner_test.py` for the
status/legacy/CSV contract checks.
