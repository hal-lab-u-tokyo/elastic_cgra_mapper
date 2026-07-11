# Metrics

The primary mapper quality metric is `achieved_II`, the first candidate II for which mapping succeeds. Lower is better, and `II_ratio = MII / achieved_II` is closer to 1 when the mapper reaches the lower bound.

`benchmark_set` identifies the benchmark collection that produced the row. It is empty or `default` for older single-set manifests.

`MII` is the computed lower bound, `max(ResMII, RecMII)`. `start_II` is the first II that the experiment actually tries, which can be larger than `MII` in a smoke test or a debugging run.

The primary runtime metric is `mapping_time_sec`, which comes from the mapper's own output log. `wall_time_sec` is also recorded by the Python runner and includes process startup, generated file I/O, and log collection. Standard summary reports show both; use `mapping_time_sec` for algorithm/runtime comparisons and `wall_time_sec` for end-to-end suite cost.

`status` is normalized into `optimal`, `timeout_feasible`, `timeout`, `infeasible`, `success`, or `failed`. With `Algorithm.accept_feasible_solution = true`, a Gurobi timeout with a feasible incumbent is counted as `timeout_feasible`.

`evaluation_mode` separates the meaning of success. `routing` requires a routed mapping. `placement_only` intentionally skips routing and should be used for TRAVERSAL/YOTT-style placement-quality comparisons.

Resource utilization metrics:

- `utilization` and `pe_context_utilization`: non-`nop` PE contexts divided by `rows * cols * context_size`. `utilization` is kept for compatibility.
- `compute_context_utilization`: non-`nop` and non-`route` operation contexts divided by all PE contexts.
- `route_context_utilization`: `route` contexts divided by all PE contexts.
- `compute_pe_utilization`: PEs that execute at least one non-`route` operation divided by total PEs.
- `pe_active_ratio`: PEs that execute either an operation or a route divided by total PEs.
- `context_active_ratio`: contexts with at least one non-`nop` configuration divided by `context_size`.

Routing pressure metrics:

- `route_ops`: number of contexts used as route operations.
- `route_to_compute_ratio`: `route_ops / compute_ops`, where `compute_ops` means mapped application operations, including constants and memory operations.
- `routing_overhead_ratio`: `route_ops / used_ops`, where `used_ops` includes operation and route contexts.
- `connection_count`: number of mapped outgoing configuration connections.
- `avg_manhattan_distance` and `max_manhattan_distance`: spatial PE distance over mapped outgoing connections.
- `avg_context_distance` and `max_context_distance`: modulo context distance over mapped outgoing connections.
- `max_fanout` and `avg_fanout`: outgoing connections per non-`nop` context.
- `routed_path_count` and `routed_unreachable_edge_count`: number of DFG edges for which a routed path is found or missing in the emitted mapping.
- `routed_avg_path_length` and `routed_max_path_length`: number of mapping connections on each recovered DFG-edge route path. A direct producer-to-consumer connection has length `1`.
- `routed_avg_fifo` and `routed_max_fifo`: actual routed FIFO proxy computed from recovered route paths as `max(0, routed_path_length - 1)` per DFG edge. Lower is better. Unlike `placement_avg_fifo`, this uses the emitted route path rather than only the physical placement distance.
- `routed_avg_spatial_hop` and `routed_max_spatial_hop`: spatial hop count accumulated along the recovered route path.
- `routed_mapped_lp`: longest DFG path after routing when each DFG edge is weighted by `max(1, routed_path_length)`.

Placement shape and balance metrics:

- `active_compute_pe_count`, `active_route_pe_count`, and `active_memory_pe_count`: number of PEs used by operation, route, and memory contexts.
- `max_context_load`, `min_context_load`, `avg_context_load`, and `context_load_stddev`: distribution of non-`nop` contexts over modulo time contexts.
- `max_compute_contexts_per_pe` and `avg_compute_contexts_per_active_compute_pe`: how densely application operations are packed onto active compute PEs.
- `compute_bbox_area`: area of the bounding box covering PEs with at least one operation context.
- `compute_bbox_utilization`: active compute PEs divided by `compute_bbox_area`; higher values indicate a more compact placement.

Placement-only quality metrics:

- `placement_avg_wirelength` and `placement_max_wirelength`: Manhattan distance between the placed endpoints of each DFG edge.
- `placement_direct_edge_ratio`: fraction of DFG edges whose endpoints are on the same PE or adjacent PEs.
- `placement_avg_fifo` and `placement_max_fifo`: placement FIFO count under physical mesh-Manhattan distance, computed as `max(0, mesh_hop - 1)` per DFG edge. Lower is better. Use this for mesh-style route-pressure analysis.
- `placement_p90_fifo` and `placement_p95_fifo`: 90th and 95th percentile placement FIFO values. These expose long-tail communication edges that can be hidden by the average.
- `placement_cost_model`: placement cost model used for paper-style edge quality. `mesh` uses Manhattan distance. `one_hop_axis2` uses `ceil(row_distance / 2) + ceil(column_distance / 2)`, matching the public TRAVERSAL/YOTT `cpu_mapping` 1-hop model.
- `placement_avg_cost` and `placement_max_cost`: edge placement cost under `placement_cost_model`. Use `placement_avg_cost` for YOTT-style one-hop wire-length comparisons. `placement_max_cost` is also exported because the paper VPR-BB maximal-FIFO values match this raw one-hop segment cost more closely than the cost-minus-one proxy in repository VPR BB runs.
- `placement_optimal_distance_ratio`: fraction of DFG edges whose mapped endpoints are exactly one physical mesh hop apart. Communication distance 1 is counted as optimal.
- `placement_optimal_edge_ratio`: fraction of DFG edges whose selected placement-cost value is optimal, counted as `placement_cost <= 1`. Use this for TRAVERSAL/YOTT one-hop CGRA comparisons, where one cost unit is one fully pipelined communication segment; this is not necessarily one physical mesh hop.
- `placement_avg_paper_fifo` and `placement_max_paper_fifo`: placement-only FIFO proxy computed as `max(0, placement_cost - 1)` under `placement_cost_model`. Keep this for cost-minus-one FIFO trends; compare it separately from raw `placement_max_cost` when checking YOTT VPR-BB paper values.
- `placement_avg_fifo_like` and `placement_max_fifo_like`: legacy aliases for `placement_avg_paper_fifo` and `placement_max_paper_fifo`.
- `placement_avg_mesh_hop` and `placement_max_mesh_hop`: Manhattan hop count on the physical 2D mesh, independent of `placement_cost_model`.
- `placement_mesh_optimal_edge_ratio`: fraction of DFG edges whose endpoints are on the same PE or adjacent mesh PEs under the physical mesh model.
- `placement_avg_mesh_fifo` and `placement_max_mesh_fifo`: aliases of the physical mesh placement FIFO, computed as `max(0, mesh_hop - 1)` per DFG edge.
- `placement_mapped_lp_mesh_hop`: longest DFG path after placement when each DFG edge is weighted by `max(1, mesh_hop)`. This is the paper-style mapped critical path proxy for placement-only runs.
- `placement_criticality_weighted_mesh_hop` and `placement_criticality_weighted_fifo`: placement hop/FIFO averaged with higher weight on DFG edges that lie closer to an unweighted critical path. These are empty for cyclic DFGs where this criticality proxy is not well-defined.
- `placement_max_critical_edge_mesh_hop` and `placement_max_critical_edge_fifo`: worst hop/FIFO among edges whose criticality reaches the critical-path maximum.
- `placement_max_cut_congestion`, `placement_avg_cut_congestion`, and `placement_p95_cut_congestion`: lower-bound routing pressure from counting how many DFG edges cross each horizontal or vertical mesh cut. High values indicate that many communications must pass through the same grid boundary regardless of the exact route.
- `placement_max_horizontal_cut_congestion` and `placement_max_vertical_cut_congestion`: directional cut-congestion maxima.
- `placement_estimated_total_link_demand`, `placement_estimated_max_link_demand`, `placement_estimated_avg_link_demand`, and `placement_estimated_p95_link_demand`: fast routeability proxy from routing each placement edge along one deterministic Manhattan XY path and counting per-link demand. This is not an actual routed result; use it to identify placements likely to stress BFS/Manhattan routing.
- `placement_estimated_used_link_ratio`: fraction of physical mesh links touched by the deterministic Manhattan demand proxy.
- `placement_swap_attempts`: placement move-attempt counter exported by placement mappers. VPR rows use VPR's reported placement swap attempts. YOTO/YOTT rows count evaluated node-cell placement moves, so this is the Fig. 20(b)-style effort counter rather than the legacy `cell_visits` debug value.
- `direct_dfg_edge_ratio`: fraction of DFG edges directly represented in the emitted mapping connections. In `placement_only` mode, prefer `placement_direct_edge_ratio` because routed paths are intentionally omitted.

Memory-related metrics:

- `memory_ops`: number of `load`, `store`, and `output` operation contexts.
- `memory_context_utilization`: `memory_ops / total_pe_contexts`.
- `memory_pe_ratio`: PEs that execute at least one memory operation divided by total PEs.

Solver diagnostics:

- `objective_value`, `best_bound`, and `mip_gap` are parsed from the Gurobi log when available. These values are useful for ILP debugging and timeout interpretation, but they are not direct architectural performance metrics.

Log and artifact lookup columns:

- `trial_dir`: directory for one candidate II attempt.
- `stdout_file` and `stderr_file`: captured process output for that candidate II.
- `arch_file`: generated CGRA JSON used for that candidate II.
- `raw_output_dir`: raw output directory passed to `build/mapping`.
- `run_dir`: concrete mapper run directory found under the raw output directory.
- `mapping_file`: emitted mapping JSON when mapping succeeds.
- `gurobi_log_file`: solver log path when a Gurobi-backed mapper writes one.

Routing correctness report:

- `routing_validation.md`: generated by `validate_mapping_routes.py`. It re-reads each successful `mapping_file` and the corresponding DFG, then checks that every DFG edge is reachable from the source operation to the destination operation through zero or more `route` configs.
- Rows with `evaluation_mode = placement_only` are skipped by `routing_validation.md` because they do not claim routed connectivity.
- The routing validation also checks that DFG operations appear exactly once, mapping connections stay inside the CGRA, each connection matches a legal MRRG edge including context transitions, and `to_config_id` / `from_config_id` entries agree.
- The report counts same-context and cross-context mapping connections and route paths.
- The report also summarizes recovered route-path length, routed FIFO, routed spatial hop, and routed mapped LP for route-correct successful rows.
- This is different from routing pressure metrics: `route_ops`, `route_to_compute_ratio`, and distance metrics describe routing cost, while `routing_validation.md` checks whether the emitted routes are structurally valid.
- For ILP-backed comparisons, prefer route-correct successful rows. `FullRoutingILPMapper` enforces DFG-edge-level routing flow internally, while placement-focused or heuristic mappers should still be checked with `routing_validation.md` because a fast placement can otherwise look successful without a valid route for every DFG edge.

Suite-level run-tracking files:

- `manifest.json`: manifest snapshot and active filters.
- `summary.json`: per benchmark/architecture/mapper summaries and tried II list.
- `run_metadata.json`: command, timestamps, git commit, dirty status, filters, and result directory.
- `run_info.md`: human-readable index for the run and log lookup conventions.

Metric rationale:

- Prior CGRA modulo mapping studies commonly report II/MII or throughput-related quality, mapping success, and compilation or placement-and-routing time.
- Placement-oriented studies often report PE utilization; some define PE utilization as occupied PE cycles divided by II.
- Routing-aware work separates useful operation placement from route-only resource consumption because routing PEs and interconnect pressure can lower effective utilization and increase II.
