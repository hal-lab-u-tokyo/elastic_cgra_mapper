# TRAVERSAL/YOTT Reproduction

The placement-only reproduction entry point is `research/configs/experiments/placement2d/traversal_yott_placement_quality.json`.

This manifest matches the public `cpu_mapping` setup used for the TRAVERSAL/YOTT-style benchmarks as closely as this mapper framework allows:

- `problem_type` is `placement2d`, so `II = 1`.
- `evaluation_mode` is `placement_only`; routed connectivity is intentionally not required.
- Grid size uses `ceil(sqrt(nodes - inputs - outputs)) + 2`, matching the public `cpu_mapping` code.
- I/O slots use `perimeter_no_corners`, matching the public `type_matrix` convention.
- `mesh_perimeter_io` uses the mesh cost model.
- `one_hop_perimeter_io` uses `one_hop_axis2`, matching the public 1-hop cost `ceil(dx / 2) + ceil(dy / 2)`.
- YOTO/YOTT trial counts are exposed as 1, 10, 100, and 1000.
- `array_yoto_*` and `array_yott_*` use direct 2D grid arrays instead of the shared placement engine. They use the same grid, I/O legality checks, output mapping format, random seed controls, and placement metrics, so they can be compared with the shared-engine mappers.
- The array YOTO/YOTT variants reserve perimeter I/O-capable slots for `load`, `store`, and `output` nodes by penalizing ordinary operations on those slots. This avoids greedy placement failures where compute nodes consume scarce I/O slots before memory/output nodes are placed.
- `placement_only_ilp` solves the same placement-only objective with Gurobi when it finishes within the manifest timeout.
- `vpr_sa` and `vpr_sa_fast` use VPR simulated annealing as external placement baselines with `pack_capacity = 1`, so each DFG node is kept as one placeable VPR block for fair placement-quality comparison.

The paper-facing placement metrics are:

- `placement_optimal_edge_ratio`: fraction of DFG edges whose placement cost is 1.
- `placement_avg_cost`: mean edge placement cost under the selected cost model.
- `placement_avg_fifo_like`: mean `max(0, placement_cost - 1)`.
- `placement_max_fifo_like`: maximum `max(0, placement_cost - 1)`.

For paper-style runtime comparison, use `mapping_time_sec`. This is the mapper-reported algorithm time. `wall_time_sec` is also shown in reports, but it includes the Python suite runner, process startup, generated input/output files, and log collection. That overhead is useful for workflow cost, but it should not be compared directly with the TRAVERSAL/YOTT placement kernel runtime.

`vpr_sa` runs VPR simulated-annealing placement with the bounding-box cost model. `vpr_sa_fast` runs a lower-effort VPR placement setting for a fast baseline. The repository pins VTR/VPR v8.0.0 as `third_party/vtr` for these baselines. If VPR is not built or the XML is not found, the row is recorded as `skipped` instead of failing the full suite.

VPR setup:

```bash
git submodule update --init --recursive third_party/vtr
sh scripts/build_vpr.sh
```

By default the manifest uses `third_party/vtr/build/vpr/vpr` and `third_party/vtr/vtr_flow/arch/timing/k6_N10_40nm.xml`. Set `VPR_BIN` or `VPR_ARCH_XML` to override them. When a VPR mapper config sets `pack_capacity = 1`, the runner derives a temporary N=1 architecture from the bundled VPR XML and disables buffer absorption, which avoids FPGA-style packing of multiple DFG nodes into one CLB. If `pack_capacity` is omitted, the runner uses the given architecture as-is and parses packed `.net` placements when possible; this mode is useful for VPR smoke checks but is not a strict one-op-per-site placement comparison.

The runner creates a simple BLIF from each DFG, runs VPR placement, parses the generated `.place` and `.net` files, and reports the same placement-quality metrics as the in-repository mappers. VPR option details differ by VPR/VTR version, so the manifest entry can override the default command with `vpr_args`.

Because this is a placement-only comparison, `routing_validation.md` skips all rows. Use this manifest for placement quality and runtime trends, not for routed CGRA correctness.

Typical command:

```bash
docker compose -f environment/docker-compose.yaml exec gurobi bash -lc '
cd /home/ubuntu/elastic_cgra_mapper &&
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/placement2d/traversal_yott_placement_quality.json
'
```

For a short check:

```bash
docker compose -f environment/docker-compose.yaml exec gurobi bash -lc '
cd /home/ubuntu/elastic_cgra_mapper &&
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/placement2d/traversal_yott_placement_quality.json \
  --only-benchmark atax \
  --only-mapper yoto_1,yott_1,vpr_sa \
  --tag repro_smoke
'
```
