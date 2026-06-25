# 2D Placement Manifests

Use these manifests for 2D placement experiments:

- `search.json`: YOTO/YOTT, PRISA, and VPR SA on a representative benchmark mix.
- `all_mappers.json`: `search.json` plus placement-only ILP.
- `ilp_yoto_yott.json`: focused ILP/YOTO/YOTT comparison on an ILP-tractable subset.

Paper-comparison manifests:

- `paper_comparison/traversal_yott.json`
- `paper_comparison/yott_cases2021.json`
- `paper_comparison/prisa_vpr8.json`

Grid and I/O settings:

- `ceil_sqrt_non_io_plus_2`: choose a square grid from non-I/O operation count, then add one border PE ring for I/O.
- `ceil_sqrt_nodes_plus_2`: choose a square grid from all DFG nodes, then add one border PE ring for I/O.
- `perimeter_no_corners`: allow I/O only on non-corner border PEs.
- `memory_io: "all"`: allow every PE to host memory/I/O operations.
- `network_type: "one_hop_axis2"`: use the one-hop fully pipelined placement-cost model.
- `network_type: "mesh"`: use Manhattan mesh distance.

Use `probes/` for diagnostics and ablations. Use `archive/` only for older manifests.
