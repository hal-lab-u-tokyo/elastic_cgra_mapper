# Python Tools

Visualization and support utilities. Use `research/scripts/` for experiment
runs.

## Directories

| path | use |
| --- | --- |
| `entity/` | Python data classes for CGRA, mapping, and log files |
| `io_lib/` | JSON readers and plot/remapper config loaders |
| `visualizer/` | DFG and mapping visualization CLIs |
| `experiment_runner/` | mapping/remapping runner wrappers |
| `analyzer/` | log analysis utility |

## Visualizers

```bash
cd python_tools/visualizer
python3 dfg_visualize_main.py
python3 mapping_visualize_main.py
```

Generated figures are written under `python_tools/visualizer/output/` and are
ignored by git.
