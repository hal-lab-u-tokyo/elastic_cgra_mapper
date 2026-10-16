# CGRA Architecture Templates

Architecture templates define the base PE mesh. Experiment manifests can resize
or specialize them per benchmark.

## Templates

| template | use |
| --- | --- |
| `mesh_4x4_default.json` | small ILP and smoke runs |
| `mesh_6x6_default.json` | default modulo search scale |
| `mesh_8x8_default.json` | larger modulo checks |
| `mesh_10x10_default.json` | placement comparisons and auto-grid runs |

## Manifest Knobs

| field | meaning |
| --- | --- |
| `auto_grid.policy` | derive grid size from the benchmark, such as node count or structural I/O count |
| `auto_grid.margin` | add extra rows/columns around the derived grid |
| `memory_io` | choose memory/I/O-capable PEs, such as all PEs or perimeter PEs |
| `network_type` | placement distance model: `mesh` or `one_hop_axis2` |
| `ii` | fixed context count for 2D placement, usually `1` |
| `mii`, `ii_max` | modulo search range |

Common 2D placement settings live in
`research/configs/experiments/placement2d/README.md`.
