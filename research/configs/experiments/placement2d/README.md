# 2D Placement Manifests

Use these first:

- `search.json`: normal 2D placement search with YOTO/YOTT 1000, PRISA, PRISA without SIS, and VPR SA baselines.
- `all_mappers.json`: the same search setting plus placement-only ILP.

Grid and I/O conventions:

- `ceil_sqrt_non_io_plus_2`: choose a square grid from non-I/O operation count, then add one border PE ring for perimeter I/O.
- `perimeter_no_corners`: only non-corner border PEs can host memory/input/output operations. This is useful for TRAVERSAL/YOTT-style 2D placement with peripheral I/O.
- Use `memory_io: "all"` for PRISA VPR-8 reproduction or homogeneous placement-quality comparisons.

Use these for paper reproduction:

- `reproduction/traversal_yott.json`
- `reproduction/prisa_vpr8.json`

Use `probes/` for convergence studies and implementation diagnostics. Use `archive/` only for old exploratory manifests.
