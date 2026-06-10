# Metrics

The primary mapper quality metric is `achieved_II`, the first candidate II for which mapping succeeds. Lower is better, and `II_ratio = MII / achieved_II` is closer to 1 when the mapper reaches the lower bound.

`benchmark_set` identifies the benchmark collection that produced the row. It is empty or `default` for older single-set manifests.

`MII` is the computed lower bound, `max(ResMII, RecMII)`. `start_II` is the first II that the experiment actually tries, which can be larger than `MII` in a smoke test or a debugging run.

The primary runtime metric is `mapping_time_sec`, which comes from the mapper's own output log. `wall_time_sec` is also recorded by the Python runner and includes process overhead.

`status` is normalized into `optimal`, `timeout_feasible`, `timeout`, `infeasible`, `success`, or `failed`. With `Algorithm.accept_feasible_solution = true`, a Gurobi timeout with a feasible incumbent is counted as `timeout_feasible`.

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

Placement shape and balance metrics:

- `active_compute_pe_count`, `active_route_pe_count`, and `active_memory_pe_count`: number of PEs used by operation, route, and memory contexts.
- `max_context_load`, `min_context_load`, `avg_context_load`, and `context_load_stddev`: distribution of non-`nop` contexts over modulo time contexts.
- `max_compute_contexts_per_pe` and `avg_compute_contexts_per_active_compute_pe`: how densely application operations are packed onto active compute PEs.
- `compute_bbox_area`: area of the bounding box covering PEs with at least one operation context.
- `compute_bbox_utilization`: active compute PEs divided by `compute_bbox_area`; higher values indicate a more compact placement.

Memory-related metrics:

- `memory_ops`: number of `load`, `store`, and `output` operation contexts.
- `memory_context_utilization`: `memory_ops / total_pe_contexts`.
- `memory_pe_ratio`: PEs that execute at least one memory operation divided by total PEs.

Solver diagnostics:

- `objective_value`, `best_bound`, and `mip_gap` are parsed from the Gurobi log when available. These values are useful for ILP debugging and timeout interpretation, but they are not direct architectural performance metrics.

Metric rationale:

- CGRA modulo mapping papers commonly report II/MII or throughput-related quality, mapping success, and compilation or placement-and-routing time.
- Placement-oriented work often reports PE utilization; some papers explicitly define PE utilization as occupied PE cycles divided by II.
- Routing-aware work separates useful operation placement from route-only resource consumption because routing PEs and interconnect pressure can lower effective utilization and increase II.
