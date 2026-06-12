# Experiment Manifests

Use these manifests for ordinary experiments:

- `modulo/search.json`: modulo mappers only; quick iteration.
- `modulo/all_mappers.json`: modulo mappers plus routed/context-aware ILP mappers.
- `placement2d/search.json`: paper-like 2D placement subset with fast array mappers and VPR.
- `placement2d/all_mappers.json`: the same paper-like setting with shared-engine mappers, array mappers, VPR, and placement-only ILP.
- `placement2d/traversal_yott.json`: TRAVERSAL/YOTT-style DOT benchmarks, 100 trials.
- `placement2d/traversal_yott_1000_trials.json`: same benchmarks, 1000 trials for YOTO/YOTT.
- `placement2d/traversal_yott_placement_quality.json`: placement-only TRAVERSAL/YOTT comparison using the imported LISA/m_bench DOTs, cpu_mapping-style grid sizing, border I/O without corners, mesh and 1-hop cost models, YOTO/YOTT trial counts from the paper tables, array fast-path mappers, placement-only ILP, and VPR external baselines.

See `research/docs/traversal_yott_reproduction.md` for the reproduction scope, metrics, and command examples.

Problem type rules:

- `problem_type: "modulo"`: II is swept from MII to `ii_max`; use `Modulo*`, `FullRoutingILPMapper`, and `ConnectivityPathILPMapper` configs.
- `problem_type: "placement2d"`: II/context size is fixed to 1; use `Placement2D*` and `PlacementOnlyILPMapper` configs.
- `evaluation_mode: "routing"`: a successful result must include valid routing.
- `evaluation_mode: "placement_only"`: routing is skipped; compare direct edge ratio, wire length, and FIFO-like distance. Routing validation reports these rows as skipped because they do not claim routed paths.
- `runner: "vpr"` in a mapper entry: run VPR as an external placement-only baseline. Run `scripts/build_vpr.sh`, or set `VPR_BIN` and `VPR_ARCH_XML`; missing VPR is recorded as `skipped`. Set `pack_capacity: 1` for a strict one-DFG-node-per-site comparison.

Architecture rules:

- Set `memory_io: "all"` to allow memory/I/O operations on every PE.
- Set `memory_io: "perimeter"` to allow memory/I/O operations only on the outer border.
- Set `memory_io: "perimeter_no_corners"` to match the TRAVERSAL/YOTT cpu_mapping convention: border I/O slots, excluding the four corners.
- Existing `memory_io: "both_ends"` and `memory_io: "one_end"` restrict memory/I/O to the left/right columns or left column.
- Set `network_type: "one_hop_axis2"` for the TRAVERSAL/YOTT 1-hop model, where a placement edge cost is `ceil(row_distance / 2) + ceil(column_distance / 2)`.

Before a run:

1. Add or remove entries in `mappers`.
2. Run `research/scripts/preflight_manifest.py`.
3. Run `research/scripts/run_suite.py --manifest <manifest>`.

For new algorithms, choose the manifest that matches the problem type and keep the benchmark, architecture, timeout, and metric settings unchanged while comparing mappers.
