# Placement Research Plan

Keep modulo mapping and 2D placement results separate.

## Modulo Mapping

Use this while designing algorithms that may use multiple contexts of the same physical PE:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/modulo/search.json
```

Use this when routed/context-aware ILP mappers should be included:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/modulo/all_mappers.json
```

## 2D Placement

Use this while designing algorithms where one physical PE is one placement slot:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/placement2d/search.json
```

This uses representative LISA/m_bench kernels, cpu_mapping-style grid sizing, perimeter I/O without corners, the 1-hop cost model, and placement-only metrics.

Use this when shared-engine mappers, VPR, and the placement-only ILP baseline should be included under the same II=1 setting:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/placement2d/all_mappers.json
```

## Metrics

Compare by benchmark first, then aggregate:

- `achieved_II`
- `mapping_time_sec`
- `compute_pe_utilization`
- `route_to_compute_ratio`
- `avg_manhattan_distance`
- `compute_bbox_utilization`
- `routing_validation.md`

For stochastic mappers, keep `random_seed` explicit. Increase `seed_count`, `routing_retry_count`, or `max_iterations` only when measuring stability or success rate.
