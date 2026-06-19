# Research Workflow

This directory contains a small research layer on top of the existing `build/mapping` executable. It supports two intentionally separate problem settings: modulo-aware CGRA mapping, where `context_size` is treated as the candidate II and different contexts of one physical PE may hold different operations, and 2D placement, where a physical PE is occupied once any context at that PE is used.

## Quick Start

Build the project first:

```bash
sh scripts/build.sh
```

Build VPR only when running the external VPR placement baseline:

```bash
sh scripts/build_vpr.sh
```

Run a standard manifest:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/modulo/search.json
```

Generate the standard report set for any completed run:

```bash
python3 research/scripts/generate_reports.py \
  --result-dir <result_dir>
```

The generated files under `research/results/` are ignored by git except for layout markers such as `.gitkeep` and `research/results/README.md`.

## Directory Layout

- `configs/arch_templates/`: CGRA templates without `context_size`; the runner fills it with each candidate II.
- `configs/mapper/`: mapper configuration files. Modulo mapper implementations live under `mapper/src/modulo/`; 2D placement implementations live under `mapper/src/placement2d/`.
- `configs/experiments/`: experiment manifests grouped by problem type.
- `scripts/`: II sweep runner, MII calculator, metrics normalizer, and comparison helper.
- `docs/`: short notes on the problem setting, metrics, and extension points.

## Adding a Mapper

Add new C++ mapping algorithms under the directory that matches the problem setting: `mapper/include/mapper/modulo/` and `mapper/src/modulo/` for modulo mappers, or `mapper/include/mapper/placement2d/` and `mapper/src/placement2d/` for 2D placement mappers. Derive from `mapper::IMapper`, register the `Algorithm.type` string with `mapper::RegisterMapperType`, add a JSON config under `research/configs/mapper/`, then add that config to a manifest. Mapper `.cpp` files under `mapper/src/` are collected recursively by CMake.

Heuristic mapper configs can optionally set `max_trials`, `seed_count`, `routing_retry_count`, `random_seed`, and `max_iterations` under `Algorithm`. `max_trials` is the number of placement candidates tried per seed at one II, `seed_count` repeats the search with independent random seeds, `routing_retry_count` controls how many route-order attempts are tried for each placement, and `max_iterations` controls simulated annealing iterations.

The `modulo_*_mapper.json` configs solve modulo mapping and may use multiple contexts of one physical PE. The `placement2d_*_mapper.json` configs solve 2D placement with II/context size fixed to 1. Successful rows are checked by `routing_validation.md`.

See `research/docs/mapper_extension_guide.md` for the exact checklist.

See `research/docs/vpr_modulo_routing.md` for the VPR routed baseline scope and limitations.

## Mapper Comparison

Use these four manifests:

- `configs/experiments/modulo/search.json`: modulo heuristic mappers plus the lightweight VPR SA routed baseline.
- `configs/experiments/modulo/all_mappers.json`: modulo mappers plus routed/context-aware ILP mappers and strict VPR routed baselines.
- `configs/experiments/placement2d/search.json`: 2D placement mappers only; quick iteration.
- `configs/experiments/placement2d/all_mappers.json`: 2D placement mappers plus the placement-only ILP baseline.

`modulo/search.json` uses one 6x6 default CGRA and a small native kernel set for routed modulo mapping. It includes the internal heuristic mappers, `modulo_sa_mapper`, and `vpr_sa_routed`, which runs VPR simulated-annealing placement first, assigns modulo contexts, then routes on this repository's MRRG. The slower `vpr_sa_full_route` baseline is kept in `modulo/all_mappers.json`. `placement2d/search.json` uses a representative LISA/m_bench subset, cpu_mapping-style auto grid sizing, perimeter I/O without corners, the 1-hop cost model, and placement-only evaluation.

To compare a new mapper, add or remove entries in the manifest's `mappers` list, then run:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/modulo/search.json
```

Use `configs/experiments/modulo/all_mappers.json` for small modulo runs that include ILP mappers:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/modulo/all_mappers.json
```

This manifest uses small kernels and a longer per-II timeout. `FullRoutingILPMapper` solves placement and DFG-edge routing together. `ConnectivityPathILPMapper` uses precomputed MRRG paths. `PlacementOnlyILPMapper` is a placement-first exact baseline and is implemented with the 2D placement mappers because it does not model routed paths.

`vpr_sa_full_route` is the strict VPR-routing baseline for modulo runs. It routes over a generated CGRA RR graph, imports the VPR route tree, and accepts a result only when the imported mapping passes the same reachability and legal-edge checks as native mappers. It is intended for `modulo/all_mappers.json`, not the lightweight search manifest.

Use `configs/experiments/placement2d/search.json` for quick 2D placement comparisons. It includes cpu_mapping-style YOTO/YOTT 1000-trial baselines, PRISA, PRISA without SIS, and VPR simulated-annealing placement baselines:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/placement2d/search.json
```

Use `configs/experiments/placement2d/all_mappers.json` to add the placement-only ILP baseline under the same II=1 setting. It uses a representative subset that avoids very large ILP timeouts. For placement2d manifests, one physical PE is one placement slot and II/context size is fixed to 1. `preflight_manifest.py` checks that DFG node count is not larger than physical PE count.

Use `configs/experiments/placement2d/reproduction/traversal_yott.json` for TRAVERSAL/YOTT-style placement-only reproduction, and `configs/experiments/placement2d/reproduction/prisa_vpr8.json` for PRISA VPR-8 reproduction. These manifests include VPR external baselines, which use `third_party/vtr/build/vpr/vpr` and `third_party/vtr/vtr_flow/arch/timing/k6_N10_40nm.xml` by default. Set `VPR_BIN` or `VPR_ARCH_XML` to override them. The VPR mapper entries set `pack_capacity: 1`, so the runner derives a temporary N=1 VPR architecture and keeps one DFG node per placement site for strict placement-quality comparison.

If `--out` is omitted, `run_suite.py` creates a timestamped result directory under the manifest's `result_group`. It prints one progress line after each benchmark/architecture/mapper condition. Reports are generated automatically unless `--skip-reports` is passed. `routing_validation.md` checks DFG-edge reachability, legal MRRG edges, reciprocal connections, and same-context/cross-context route counts.

## Standard Assumptions

For ordinary modulo CGRA mapping comparisons, use `parallel_num = 1`. The current `build/mapping` executable duplicates the input DFG when `parallel_num > 1`, which is useful for throughput-style experiments but changes the mapping problem for algorithm comparisons.

The MII helper computes ResMII and RecMII. RecMII uses explicit edge distance attributes when present, and by default treats missing-distance self-loop edges as distance-1 recurrences for compatibility with the current benchmark files.

The smoke manifest starts from a fixed II so logs and metrics are generated quickly. Use `mii: "auto"` when measuring achieved II from the computed lower bound.

Manifests can define multiple benchmark sets:

```json
"benchmark_sets": [
  {
    "name": "kernel",
    "benchmark_root": "/home/ubuntu/elastic_cgra_mapper/benchmark/kernel",
    "benchmarks": ["convolution_no_loop", "fixed_fir_pro"]
  },
  {
    "name": "converted_cgrabench",
    "benchmark_root": "/home/ubuntu/elastic_cgra_mapper/benchmark/kernel",
    "benchmarks": ["fft_pro"]
  }
]
```

The aggregate `metrics.csv` includes `benchmark_set`, and `report_by_benchmark.py` breaks results down by set, benchmark, mapper, and architecture.

Raw GenMap and CGRA-Bench `.dot` files are not always directly compatible with the current mapper input format because some use uppercase opcodes, `type` fields, or display `label`s instead of lowercase `opcode` fields. Convert or normalize them before adding them to final manifests.
