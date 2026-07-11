# Mapper Presets

Each JSON file selects one registered C++ mapper and its default parameters.
Experiment manifests may change runtime parameters with `algorithm_overrides`.

## 2D Placement

| preset | mapper |
| --- | --- |
| `placement2d/yoto.json` | YOTO |
| `placement2d/yott.json` | YOTT |
| `placement2d/yott_core.json` | YOTT Core |
| `placement2d/yott_core_repair.json` | YOTT Core Repair |
| `placement2d/ilp.json` | placement-only ILP |
| `placement2d/prisa.json` | PRISA-style array mapper |
| `placement2d/prisa_no_sis.json` | PRISA-style array mapper without SIS |

The 2D YOTO/YOTT implementations were developed with reference to the
[YOTT paper](https://doi.org/10.1145/3477038) and the authors'
[`cpu_mapping` repository](https://github.com/canesche/cpu_mapping). The
correspondence and remaining differences are documented in
[`../../docs/placement/yott.md`](../../docs/placement/yott.md).

The PRISA-style presets remain experimental; matching the reported paper
results has not been established.

## Modulo Mapping

| preset | mapper |
| --- | --- |
| `modulo/physical_yoto.json` | 2D YOTO, context assignment, and routing |
| `modulo/physical_yott.json` | 2D YOTT, context assignment, and routing |
| `modulo/yoto.json` | direct-context YOTO |
| `modulo/yott.json` | direct-context YOTT |
| `modulo/sa.json` | simulated annealing |
| `modulo/placement_first.json` | placement-first BFS mapper |
| `modulo/prisa.json` | PRISA-style placement followed by BFS routing |
| `modulo/prisa_manhattan.json` | PRISA-style placement and Manhattan routing |
| `modulo/full_routing_ilp.json` | integrated placement and routing ILP |
| `modulo/connectivity_path_ilp.json` | path-candidate routing ILP |

Their routing combinations are defined in
[`../experiments/modulo/README.md`](../experiments/modulo/README.md).
The modulo YOTO/YOTT presets are extensions in this repository, not algorithms
published directly in the cited YOTT sources.
