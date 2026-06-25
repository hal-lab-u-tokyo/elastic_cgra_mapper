# Research

Manifests, mapper presets, architecture templates, benchmark normalization, metric extraction, validation, and report generation for mapper comparisons.

## Problem Settings

- **Modulo mapping**: `context_size` is the candidate II. Operations are placed on `(PE, context)` and DFG edges must be routed.
- **2D placement**: II is fixed to 1. Each operation occupies one physical PE and the run reports placement-quality metrics.

## Quick Start

Build the mapper:

```bash
sh scripts/build.sh
```

Build VPR when using VPR entries:

```bash
sh scripts/build_vpr.sh
```

Run a manifest:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/placement2d/search.json
```

Generate reports for an existing run:

```bash
python3 research/scripts/generate_reports.py \
  --result-dir <result_dir>
```

## Standard Manifests

| setting | manifest |
| --- | --- |
| Modulo mapper search | `configs/experiments/modulo/search.json` |
| Modulo comparison with ILP and VPR | `configs/experiments/modulo/all_mappers.json` |
| 2D placement search | `configs/experiments/placement2d/search.json` |
| 2D placement comparison with ILP | `configs/experiments/placement2d/all_mappers.json` |
| TRAVERSAL/YOTT paper comparison | `configs/experiments/placement2d/paper_comparison/traversal_yott.json` |
| YOTT 2021 paper benchmark comparison | `configs/experiments/placement2d/paper_comparison/yott_cases2021.json` |
| PRISA paper comparison | `configs/experiments/placement2d/paper_comparison/prisa_vpr8.json` |

Manifest schema: `configs/experiments/README.md`.

## Directory Layout

| path | responsibility |
| --- | --- |
| `configs/arch_templates/` | CGRA architecture templates |
| `configs/mapper/` | mapper JSON presets |
| `configs/experiments/` | experiment manifests grouped by problem setting |
| `scripts/` | runners, normalization, reporting, and validation |
| `docs/` | metric definitions, paper comparison notes, and extension guides |
| `results/` | generated outputs ignored by git |

## Adding a Mapper

1. Add the C++ implementation under `mapper/src/placement2d/` or `mapper/src/modulo/`.
2. Add the matching header under `mapper/include/mapper/placement2d/` or `mapper/include/mapper/modulo/`.
3. Derive from `mapper::IMapper`.
4. Register a unique `Algorithm.type` with the mapper registry.
5. Add one JSON preset under `research/configs/mapper/`.
6. Add the preset to one manifest under `research/configs/experiments/`.

For placement-first modulo methods, add the mapper to `mapper_matrix.placements` and choose a router under `mapper_matrix.routers`. For integrated routing-aware methods, add it to `routing_aware_mappers`. For 2D placement methods, add it to the manifest `mappers` list.

Detailed checklist: `docs/mapper_extension_guide.md`.

## Outputs

Each run creates a timestamped directory under the manifest's `result_group`. The main files are:

- `run_info.md`: command, manifest, output path, and timing.
- `run_metadata.json`: machine-readable run metadata.
- `metrics.csv`: normalized per-case metrics.
- `summary*.md` and `benchmark_report.md`: aggregate reports.
- `routing_validation.md`: route legality checks for modulo runs and routed placement checks when enabled.
- raw per-case logs and mapping files.

## Notes

- Use `parallel_num = 1` for algorithm comparisons. Larger values duplicate the input DFG and change the mapping problem.
- Use `mii: "auto"` when measuring achieved II from the computed MII.
- Use `research/scripts/normalize_benchmarks.py` before adding DOT/XML benchmarks that do not already use the mapper's opcode format.
- Set `VPR_BIN` or `VPR_ARCH_XML` to override the bundled VPR binary or architecture XML.
