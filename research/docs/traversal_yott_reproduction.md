# TRAVERSAL/YOTT Reproduction

The placement-only reproduction entry point is `research/configs/experiments/placement2d/reproduction/traversal_yott.json`.

This manifest matches the public `cpu_mapping` setup used for the TRAVERSAL/YOTT-style benchmarks as closely as this mapper framework allows:

- `problem_type` is `placement2d`, so `II = 1`.
- `evaluation_mode` is `placement_only`; routed connectivity is intentionally not required.
- Grid size uses `ceil(sqrt(nodes - inputs - outputs)) + 2`, matching the public `cpu_mapping` code.
- I/O slots use `perimeter_no_corners`, matching the public `type_matrix` convention.
- `mesh_perimeter_io` uses the mesh cost model.
- `one_hop_perimeter_io` uses `one_hop_axis2`, matching the public 1-hop cost `ceil(dx / 2) + ceil(dy / 2)`.
- YOTO/YOTT trial counts are exposed as 1, 10, 100, and 1000.
- `faithful_array_yoto_*` and `faithful_array_yott_*` use direct 2D grid storage while preserving the shared-engine YOTO/YOTT traversal order and, for YOTT, the I/O/reconvergence annotations, degree matching, and look-ahead rank.
- `cpu_mapping_yoto_1000` and `cpu_mapping_yott_1000` port the public `cpu_mapping` placement kernels. They use the public type matrix directly: corners are unavailable, perimeter non-corner slots are for inputs/outputs, and interior slots are for compute nodes.
- `placement2d_ilp` solves the same placement-only objective with Gurobi when it finishes within the manifest timeout.
- `vpr_sa` and `vpr_sa_fast` use VPR simulated annealing as external placement baselines with `pack_capacity = 1`, so each DFG node is kept as one placeable VPR block for fair placement-quality comparison.

Fidelity notes:

- The public `cpu_mapping` repository is a placement-and-routing codebase for YOTO, YOTT, and SA. Its executable fixes the random seed to `1706961029`, takes a trial count, and supports mesh and 1-hop architectures.
- TRAVERSAL/YOTO is a one-traversal placement-and-routing approach. The public YOTO path builds a traversal edge sequence, places nodes greedily near already placed neighbors, runs the local routing routine, and then selects the best successful trial.
- YOTT is a two-traversal placement approach. The paper's key additions over YOTO are I/O annotation, reconvergent annotation, degree matching, and look-ahead placement. The paper then treats routing as a small downstream step because most edges are expected to be adjacent; it describes maze routing for the remaining non-adjacent edges.
- The public YOTT source computes the annotated placement sequence, but its direct call to the YOTO/YOTT routing routine is commented out in the checked version. For paper-style comparison, use `evaluation_mode: "placement_only"` and the placement metrics below.
- `Placement2DYOTOMapper` and `Placement2DYOTTMapper` are the closest in-repository reproductions of the paper algorithms: they use the YOTO/YOTT traversal sequence, perimeter I/O legality when the architecture distinguishes I/O cells, Brandes betweenness centrality for YOTT's centrality traversal mode, reconvergent/I/O annotations, degree matching, and look-ahead scoring.
- `Placement2DFaithfulArrayYOTOMapper` and `Placement2DFaithfulArrayYOTTMapper` are array ports of the paper-faithful shared-engine mappers. Use them when checking whether the YOTO/YOTT quality trends can be preserved while reducing placement-kernel overhead.
- `Placement2DCPUMappingYOTOMapper` and `Placement2DCPUMappingYOTTMapper` are direct-grid ports of the public `cpu_mapping` placement kernels. They preserve the repository's output format and placement metrics while matching the public local-neighborhood placement behavior: type matrix, fixed adjacency offsets, grid-freedom update, YOTO's one-traversal local placement, and YOTT's tips/intersection/annotated-node candidate selection. They include safety checks for malformed inputs and use this repository's RNG, so they are not intended to be bit-identical to the original executable.

Paper-style placement metrics:

- `placement_optimal_distance_ratio`: fraction of DFG edges whose mapped endpoints are exactly one physical mesh hop apart.
- `placement_optimal_edge_ratio`: legacy cost-model counterpart, retained for older runs.
- `placement_avg_cost`: mean edge placement cost under the selected cost model.
- `placement_avg_paper_fifo`: mean FIFO proxy for the selected paper cost model, computed as `max(0, placement_cost - 1)`. For the TRAVERSAL/YOTT one-hop setup this is the Table-1-style FIFO quantity.
- `placement_avg_fifo`: mean physical mesh FIFO proxy, computed as `max(0, mesh_hop - 1)`. Use this for mesh route-pressure analysis, not for one-hop Table 1 normalization.
- `placement_max_fifo` / `placement_max_mesh_fifo`: maximum physical mesh FIFO proxy.

For paper-style runtime comparison, use `mapping_time_sec`. This is the mapper-reported algorithm time. `wall_time_sec` is also shown in reports, but it includes the Python suite runner, process startup, generated input/output files, and log collection. That overhead is useful for workflow cost, but it should not be compared directly with the TRAVERSAL/YOTT placement kernel runtime.

`vpr_sa` runs VPR simulated-annealing placement with the bounding-box cost model. `vpr_sa_fast` runs a lower-effort VPR placement setting for a fast baseline. The repository pins VTR/VPR v8.0.0 as `third_party/vtr` for these baselines. If VPR is not built or the XML is not found, the row is recorded as `skipped` instead of failing the full suite.

VPR setup:

```bash
git submodule update --init --recursive third_party/vtr
sh scripts/build_vpr.sh
```

By default the manifest uses `third_party/vtr/build/vpr/vpr` and `third_party/vtr/vtr_flow/arch/timing/k6_N10_40nm.xml`. Set `VPR_BIN` or `VPR_ARCH_XML` to override them. When a VPR mapper config sets `pack_capacity = 1`, the runner derives a temporary N=1 architecture from the bundled VPR XML. For the YOTT case-study VPR-BB baseline, `fixed_layout_to_arch = true` replaces VPR's auto layout with the same fixed CGRA grid used by the manifest. `protect_internal_buffer_luts = true` keeps internal one-input DFG nodes as placeable LUTs, while `output_nodes_as_io_pads = true` lets structural output nodes consume output I/O cells instead of CLB cells. If `pack_capacity` is omitted, the runner uses the given architecture as-is and parses packed `.net` placements when possible; this mode is useful for VPR smoke checks but is not a strict one-op-per-site placement comparison.

The BLIF representation cannot encode a multi-input DFG output operation as a single VPR output pad with all incoming DFG edges in the placement objective. In `output_nodes_as_io_pads` mode, VPR sees one representative input edge for such an output pad so that the resource model matches the paper-style structural I/O grid. The reported metrics are still computed from the original DFG edges and the final pad location.

The runner creates a simple BLIF from each DFG, runs VPR placement, parses the generated `.place` and `.net` files, and reports the same placement-quality metrics as the in-repository mappers. VPR option details differ by VPR/VTR version, so the manifest entry can override the default command with `vpr_args`.

Because this is a placement-only comparison, `routing_validation.md` skips all rows. Use this manifest for placement quality and runtime trends, not for routed CGRA correctness.

Typical command:

```bash
docker compose -f environment/docker-compose.yaml exec gurobi bash -lc '
cd /home/ubuntu/elastic_cgra_mapper &&
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/placement2d/reproduction/traversal_yott.json
'
```

For a short check:

```bash
docker compose -f environment/docker-compose.yaml exec gurobi bash -lc '
cd /home/ubuntu/elastic_cgra_mapper &&
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/placement2d/reproduction/traversal_yott.json \
  --only-benchmark atax \
  --only-mapper yoto_1,yott_1,vpr_sa \
  --tag repro_smoke
'
```
